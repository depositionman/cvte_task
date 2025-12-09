#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

// 线程池类
class ThreadPool {
private:
    std::vector<std::thread> threads_;       // 线程列表
    std::queue<std::function<void()>> tasks_; // 任务队列
    mutable std::mutex mutex_;               // 互斥锁（mutable允许在const函数中使用）
    std::condition_variable cv_;             // 条件变量
    std::atomic<bool> stop_;                 // 停止标志
    size_t thread_count_;                    // 线程数量

public:
    /**
     * @brief 构造函数
     * @param thread_count 线程数量（默认使用CPU核心数）
     */
    explicit ThreadPool(size_t thread_count = 0);

    /**
     * @brief 析构函数
     */
    ~ThreadPool();

    /**
     * @brief 添加任务到线程池
     * @tparam F 任务函数类型
     * @tparam Args 任务函数参数类型
     * @param f 任务函数
     * @param args 任务函数参数
     * @return 任务执行结果的future
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    /**
     * @brief 获取线程数量
     * @return 线程数量
     */
    size_t get_thread_count() const { return thread_count_; }

    /**
     * @brief 获取当前队列中的任务数量
     * @return 任务数量
     */
    size_t get_task_queue_size() const;

private:
    /**
     * @brief 线程工作函数
     */
    void worker();
};

// 模板函数实现
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    // 创建一个shared_ptr包装的packaged_task
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    { // 加锁作用域
        std::unique_lock<std::mutex> lock(mutex_);

        // 检查线程池是否已停止
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // 添加任务到队列
        tasks_.emplace([task]() { (*task)(); });
    }

    // 通知一个等待的线程
    cv_.notify_one();

    return res;
}