/**
 * @file Pipe.h
 * @brief Multi-producer / single-consumer async pipe for coroutine communication.
 *
 * Usage:
 *   auto [tx, rx] = makePipe<std::string>(scheduler);
 *   tx->send("hello");           // from any thread; RAII close on last Sender destruction
 *   auto v = co_await rx->recv(); // suspends if empty; returns nullopt on EOF
 */
#pragma once

#include <nitrocoro/core/MpscQueue.h>
#include <nitrocoro/core/Scheduler.h>

#include <atomic>
#include <coroutine>
#include <memory>
#include <optional>
#include <utility>

namespace nitrocoro
{

template <typename T>
class PipeReceiver;

template <typename T>
class PipeSender;

namespace detail
{

template <typename T>
struct PipeState
{
    explicit PipeState(Scheduler * scheduler)
        : scheduler_(scheduler)
    {
    }

    // Called by the last PipeSender destructor
    void close()
    {
        closed_.store(true, std::memory_order_release);
        wakeReceiver();
    }

    // Returns false if already closed
    bool send(T value)
    {
        if (closed_.load(std::memory_order_acquire))
            return false;
        queue_.push(std::move(value));
        wakeReceiver();
        return true;
    }

    // Async recv awaitable
    struct [[nodiscard]] RecvAwaiter
    {
        PipeState & state_;

        bool await_ready() noexcept
        {
            return !state_.queue_.empty() || state_.closed_.load(std::memory_order_acquire);
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            state_.waiter_.store(h, std::memory_order_release);
            // Re-check to avoid missing a send/close that raced before store
            if (!state_.queue_.empty() || state_.closed_.load(std::memory_order_acquire))
            {
                // CAS: if a producer already took h, it will schedule it — must return true
                auto expected = h;
                if (state_.waiter_.compare_exchange_strong(expected, nullptr,
                        std::memory_order_acq_rel))
                    return false; // we cleared it ourselves, safe to resume inline
                return true;     // a producer took h and will schedule it
            }
            return true;
        }

        std::optional<T> await_resume() noexcept
        {
            return state_.queue_.pop();
        }
    };

    RecvAwaiter recv() { return RecvAwaiter{ *this }; }

private:
    void wakeReceiver()
    {
        if (auto h = waiter_.exchange(nullptr, std::memory_order_acq_rel))
            scheduler_->schedule(h);
    }

    Scheduler * scheduler_;
    MpscQueue<T> queue_;
    std::atomic<bool> closed_{ false };
    std::atomic<std::coroutine_handle<>> waiter_{ nullptr };
};

} // namespace detail

template <typename T>
class PipeSender
{
public:
    explicit PipeSender(std::shared_ptr<detail::PipeState<T>> state)
        : state_(std::move(state))
    {
    }

    ~PipeSender() { state_->close(); }

    PipeSender(const PipeSender &) = delete;
    PipeSender & operator=(const PipeSender &) = delete;
    PipeSender(PipeSender &&) = delete;
    PipeSender & operator=(PipeSender &&) = delete;

    // Returns false if the receiver is gone (pipe closed)
    bool send(T value) { return state_->send(std::move(value)); }

private:
    std::shared_ptr<detail::PipeState<T>> state_;
};

template <typename T>
class PipeReceiver
{
public:
    explicit PipeReceiver(std::shared_ptr<detail::PipeState<T>> state)
        : state_(std::move(state))
    {
    }

    PipeReceiver(const PipeReceiver &) = delete;
    PipeReceiver & operator=(const PipeReceiver &) = delete;
    PipeReceiver(PipeReceiver &&) = default;
    PipeReceiver & operator=(PipeReceiver &&) = default;

    // Suspends if empty; returns nullopt on EOF
    [[nodiscard]] auto recv() { return state_->recv(); }

private:
    std::shared_ptr<detail::PipeState<T>> state_;
};

template <typename T>
std::pair<std::shared_ptr<PipeSender<T>>, std::unique_ptr<PipeReceiver<T>>>
makePipe(Scheduler * scheduler = Scheduler::current())
{
    auto state = std::make_shared<detail::PipeState<T>>(scheduler);
    return {
        std::make_shared<PipeSender<T>>(state),
        std::make_unique<PipeReceiver<T>>(state),
    };
}

} // namespace nitrocoro
