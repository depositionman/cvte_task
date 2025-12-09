#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>
#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <iomanip>
#include "FileTransfer.h"
#include "FileSender.h"
#include "MemoryPool.h"
#include "ThreadPool.h"
#include "ClientDBus.h"

// 全局实例
static std::unique_ptr<MemoryPool> memory_pool_;
static std::unique_ptr<ThreadPool> thread_pool_;
static std::unique_ptr<ClientDBus> dbus_client_;

// 进度跟踪结构
struct ProgressTracker {
    std::atomic<int> completed_chunks{0};
    int total_chunks{0};
    std::string filename;
    std::chrono::steady_clock::time_point start_time;
};

static std::mutex progress_mutex_;
static std::unordered_map<std::string, ProgressTracker> progress_trackers_;

// 发送单个文件块到服务端
void send_file_chunk(const FileChunk& chunk) {
    // 这里将使用fdbus发送文件块
    // 目前只是打印信息，后续需要实现fdbus通信
    std::cout << "[FileSender] 发送文件块: " << chunk.fileName 
              << " 块索引: " << chunk.fileIndex 
              << " 块大小: " << chunk.chunkLength << std::endl;
    
    // 调用DBus客户端发送文件块
    if (dbus_client_) {
        dbus_client_->SendFileChunk(chunk);
    } else {
        std::cerr << "[FileSender] DBus客户端未初始化，无法发送文件块" << std::endl;
    }
}

// 显示进度条
void show_progress(const std::string& filepath, int completed, int total) {
    const int bar_width = 50;
    float progress = static_cast<float>(completed) / total;
    int pos = static_cast<int>(bar_width * progress);
    
    std::cout << "\r[FileSender] " << filepath << " [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% "
              << completed << "/" << total << " 块";
    std::cout.flush();
    
    if (completed >= total) {
        std::cout << std::endl;
    }
}

// 处理文件块的线程函数
void process_file_chunk(const std::string& filepath, off_t offset, int chunk_index, int total_chunks, 
                       const std::string& userid, mode_t mode, int file_length) {
    // 每个线程打开自己的文件描述符，避免竞争条件
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[FileSender] 无法打开文件: " << filepath << std::endl;
        return;
    }

    // 从内存池分配块
    MemoryBlock* block = memory_pool_->allocate(userid);
    if (!block) {
        std::cerr << "[FileSender] 内存池分配失败" << std::endl;
        close(fd);
        return;
    }

    // 读取文件数据到内存块
    lseek(fd, offset, SEEK_SET);
    int read_len = read(fd, block->data, memory_pool_->get_block_data_size());
    if (read_len < 0) {
        std::cerr << "[FileSender] 文件读取失败: " << filepath << std::endl;
        memory_pool_->deallocate(block);
        close(fd);
        return;
    }

    block->size = read_len;
    block->fileIndex = chunk_index;
    block->totalChunks = total_chunks;
    strncpy(block->fileName, filepath.c_str(), sizeof(block->fileName) - 1);
    block->fileName[sizeof(block->fileName) - 1] = '\0';
    block->fileLength = file_length;
    block->fileMode = mode;

    // 创建FileChunk结构用于发送
    FileChunk chunk{}; // 使用花括号初始化，避免memset破坏可能的std::string成员
    memcpy(chunk.data, block->data, read_len);
    strncpy(chunk.userid, userid.c_str(), sizeof(chunk.userid) - 1);
    strncpy(chunk.fileName, filepath.c_str(), sizeof(chunk.fileName) - 1);
    chunk.fileIndex = chunk_index;
    chunk.totalChunks = total_chunks;
    chunk.chunkLength = read_len;
    chunk.fileLength = file_length;
    chunk.fileMode = mode;
    chunk.isLastChunk = (chunk_index == total_chunks - 1);

    // 发送文件块
    send_file_chunk(chunk);

    // 释放内存块
    memory_pool_->deallocate(block);
    
    // 关闭文件描述符
    close(fd);
    
    // 更新进度
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        auto it = progress_trackers_.find(filepath);
        if (it != progress_trackers_.end()) {
            int completed = ++it->second.completed_chunks;
            if (completed % 10 == 0 || completed == total_chunks) {
                show_progress(filepath, completed, total_chunks);
            }
        }
    }
}

