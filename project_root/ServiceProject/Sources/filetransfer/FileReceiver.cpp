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
#include <chrono>

// 线程池实例
static ThreadPool* receiver_thread_pool = nullptr;

// 内存池实例 - 用于控制服务器端内存使用
static std::unique_ptr<MemoryPool> server_memory_pool = nullptr;

// 内存使用限制
static const size_t MAX_SERVER_MEMORY_BYTES = 100 * 1024 * 1024; // 100MB限制
static std::atomic<size_t> current_memory_usage{0};
static std::mutex memory_mutex_;
static std::condition_variable memory_cv_;

// 文件传输状态映射 - 使用传输ID作为键
static std::map<std::string, TransferStatus> file_transfer_states;
static std::mutex transfer_states_mutex;

// 传输状态持久化存储文件路径
static const std::string TRANSFER_STATUS_FILE = "./transfer_status.dat";

// GDBus缓冲区实现 - 内存中的文件块缓存
struct FileChunkCache {
    std::vector<uint8_t> data;
    size_t chunkIndex;
    std::chrono::steady_clock::time_point timestamp;
};

// 文件块数据存储映射 - 使用传输ID作为键
static std::map<std::string, std::map<int, FileChunkCache>> file_chunk_storage;
static std::mutex chunk_storage_mutex;

bool assemble_and_save_file(const std::string& transferId, const std::string& fileName, const std::string& outdir, const TransferStatus& status);

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
        server_memory_pool = std::make_unique<MemoryPool>(FILE_CHUNK_SIZE, memory_pool_blocks);
        
        // std::cout << "File receiver initialized with " << thread_count 
        //           << " threads and " << memory_pool_blocks << " memory blocks." << std::endl;
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
        
        std::cout << "File receiver cleaned up successfully." << std::endl;
        return 0;
    }
    
    return -1;
}

