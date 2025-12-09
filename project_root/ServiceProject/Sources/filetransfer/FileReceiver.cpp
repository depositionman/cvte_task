#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <thread>
#include <fstream>
#include <cstring>
#include <iostream>
#include <mutex>
#include <memory>
#include "FileTransfer.h"
#include "FileReceiver.h"
#include "ThreadPool.h"
#include "MemoryPool.h"
#include <libgen.h>  
#include <cstdlib>

// 线程池实例
static ThreadPool* receiver_thread_pool = nullptr;

// 内存池实例 - 用于控制服务器端内存使用
static std::unique_ptr<MemoryPool> server_memory_pool = nullptr;

// 内存使用限制
static const size_t MAX_SERVER_MEMORY_BYTES = 100 * 1024 * 1024; // 100MB限制
static std::atomic<size_t> current_memory_usage{0};
static std::mutex memory_mutex_;
static std::condition_variable memory_cv_;

// 文件合并器 - 使用内存池管理文件块数据
struct FileAssembler {
    std::vector<MemoryBlock*> chunks;  // 使用内存池块指针
    int total_chunks = 0;
    int fileLength = 0;
    std::string filename;
    mode_t fileMode;
    std::string userid;
    int received = 0;
    std::mutex mutex_;

    void add_chunk(const FileChunk& chunk) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (chunks.empty()) {
            total_chunks = chunk.totalChunks;
            fileLength = chunk.fileLength;
            filename = chunk.fileName;
            fileMode = chunk.fileMode;
            userid = chunk.userid;
            chunks.resize(total_chunks, nullptr);
        }
        
        // 从内存池分配块来存储文件数据
        if (server_memory_pool) {
            MemoryBlock* block = server_memory_pool->allocate(userid);
            if (block) {
                // 复制文件数据到内存池块
                memcpy(block->data, chunk.data, chunk.chunkLength);
                block->size = chunk.chunkLength;
                block->fileIndex = chunk.fileIndex;
                block->totalChunks = chunk.totalChunks;
                strncpy(block->fileName, chunk.fileName, sizeof(block->fileName) - 1);
                block->fileName[sizeof(block->fileName) - 1] = '\0';
                block->fileLength = chunk.fileLength;
                block->fileMode = chunk.fileMode;
                
                chunks[chunk.fileIndex] = block;
                received++;
                
                // 更新内存使用计数
                current_memory_usage += chunk.chunkLength;
            }
        }
    }

    bool complete() const {
        return received == total_chunks;
    }

    void save(const std::string& outdir) {
        // 提取文件名
        char* filename_cstr = strdup(filename.c_str());  
        char* base_filename = basename(filename_cstr);  
        std::string outpath = outdir + "/" + base_filename;  
        free(filename_cstr);  

        // 直接保存到指定文件夹
        std::ofstream ofs(outpath, std::ios::binary);
        if (!ofs.is_open()) {
            std::cerr << "Failed to open file for writing: " << outpath << std::endl;
            return;
        }
        
        // 写入所有分片数据
        for (const auto& block : chunks) {
            if (block && block->data) {
                ofs.write(block->data, block->size);
            }
        }
        ofs.close();
        chmod(outpath.c_str(), fileMode); 
        std::cout << "Saved: " << outpath << " (mode: " << std::oct << fileMode << ")" << std::endl;
        
        // 释放内存池块
        cleanup();
    }
    
    void cleanup() {
        for (auto& block : chunks) {
            if (block && server_memory_pool) {
                // 更新内存使用计数
                current_memory_usage -= block->size;
                server_memory_pool->deallocate(block);
            }
        }
        chunks.clear();
        received = 0;
    }
    
    ~FileAssembler() {
        cleanup();
    }
};

// 文件组装器映射
static std::map<std::string, FileAssembler> file_assemblers;
static std::mutex assemblers_mutex;

