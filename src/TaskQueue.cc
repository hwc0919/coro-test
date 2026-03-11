/**
 * @file TaskQueue.cc
 * @brief Default thread-pool TaskQueue implementation
 */
#include <nitrocoro/utils/TaskQueue.h>

#include <algorithm>
#include <thread>

namespace nitrocoro
{

ThreadPool::ThreadPool(size_t threadNum)
{
    if (threadNum == 0)
        threadNum = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);

    for (size_t i = 0; i < threadNum; ++i)
        workers_.emplace_back([this] { workerThread(); });
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();

    for (auto & w : workers_)
        if (w.joinable())
            w.join();
}

void ThreadPool::post(std::function<void()> task)
{
    {
        std::lock_guard lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerThread()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_)
                return;

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

TaskQueueProvider & defaultTaskQueueProvider()
{
    static TaskQueueProvider provider = [] {
        static auto instance = std::make_shared<ThreadPool>();
        return instance;
    };
    return provider;
}

} // namespace nitrocoro
