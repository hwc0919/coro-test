/**
 * @file Stream.h
 * @brief Type-erased byte stream wrapper
 */
#pragma once

#include <nitrocoro/core/Task.h>

#include <memory>

namespace nitrocoro::io
{

template <typename S>
concept StreamConcept = requires(S & s, void * rbuf, const void * wbuf, size_t len) {
    { s.read(rbuf, len) } -> std::same_as<Task<size_t>>;
    { s.write(wbuf, len) } -> std::same_as<Task<size_t>>;
    { s.shutdown() } -> std::same_as<Task<>>;
};

class Stream;
using StreamPtr = std::shared_ptr<Stream>;

/**
 * @brief Type-erased wrapper for any byte stream (TcpConnection, TlsStream, etc.)
 *
 * Uses the Holder pattern to erase the concrete stream type while preserving
 * the async I/O interface. Virtual functions are only used internally in the
 * Holder, not exposed in the public API.
 */
class Stream
{
public:
    template <StreamConcept S>
    explicit Stream(std::shared_ptr<S> stream)
        : holder_(std::make_shared<Holder<S>>(std::move(stream)))
    {
    }
    ~Stream() = default;

    Stream(const Stream &) = delete;
    Stream & operator=(const Stream &) = delete;
    Stream(Stream &&) = delete;
    Stream & operator=(Stream &&) = delete;

    Task<size_t> read(void * buf, size_t len) { return holder_->read(buf, len); }
    Task<size_t> write(const void * buf, size_t len) { return holder_->write(buf, len); }
    Task<> shutdown() { return holder_->shutdown(); }
    bool peerClosed() { return holder_->peerClosed(); }

private:
    struct HolderBase
    {
        virtual ~HolderBase() = default;
        virtual Task<size_t> read(void * buf, size_t len) = 0;
        virtual Task<size_t> write(const void * buf, size_t len) = 0;
        virtual Task<> shutdown() = 0;
        virtual bool peerClosed() const = 0;
    };

    template <typename S>
    struct Holder : HolderBase
    {
        std::shared_ptr<S> stream;

        explicit Holder(std::shared_ptr<S> s)
            : stream(std::move(s)) {}
        Task<size_t> read(void * buf, size_t len) override { co_return co_await stream->read(buf, len); }
        Task<size_t> write(const void * buf, size_t len) override { co_return co_await stream->write(buf, len); }
        Task<> shutdown() override { co_await stream->shutdown(); }
        bool peerClosed() const override
        {
            if constexpr (requires { stream->peerClosed(); })
                return stream->peerClosed();
            else
                return false;
        }
    };

    std::shared_ptr<HolderBase> holder_;
};

} // namespace nitrocoro::io