// 初始化文件接收器
int init_file_receiver(size_t thread_count, size_t memory_pool_blocks) {
    if (receiver_thread_pool != nullptr) {
        std::cerr << "File receiver already initialized." << std::endl;
        return -1;
    }
    
    try {
        // 创建线程池
        receiver_thread_pool = new ThreadPool(thread_count);
        
        // 创建内存池 - 用于流量控制和内存管理
        server_memory_pool = std::make_unique<MemoryPool>(1024, memory_pool_blocks);
        
        std::cout << "File receiver initialized with " << thread_count 
                  << " threads and " << memory_pool_blocks << " memory blocks." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize file receiver: " << e.what() << std::endl;
        return -1;
    }
}

// 清理文件接收器资源
int cleanup_file_receiver() {
    if (receiver_thread_pool != nullptr) {
        delete receiver_thread_pool;
        receiver_thread_pool = nullptr;
        
        // 清理内存池
        server_memory_pool.reset();
        
        // 清理所有未完成的文件组装器
        std::lock_guard<std::mutex> lock(assemblers_mutex);
        file_assemblers.clear();
        
        // 重置内存使用计数
        current_memory_usage = 0;
        
        std::cout << "File receiver cleanup completed." << std::endl;
        return 0;
    }
    return -1;
}

// 处理接收到的文件块
void process_file_chunk(const FileChunk& chunk, const std::string& outdir) {
    // 内存使用控制：检查是否超过服务器内存限制
    if (current_memory_usage + chunk.chunkLength > MAX_SERVER_MEMORY_BYTES) {
        std::unique_lock<std::mutex> lock(memory_mutex_);
        
        // 等待内存释放
        memory_cv_.wait(lock, [&chunk]() {
            return current_memory_usage + chunk.chunkLength <= MAX_SERVER_MEMORY_BYTES;
        });
    }
    
    // 检查内存池是否可用
    if (!server_memory_pool) {
        std::cerr << "Memory pool not available for file chunk processing." << std::endl;
        return;
    }
    
    std::string key = std::string(chunk.fileName) + "_" + chunk.userid;
    
    // 添加到文件组装器
    {   
        std::lock_guard<std::mutex> lock(assemblers_mutex);
        file_assemblers[key].add_chunk(chunk);
    }
    
    // 检查是否完成
    FileAssembler* assembler;
    bool is_complete;
    {   
        std::lock_guard<std::mutex> lock(assemblers_mutex);
        assembler = &file_assemblers[key];
        is_complete = assembler->complete();
    }
    
    // 如果文件组装完成，保存文件并清理资源
    if (is_complete) {
        assembler->save(outdir);
        
        // 通知等待的线程内存已释放
        memory_cv_.notify_all();
        
        // 从映射中移除完成的组装器
        std::lock_guard<std::mutex> lock(assemblers_mutex);
        file_assemblers.erase(key);
    }
}

// 接收文件块并添加到线程池处理
int receive_file_chunk(const FileChunk& chunk, const std::string& outdir) {
    if (receiver_thread_pool == nullptr) {
        std::cerr << "File receiver not initialized. Call init_file_receiver first." << std::endl;
        return -1;
    }
    
    // 将文件块处理任务添加到线程池
    receiver_thread_pool->enqueue(process_file_chunk, chunk, outdir);
    return 0;
}

// 获取线程池大小
size_t get_receiver_thread_pool_size() {
    if (receiver_thread_pool != nullptr) {
        return receiver_thread_pool->get_thread_count();
    }
    return 0;
}

// 获取当前内存池状态
void get_memory_pool_status() {
    if (server_memory_pool) {
        size_t total_blocks, used_blocks;
        server_memory_pool->get_status(total_blocks, used_blocks);
        std::cout << "Memory Pool Status - Total: " << total_blocks 
                  << ", Used: " << used_blocks 
                  << ", Available: " << (total_blocks - used_blocks) << std::endl;
    } else {
        std::cout << "Memory pool not initialized." << std::endl;
    }
}

// 获取当前内存使用情况
void get_memory_usage() {
    std::cout << "Current memory usage: " << current_memory_usage << " bytes (" 
              << (current_memory_usage * 100.0 / MAX_SERVER_MEMORY_BYTES) << "% of limit)" << std::endl;
}