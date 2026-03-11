/**
 * @file TaskQueue.h
 * @brief Abstract task queue interface and default thread-pool implementation
 */
#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nitrocoro
{

class TaskQueue
{
public:
    virtual ~TaskQueue() = default;

    TaskQueue(const TaskQueue &) = delete;
    TaskQueue & operator=(const TaskQueue &) = delete;

    virtual void post(std::function<void()> task) = 0;

protected:
    TaskQueue() = default;
};

class ThreadPool final : public TaskQueue
{
public:
    explicit ThreadPool(size_t threadNum = 0);
    ~ThreadPool() override;

    void post(std::function<void()> task) override;

private:
    void workerThread();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

using TaskQueueProvider = std::function<std::shared_ptr<TaskQueue>()>;

TaskQueueProvider & defaultTaskQueueProvider();

} // namespace nitrocoro
