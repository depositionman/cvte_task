#pragma once
#include <string>
#include <sys/types.h>

// 初始化文件接收器（创建线程池）
int init_file_receiver(size_t thread_pool_size = 0, size_t memory_pool_blocks = 100);

// 清理文件接收器（释放线程池）
int cleanup_file_receiver();

// 接收单个文件块
int receive_file_chunk(const struct FileChunk& chunk, const std::string& outdir);

// 处理文件块的线程函数
void process_file_chunk(const struct FileChunk& chunk, const std::string& outdir);

// 获取线程池大小
size_t get_receiver_thread_pool_size();
