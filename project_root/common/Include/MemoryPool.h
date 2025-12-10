#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <string>
#include "FileTransfer.h"  // 包含文件传输配置宏

// 内存块结构
struct MemoryBlock {
    char* data;           // 数据体指针 (1KB)
    char client_id[20];  // 客户端标识头
    bool is_used;        // 是否被使用
    size_t size;         // 实际数据大小
    int fileIndex;       // 文件块索引
    int totalChunks;     // 总块数
    char fileName[256];  // 文件名
    int fileLength;      // 文件总长度
    mode_t fileMode;     // 文件权限

    MemoryBlock();
    ~MemoryBlock();
};

// 内存池类
class MemoryPool {
private:
    std::vector<MemoryBlock*> blocks_;  // 内存块列表
    mutable std::mutex mutex_;          // 互斥锁（mutable允许在const函数中使用）
    std::condition_variable cv_;        // 条件变量
    size_t block_size_;                 // 每个块的数据体大小 (1KB)
    size_t total_blocks_;               // 总块数
    size_t used_blocks_;                // 已使用块数

public:
    /**
     * @brief 构造函数
     * @param block_size 每个块的数据体大小（默认1KB）
     * @param initial_blocks 初始块数量
     */
    explicit MemoryPool(size_t block_size = 1024, size_t initial_blocks = 100);

    /**
     * @brief 析构函数
     */
    ~MemoryPool();

    /**
     * @brief 分配一个内存块
     * @param client_id 客户端标识
     * @return 指向MemoryBlock的指针，失败返回nullptr
     */
    MemoryBlock* allocate(const std::string& client_id);

    /**
     * @brief 释放一个内存块
     * @param block 要释放的内存块指针
     */
    void deallocate(MemoryBlock* block);

    /**
     * @brief 获取内存池状态信息
     * @param total_blocks 总块数
     * @param used_blocks 已使用块数
     */
    void get_status(size_t& total_blocks, size_t& used_blocks) const;

    /**
     * @brief 获取数据体大小
     * @return 数据体大小（字节）
     */
    size_t get_block_data_size() const { return block_size_; }

private:
    /**
     * @brief 扩展内存池
     * @param num_blocks 要扩展的块数
     */
    void expand(size_t num_blocks);
};