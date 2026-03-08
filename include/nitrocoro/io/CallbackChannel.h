/**
 * @file CallbackChannel.h
 * @brief Callback-driven channel for integrating third-party async libraries (e.g. hiredis)
 */
#pragma once

#include <nitrocoro/core/Scheduler.h>

#include <functional>
#include <memory>

namespace nitrocoro::io
{

class CallbackChannel
{
    struct State;

public:
    CallbackChannel(int fd, Scheduler * scheduler);
    ~CallbackChannel() noexcept;

    CallbackChannel(const CallbackChannel &) = delete;
    CallbackChannel & operator=(const CallbackChannel &) = delete;

    void setGuard(std::shared_ptr<void> guard) { guard_ = std::move(guard); }

    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    void setReadableCallback(std::function<void()> cb);
    void setWritableCallback(std::function<void()> cb);
    void setCloseCallback(std::function<void()> cb);
    void setErrorCallback(std::function<void()> cb);

private:
    static void handleIoEvents(State * state, uint32_t ev);
    void setEvents(uint32_t newEvents);

    uint64_t id_;
    int fd_;
    Scheduler * scheduler_;
    uint32_t events_{ 0 };
    std::shared_ptr<State> state_;
    std::shared_ptr<void> guard_;
};

} // namespace nitrocoro::io
