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
#include "FileTransfer.h"
#include "FileReceiver.h"
#include "ThreadPool.h"
#include <libgen.h>  
#include <cstdlib>

// 线程池实例
static ThreadPool* receiver_thread_pool = nullptr;

// 文件合并器
struct FileAssembler {
    std::vector<std::vector<char>> chunks;
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
            chunks.resize(total_chunks);
        }
        chunks[chunk.fileIndex].assign(chunk.data, chunk.data + chunk.chunkLength);
        received++;
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
        for (const auto& chunk : chunks) {
            ofs.write(chunk.data(), chunk.size());
        }
        ofs.close();
        chmod(outpath.c_str(), fileMode); 
        std::cout << "Saved: " << outpath << " (mode: " << std::oct << fileMode << ")" << std::endl;
    }
};

// 文件组装器映射
static std::map<std::string, FileAssembler> file_assemblers;
static std::mutex assemblers_mutex;

// 初始化文件接收器
int init_file_receiver(size_t thread_count) {
    if (receiver_thread_pool != nullptr) {
        std::cerr << "File receiver already initialized." << std::endl;
        return -1;
    }
    
    try {
        receiver_thread_pool = new ThreadPool(thread_count);
        std::cout << "File receiver initialized with " << thread_count << " threads." << std::endl;
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
        
        // 清理所有未完成的文件组装器
        std::lock_guard<std::mutex> lock(assemblers_mutex);
        file_assemblers.clear();
        
        std::cout << "File receiver cleanup completed." << std::endl;
        return 0;
    }
    return -1;
}

// 处理接收到的文件块
void process_file_chunk(const FileChunk& chunk, const std::string& outdir) {
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