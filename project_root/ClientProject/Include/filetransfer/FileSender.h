#pragma once
#include <sys/types.h>
#include <string>
#include "ThreadPool.h"

// 前向声明
class ClientDBus;

// 初始化文件发送器（创建内存池和线程池）
bool init_file_sender(size_t thread_pool_size = 0);

// 设置DBus客户端实例
void set_dbus_client(ClientDBus* dbus_client);

// 清理文件发送器（释放内存池和线程池）
void cleanup_file_sender();

// 发送文件
void send_file(const std::string& filepath, const std::string& userid, mode_t mode, const std::string& transferId = "");

// 发送文件夹
void send_folder(const std::string& folder, const std::string& userid, mode_t mode, const std::string& transferId = "");

// 发送单个条目（文件或文件夹）
void send_entry(const std::string& path, const std::string& userid, mode_t mode, const std::string& transferId = "");

// 获取内存池状态
void get_memory_pool_status(size_t& total_blocks, size_t& used_blocks);

// 获取线程池信息
size_t get_thread_pool_size();
