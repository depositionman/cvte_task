#include "ThreadPool.h"
#include <iostream>
#include <thread>

// 构造函数
ThreadPool::ThreadPool(size_t thread_count) : stop_(false) {
    // 默认使用CPU核心数
    if (thread_count == 0) {
        thread_count_ = std::thread::hardware_concurrency();
        if (thread_count_ == 0) {
            thread_count_ = 4; // 最小4个线程
        }
    } else {
        thread_count_ = thread_count;
    }

    // 创建线程
    threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back(&ThreadPool::worker, this);
    }

    std::cout << "[ThreadPool] 初始化完成，线程数: " << thread_count_ << std::endl;
}

// 析构函数
ThreadPool::~ThreadPool() {
    stop_ = true;
    cv_.notify_all(); // 通知所有线程

    // 等待所有线程结束
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "[ThreadPool] 已销毁，线程数: " << thread_count_ << std::endl;
}

// 线程工作函数
void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;

        { // 加锁作用域
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待任务或停止信号
            cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

            // 如果线程池已停止且任务队列为空，退出线程
            if (stop_ && tasks_.empty()) {
                return;
            }

            // 获取任务
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        // 执行任务
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] 任务执行异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ThreadPool] 任务执行未知异常" << std::endl;
        }
    }
}

// 获取任务队列大小
size_t ThreadPool::get_task_queue_size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}
