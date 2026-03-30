#pragma once

#include <nitrocoro/io/Channel.h>
#include <unistd.h>

namespace nitrocoro::io::adapters
{

using nitrocoro::io::Channel;

// BufferReader never calls enable/disableReading, it assumes that
// the reading event is enabled by user and won't be disabled.
struct BufferReader
{
    BufferReader(void * buf, size_t len)
        : buf_(buf), len_(len) {}
    size_t readLen() const { return readLen_; }
    int savedErrno() const { return savedErrno_; }

    Channel::IoStatus operator()(int fd, Channel *)
    {
        if (!buf_ || len_ == 0)
            return Channel::IoStatus::Success;

        ssize_t ret = ::read(fd, buf_, len_);
        if (ret > 0)
        {
            readLen_ = ret;
            return Channel::IoStatus::Success;
        }
        else if (ret == 0)
        {
            return Channel::IoStatus::Eof;
        }
        else
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    return Channel::IoStatus::NeedRead;
                case EINTR:
                    return Channel::IoStatus::Retry;
                case ECONNRESET:
                    return Channel::IoStatus::Eof;
                default:
                    savedErrno_ = errno;
                    return Channel::IoStatus::Error;
            }
        }
    }

private:
    void * buf_;
    size_t len_;
    size_t readLen_{ 0 };
    int savedErrno_{ 0 };
};

} // namespace nitrocoro::io::adapters
