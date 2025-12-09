#include "MemoryPool.h"
#include <iostream>
#include <cstring>

// MemoryBlock 构造函数
MemoryBlock::MemoryBlock() : data(nullptr), is_used(false), size(0) {
    // 分配1KB的数据体
    data = new char[1024];
    if (data) {
        memset(data, 0, 1024);
        memset(client_id, 0, sizeof(client_id));
    }
}

// MemoryBlock 析构函数
MemoryBlock::~MemoryBlock() {
    if (data) {
        delete[] data;
        data = nullptr;
    }
}

// MemoryPool 构造函数
MemoryPool::MemoryPool(size_t block_size, size_t initial_blocks) 
    : block_size_(block_size), total_blocks_(0), used_blocks_(0) {
    // 确保块大小至少为1
    if (block_size_ == 0) {
        block_size_ = 1024;
    }

    // 初始化内存池
    expand(initial_blocks);
    std::cout << "[MemoryPool] 初始化完成，块大小: " << block_size_ 
              << " 初始块数: " << initial_blocks << std::endl;
}

// MemoryPool 析构函数
MemoryPool::~MemoryPool() {
    // 释放所有内存块
    for (auto block : blocks_) {
        delete block;
    }
    blocks_.clear();
    std::cout << "[MemoryPool] 已销毁，释放了 " << total_blocks_ << " 个块" << std::endl;
}

// 分配内存块
MemoryBlock* MemoryPool::allocate(const std::string& client_id) {
    std::unique_lock<std::mutex> lock(mutex_);

    // 查找空闲块
    for (auto block : blocks_) {
        if (!block->is_used) {
            block->is_used = true;
            block->size = 0;
            strncpy(block->client_id, client_id.c_str(), sizeof(block->client_id) - 1);
            block->client_id[sizeof(block->client_id) - 1] = '\0';
            memset(block->data, 0, block_size_);
            used_blocks_++;
            return block;
        }
    }

    // 没有空闲块，扩展内存池
    expand(50); // 每次扩展50个块

    // 返回新分配的块
    auto new_block = blocks_.back();
    new_block->is_used = true;
    new_block->size = 0;
    strncpy(new_block->client_id, client_id.c_str(), sizeof(new_block->client_id) - 1);
    new_block->client_id[sizeof(new_block->client_id) - 1] = '\0';
    used_blocks_++;
    return new_block;
}

// 释放内存块
void MemoryPool::deallocate(MemoryBlock* block) {
    if (!block) return;

    std::unique_lock<std::mutex> lock(mutex_);

    if (block->is_used) {
        block->is_used = false;
        block->size = 0;
        memset(block->client_id, 0, sizeof(block->client_id));
        memset(block->data, 0, block_size_);
        used_blocks_--;
        cv_.notify_one();
    }
}

// 获取内存池状态
void MemoryPool::get_status(size_t& total_blocks, size_t& used_blocks) const {
    std::unique_lock<std::mutex> lock(mutex_);
    total_blocks = total_blocks_;
    used_blocks = used_blocks_;
}

// 扩展内存池
void MemoryPool::expand(size_t num_blocks) {
    for (size_t i = 0; i < num_blocks; ++i) {
        MemoryBlock* block = new MemoryBlock();
        if (block) {
            blocks_.push_back(block);
            total_blocks_++;
        } else {
            std::cerr << "[MemoryPool] 扩展失败: 无法分配新块" << std::endl;
            break;
        }
    }
    std::cout << "[MemoryPool] 已扩展，新增 " << num_blocks << " 个块，总块数: " << total_blocks_ << std::endl;
}
