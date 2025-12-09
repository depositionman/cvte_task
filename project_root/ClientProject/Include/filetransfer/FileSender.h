#pragma once
#include <sys/types.h>
#include <string>
#include "MemoryPool.h"
#include "ThreadPool.h"

// 初始化文件发送器（创建内存池和线程池）
bool init_file_sender(size_t memory_pool_blocks = 100, size_t thread_pool_size = 0);

// 清理文件发送器（释放内存池和线程池）
void cleanup_file_sender();

// 发送文件
void send_file(const char* filepath, const char* userid, mode_t mode);

// 发送文件夹
void send_folder(const char* folder, const char* userid, mode_t mode);

// 发送单个条目（文件或文件夹）
void send_entry(const char* path, const char* userid, mode_t mode);

// 获取内存池状态
void get_memory_pool_status(size_t& total_blocks, size_t& used_blocks);

// 获取线程池信息
size_t get_thread_pool_size();
