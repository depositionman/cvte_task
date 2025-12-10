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
#include "ThreadPool.h"
#include "ClientDBus.h"
#include <unordered_map>
#include <condition_variable>

// 全局实例
static std::unique_ptr<ThreadPool> thread_pool_;
static ClientDBus* dbus_client_ = nullptr;

// 文件描述符限制管理
static const int MAX_CONCURRENT_FILES = 100; // 最大并发文件数
static std::atomic<int> current_concurrent_files{0};
static std::mutex fd_mutex_;
static std::condition_variable fd_cv_;

// 错误输出同步
static std::mutex error_mutex_;

// 进度跟踪结构
struct ProgressTracker {
    int total_chunks{0};
    std::string filename;
    std::chrono::steady_clock::time_point start_time;
};

static std::mutex progress_mutex_;
static std::unordered_map<std::string, ProgressTracker> progress_trackers_;
static std::unordered_map<std::string, std::atomic<int>> progress_counters_;

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

// 全局连接等待机制
std::mutex connection_mutex_;
std::condition_variable connection_cv_;
std::atomic<bool> connection_available_{true};

// 等待连接恢复
void wait_for_connection() {
    if (!dbus_client_) return;
    
    std::unique_lock<std::mutex> lock(connection_mutex_);
    while (!dbus_client_->is_connected()) {
        std::cout << "[FileSender] 连接断开，等待重连..." << std::endl;
        connection_available_ = false;
        
        // 等待连接恢复，最多等待30秒
        if (connection_cv_.wait_for(lock, std::chrono::seconds(30), 
            []{ return dbus_client_ && dbus_client_->is_connected(); })) {
            std::cout << "[FileSender] 连接已恢复，继续发送..." << std::endl;
            connection_available_ = true;
            break;
        } else {
            std::cout << "[FileSender] 等待连接超时，继续重试..." << std::endl;
        }
    }
}

// 发送单个文件块到服务端
void send_file_chunk(const FileChunk& chunk) {
    // 调用DBus客户端发送文件块
    if (dbus_client_) {
        // 等待连接可用
        wait_for_connection();
        
        // 尝试发送文件块，如果失败则等待重连
        int max_retries = 10; 
        int retry_count = 0;
        
        while (retry_count < max_retries) {
            if (dbus_client_->SendFileChunk(chunk)) {
                // 发送成功
                break;
            } else {
                // 发送失败，等待重连
                retry_count++;
                
                // 等待2秒后重试
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        
        if (retry_count >= max_retries) {
            std::cerr << "[FileSender] 发送文件块失败，已达到最大重试次数" << std::endl;
        }
    } else {
        std::cerr << "[FileSender] DBus客户端未初始化，无法发送文件块" << std::endl;
    }
}

// 处理文件块的线程函数
void process_file_chunk(const std::string& filepath, off_t offset, int chunk_index, int total_chunks, 
                       const std::string& userid, mode_t mode, int file_length, const std::string& transferId) {
    // 每个线程打开自己的文件描述符，避免竞争条件
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            std::cerr << "[FileSender] 无法打开文件: " << filepath << std::endl;
        }
        return;
    }

    // 直接使用FileChunk结构体，避免不必要的内存池中转
    FileChunk chunk{}; 
    
    // 设置文件块信息
    chunk.fileIndex = chunk_index;
    chunk.totalChunks = total_chunks;
    chunk.fileLength = file_length;
    chunk.fileMode = mode;
    chunk.isLastChunk = (chunk_index == total_chunks - 1);
    
    // 安全复制字符串
    strncpy(chunk.userid, userid.c_str(), sizeof(chunk.userid) - 1);
    chunk.userid[sizeof(chunk.userid) - 1] = '\0';
    strncpy(chunk.fileName, filepath.c_str(), sizeof(chunk.fileName) - 1);
    chunk.fileName[sizeof(chunk.fileName) - 1] = '\0';
    
    // 设置传输ID
    if (!transferId.empty()) {
        strncpy(chunk.transferId, transferId.c_str(), sizeof(chunk.transferId) - 1);
        chunk.transferId[sizeof(chunk.transferId) - 1] = '\0';
    } else {
        chunk.transferId[0] = '\0'; // 清空传输ID
    }

    // std::cout << "[FileSender] 发送文件块: " << chunk.fileName
    //           << ", 索引: " << chunk.fileIndex
    //           << ", 大小: " << chunk.chunkLength
    //           << ", 传输ID: " << (chunk.transferId[0] ? chunk.transferId : "无") << std::endl;

    // 直接读取文件数据到chunk.data
    lseek(fd, offset, SEEK_SET);
    int read_len = read(fd, chunk.data, sizeof(chunk.data));
    if (read_len < 0) {
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            std::cerr << "[FileSender] 文件读取失败: " << filepath << std::endl;
        }
        close(fd);
        return;
    }
    chunk.chunkLength = read_len;

    // 发送文件块
    send_file_chunk(chunk);
    
    // 关闭文件描述符
    close(fd);
    
    // 更新进度
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        auto counter_it = progress_counters_.find(filepath);
        if (counter_it != progress_counters_.end()) {
            int completed = ++counter_it->second;
            
            // 每10个块或最后一个块显示进度
            if (completed % 10 == 0 || completed == total_chunks) {
                auto tracker_it = progress_trackers_.find(filepath);
                if (tracker_it != progress_trackers_.end()) {
                    show_progress(filepath, completed, tracker_it->second.total_chunks);
                }
            }
        }
    }
}