// 处理接收到的文件块
void process_file_chunk(const FileChunk& chunk, const std::string& outdir) {
    // 调试信息：打印接收到的文件块信息
    // std::cout << "[FileReceiver] 接收到文件块: " << std::endl;
    // std::cout << "  transferId: " << chunk.transferId << std::endl;
    // std::cout << "  userid: " << chunk.userid << std::endl;
    // std::cout << "  fileName: " << chunk.fileName << std::endl;
    // std::cout << "  fileIndex: " << chunk.fileIndex << std::endl;
    // std::cout << "  totalChunks: " << chunk.totalChunks << std::endl;
    // std::cout << "  fileLength: " << chunk.fileLength << std::endl;
    // std::cout << "  chunkLength: " << chunk.chunkLength << std::endl;
    // std::cout << "  isLastChunk: " << std::boolalpha << chunk.isLastChunk << std::endl;
    
    // 内存使用控制：检查是否超过服务器内存限制
    if (current_memory_usage + chunk.chunkLength > MAX_SERVER_MEMORY_BYTES) {
        std::unique_lock<std::mutex> lock(memory_mutex_);
        
        // 等待内存释放
        memory_cv_.wait(lock, [&chunk]() {
            return current_memory_usage + chunk.chunkLength <= MAX_SERVER_MEMORY_BYTES;
        });
    }
    
    // 更新内存使用量
    current_memory_usage += chunk.chunkLength;
    
    // 检查内存池是否可用
    if (!server_memory_pool) {
        std::cerr << "Memory pool not available for file chunk processing." << std::endl;
        current_memory_usage -= chunk.chunkLength; // 回滚内存使用量
        return;
    }
    
    // 使用传输ID作为键，支持断点续传
    std::string key = std::string(chunk.transferId);

    // std::cout << "[process_file_chunk] key = " << key << std::endl;
    
    // 存储文件块数据
    {
        std::lock_guard<std::mutex> lock(chunk_storage_mutex);
        FileChunkCache cache;
        cache.chunkIndex = chunk.fileIndex;
        cache.data.assign(chunk.data, chunk.data + chunk.chunkLength);
        cache.timestamp = std::chrono::steady_clock::now();
        file_chunk_storage[key][chunk.fileIndex] = cache;
    }
    
    // 添加到文件传输状态
    {
        std::lock_guard<std::mutex> lock(transfer_states_mutex);
        
        auto it = file_transfer_states.find(key);
        if (it == file_transfer_states.end()) {
            // 新传输，初始化TransferStatus
            file_transfer_states[key] = TransferStatus(chunk.totalChunks, chunk.fileLength);
        }
        
        // 标记块已接收
        file_transfer_states[key].markChunkReceived(chunk.fileIndex, chunk.chunkLength);
    }
    
    // 检查是否完成
    bool is_complete;
    {   
        std::lock_guard<std::mutex> lock(transfer_states_mutex);
        auto it = file_transfer_states.find(key);
        is_complete = (it != file_transfer_states.end() && it->second.isCompleted);
    }
    
    // 如果文件组装完成，保存文件并清理资源
    if (is_complete) {
        std::lock_guard<std::mutex> lock(transfer_states_mutex);
        auto it = file_transfer_states.find(key);
        if (it != file_transfer_states.end()) {
            TransferStatus& final_status = it->second;
            
            if (final_status.isCompleted) {
                std::cout << "[FileReceiver] 文件传输完成: " << key 
                          << " (" << final_status.receivedChunks << "/" << final_status.totalChunks << ")" << std::endl;
                
                // 组装并保存文件
                if (assemble_and_save_file(key, std::string(chunk.fileName), outdir, final_status)) {
                    std::cout << "[FileReceiver] 文件保存成功: " << chunk.fileName << std::endl;
                } else {
                    std::cerr << "[FileReceiver] 文件保存失败: " << chunk.fileName << std::endl;
                }
                
                // 从映射中移除完成的传输状态
                file_transfer_states.erase(key);
                
                // 释放内存并通知等待的线程
                current_memory_usage -= chunk.chunkLength;
                memory_cv_.notify_all();
            } else {
                std::vector<int> missing = final_status.getMissingChunks();
                std::cout << "[FileReceiver] 传输 " << key << " 缺失块数: " << missing.size() << std::endl;
                
                // 处理未完成的情况也要释放内存
                current_memory_usage -= chunk.chunkLength;
                memory_cv_.notify_all();
            }
        }
    } else {
        // 如果传输未完成，也需要在适当的时候释放内存
        // 这里我们假设在缓存处理完成后释放内存
        current_memory_usage -= chunk.chunkLength;
        memory_cv_.notify_all();
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

// 获取传输状态（包含位图信息）
TransferStatus get_transfer_status(const std::string& transferId, [[maybe_unused]] const std::string& userid, [[maybe_unused]] const std::string& fileName) {
    std::lock_guard<std::mutex> lock(transfer_states_mutex);
    
    auto it = file_transfer_states.find(transferId);
    if (it != file_transfer_states.end()) {
        return it->second;
    }
    
    // 如果找不到传输记录，返回默认状态
    TransferStatus status;
    status.statusCode = 2; 
    return status;
}

// 获取缺失的块索引列表
std::vector<int> get_missing_chunks(const std::string& transferId, [[maybe_unused]] const std::string& userid, [[maybe_unused]] const std::string& fileName) {
    std::lock_guard<std::mutex> lock(transfer_states_mutex);
    
    auto it = file_transfer_states.find(transferId);
    if (it != file_transfer_states.end()) {
        return it->second.getMissingChunks();
    }
    
    return {};
}

// 获取线程池大小
size_t get_receiver_thread_pool_size() {
    if (receiver_thread_pool == nullptr) {
        return 0;
    }
    return receiver_thread_pool->get_thread_count();
}

// 组装并保存文件
bool assemble_and_save_file(const std::string& transferId, const std::string& fileName, const std::string& outdir, const TransferStatus& status) {
    std::lock_guard<std::mutex> lock(chunk_storage_mutex);
    
    auto it = file_chunk_storage.find(transferId);
    if (it == file_chunk_storage.end()) {
        std::cerr << "[assemble_and_save_file] 未找到传输ID对应的文件块数据: " << transferId << std::endl;
        return false;
    }
    
    // 检查是否所有块都已接收
    if (status.receivedChunks != status.totalChunks) {
        std::cerr << "[assemble_and_save_file] 文件块不完整: " << status.receivedChunks << "/" << status.totalChunks << std::endl;
        return false;
    }
    
    // 处理文件名：从完整路径中提取文件名
    std::string actualFileName = fileName;
    size_t lastSlash = fileName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        actualFileName = fileName.substr(lastSlash + 1);
    }
    
    // 创建输出路径
    std::string outputPath;
    if (outdir == ".") {
        outputPath = actualFileName; // 当前目录
    } else {
        outputPath = outdir + "/" + actualFileName;
    }
    
    std::cout << "[assemble_and_save_file] 保存文件路径: " << outputPath << std::endl;
    
    // 打开文件进行写入
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "[assemble_and_save_file] 无法打开文件进行写入: " << outputPath << std::endl;
        return false;
    }
    
    // 按顺序组装文件块
    size_t totalWritten = 0;
    for (int i = 0; i < status.totalChunks; ++i) {
        auto chunkIt = it->second.find(i);
        if (chunkIt == it->second.end()) {
            std::cerr << "[assemble_and_save_file] 缺失文件块: " << i << std::endl;
            outputFile.close();
            return false;
        }
        
        const FileChunkCache& cache = chunkIt->second;
        outputFile.write(reinterpret_cast<const char*>(cache.data.data()), cache.data.size());
        totalWritten += cache.data.size();
        
        if (!outputFile.good()) {
            std::cerr << "[assemble_and_save_file] 写入文件块失败: " << i << std::endl;
            outputFile.close();
            return false;
        }
    }
    
    outputFile.close();
    
    // 验证文件大小
    if (totalWritten != static_cast<size_t>(status.fileLength)) {
        std::cerr << "[assemble_and_save_file] 文件大小不匹配: 期望=" << status.fileLength 
                  << ", 实际=" << totalWritten << std::endl;
        return false;
    }
    
    // 清理存储的文件块数据
    file_chunk_storage.erase(it);
    
    std::cout << "[assemble_and_save_file] 文件组装完成: " << fileName 
              << " (" << totalWritten << " 字节)" << std::endl;
    return true;
}