// 初始化文件发送器
bool init_file_sender(size_t memory_pool_blocks, size_t thread_pool_size) {
    try {
        // 创建内存池
        memory_pool_ = std::make_unique<MemoryPool>(1024, memory_pool_blocks);
        
        // 创建线程池
        thread_pool_ = std::make_unique<ThreadPool>(thread_pool_size);
        
        // 初始化fdbus客户端
        dbus_client_ = std::make_unique<ClientDBus>();
        if (!dbus_client_->init()) {
            std::cerr << "[FileSender] FDBus客户端初始化失败" << std::endl;
            return false;
        }
        
        std::cout << "[FileSender] 初始化完成" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FileSender] 初始化异常: " << e.what() << std::endl;
        return false;
    }
}

// 清理文件发送器
void cleanup_file_sender() {
    memory_pool_.reset();
    thread_pool_.reset();
    dbus_client_.reset();
    std::cout << "[FileSender] 清理完成" << std::endl;
}

void send_file(const char* filepath, const char* userid, mode_t mode) {
    if (!memory_pool_ || !thread_pool_) {
        std::cerr << "[FileSender] 未初始化" << std::endl;
        return;
    }

    // 获取文件信息
    struct stat st;
    if (stat(filepath, &st) < 0) {
        std::cerr << "[FileSender] 无法获取文件信息: " << filepath << std::endl;
        return;
    }
    
    off_t file_length = st.st_size;
    int total_chunks = (file_length + 1023) / 1024; // 1KB per chunk

    std::cout << "[FileSender] 开始发送文件: " << filepath 
              << " 大小: " << file_length << " 字节" 
              << " 总块数: " << total_chunks << std::endl;

    // 使用线程池并发发送所有文件块
    std::vector<std::future<void>> futures;
    futures.reserve(total_chunks);

    for (int i = 0; i < total_chunks; ++i) {
        off_t offset = i * 1024;
        
        // 使用线程池提交任务
        auto future = thread_pool_->enqueue(process_file_chunk, 
                                           std::string(filepath), 
                                           offset, 
                                           i, 
                                           total_chunks, 
                                           std::string(userid), 
                                           mode, 
                                           static_cast<int>(file_length));
        futures.push_back(std::move(future));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.get(); // 等待任务完成
    }

    std::cout << "[FileSender] 文件发送完成: " << filepath << std::endl;
}

void send_folder(const char* folder, const char* userid, mode_t mode) {
    DIR* dir = opendir(folder);
    if (!dir) {
        std::cerr << "[FileSender] 无法打开文件夹: " << folder << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        std::string fullpath = std::string(folder) + "/" + entry->d_name;
        struct stat st;
        if (stat(fullpath.c_str(), &st) < 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            send_folder(fullpath.c_str(), userid, mode);
        } else {
            // 使用线程池发送文件
            thread_pool_->enqueue(send_file, fullpath.c_str(), userid, mode);
        }
    }
    closedir(dir);
}

void send_entry(const char* path, const char* userid, mode_t mode) {
    struct stat st;
    if (stat(path, &st) < 0) {
        std::cerr << "[FileSender] 无法获取文件信息: " << path << std::endl;
        return;
    }
    
    if (S_ISDIR(st.st_mode)) {
        send_folder(path, userid, mode);
    } else {
        send_file(path, userid, mode);
    }
}

// 获取内存池状态
void get_memory_pool_status(size_t& total_blocks, size_t& used_blocks) {
    if (memory_pool_) {
        memory_pool_->get_status(total_blocks, used_blocks);
    } else {
        total_blocks = 0;
        used_blocks = 0;
    }
}

// 获取线程池大小
size_t get_thread_pool_size() {
    if (thread_pool_) {
        return thread_pool_->get_thread_count();
    }
    return 0;
}