// 设置DBus客户端实例
void set_dbus_client(ClientDBus* dbus_client) {
    dbus_client_ = dbus_client;
    if (dbus_client_) {
        std::cout << "[FileSender] DBus客户端已设置，连接状态: " 
                  << (dbus_client_->is_connected() ? "已连接" : "未连接") << std::endl;
    } else {
        std::cout << "[FileSender] DBus客户端已清除" << std::endl;
    }
}

// 初始化文件发送器
bool init_file_sender(size_t thread_pool_size) {
    try {
        // 创建线程池
        thread_pool_ = std::make_unique<ThreadPool>(thread_pool_size);
        
        // 不再创建新的DBus客户端实例，而是使用主程序已经创建的实例
        // 文件发送器将通过外部传入的DBus客户端实例进行通信
        std::cout << "[FileSender] 文件发送器初始化完成，等待外部DBus客户端连接" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FileSender] 初始化异常: " << e.what() << std::endl;
        return false;
    }
}

// 清理文件发送器
void cleanup_file_sender() {
    thread_pool_.reset();
    // 不清理外部传入的DBus客户端实例，只重置指针
    dbus_client_ = nullptr;
    std::cout << "[FileSender] 清理完成" << std::endl;
}

void send_file(const char* filepath, const char* userid, mode_t mode, const char* transferId) {
    if (!thread_pool_) {
        std::cerr << "[FileSender] 未初始化" << std::endl;
        return;
    }

    // 检查并发文件数限制
    {
        std::unique_lock<std::mutex> lock(fd_mutex_);
        fd_cv_.wait(lock, []{ 
            return current_concurrent_files.load() < MAX_CONCURRENT_FILES; 
        });
        current_concurrent_files++;
    }

    // 获取文件信息
    struct stat st;
    if (stat(filepath, &st) < 0) {
        std::cerr << "[FileSender] 无法获取文件信息: " << filepath << std::endl;
        {
            std::lock_guard<std::mutex> lock(fd_mutex_);
            current_concurrent_files--;
            fd_cv_.notify_one();
        }
        return;
    }
    
    off_t file_length = st.st_size;
    int total_chunks = (file_length + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE; // FILE_CHUNK_SIZE per chunk

    std::cout << "[FileSender] 开始发送文件: " << filepath 
              << " 大小: " << file_length << " 字节" 
              << " 总块数: " << total_chunks 
              << " 传输ID: " << (transferId && transferId[0] ? transferId : "无") << std::endl;

    // 初始化进度跟踪器
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        ProgressTracker tracker;
        tracker.total_chunks = total_chunks;
        tracker.filename = filepath;
        tracker.start_time = std::chrono::steady_clock::now();
        progress_trackers_[filepath] = tracker;
        progress_counters_[filepath] = 0;
    }

    // 使用线程池并发发送所有文件块
    std::vector<std::future<void>> futures;
    futures.reserve(total_chunks);

    for (int i = 0; i < total_chunks; ++i) {
        off_t offset = i * FILE_CHUNK_SIZE;
        
        // 使用线程池提交任务
        auto future = thread_pool_->enqueue(process_file_chunk, 
                                           std::string(filepath), 
                                           offset, 
                                           i, 
                                           total_chunks, 
                                           std::string(userid), 
                                           mode, 
                                           static_cast<int>(file_length),
                                           std::string(transferId ? transferId : ""));
        futures.push_back(std::move(future));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.get();
    }

    // 清理进度跟踪器
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        auto tracker_it = progress_trackers_.find(filepath);
        auto counter_it = progress_counters_.find(filepath);
        
        if (tracker_it != progress_trackers_.end() && counter_it != progress_counters_.end()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - tracker_it->second.start_time);
            std::cout << "[FileSender] 文件发送完成: " << filepath 
                      << " 耗时: " << duration.count() << "ms" << std::endl;
            progress_trackers_.erase(tracker_it);
            progress_counters_.erase(counter_it);
        }
    }
    
    // 释放并发文件计数
    {
        std::lock_guard<std::mutex> lock(fd_mutex_);
        current_concurrent_files--;
        fd_cv_.notify_one();
    }
}

void send_folder(const char* folder, const char* userid, mode_t mode, const char* transferId) {
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
            send_folder(fullpath.c_str(), userid, mode, transferId);
        } else {
            // 使用线程池发送文件
            thread_pool_->enqueue(send_file, fullpath.c_str(), userid, mode, transferId);
        }
    }
    closedir(dir);
}

void send_entry(const char* path, const char* userid, mode_t mode, const char* transferId) {
    struct stat st;
    if (stat(path, &st) < 0) {
        std::cerr << "[FileSender] 无法获取文件信息: " << path << std::endl;
        return;
    }
    
    if (S_ISDIR(st.st_mode)) {
        send_folder(path, userid, mode, transferId);
    } else {
        send_file(path, userid, mode, transferId);
    }
}

// 获取线程池大小
size_t get_thread_pool_size() {
    if (thread_pool_) {
        return thread_pool_->get_thread_count();
    }
    return 0;
}
