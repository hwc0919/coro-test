#pragma once

#include <nitrocoro/io/Channel.h>
#include <unistd.h>

namespace nitrocoro::io::adapters
{

using nitrocoro::io::Channel;

// BufferWriter calls enable/disableWriting only when needed, it assumes
// that user don't enable/disable write event manually.
// If situation is different, user should implement their own Writer.
struct BufferWriter
{
    BufferWriter(const void * buf, size_t len)
        : buf_(buf), len_(len)
    {
    }

    int savedErrno() const { return savedErrno_; }

    Channel::IoStatus operator()(int fd, Channel * channel)
    {
        if (!buf_ || len_ == 0)
        {
            return Channel::IoStatus::Success;
        }

        ssize_t ret = ::write(fd, static_cast<const char *>(buf_) + wroteLen_, len_ - wroteLen_);
        if (ret > 0)
        {
            wroteLen_ += ret;
            if (wroteLen_ >= static_cast<ssize_t>(len_))
            {
                channel->disableWriting();
                return Channel::IoStatus::Success;
            }
            else
            {
                return Channel::IoStatus::Retry;
            }
        }
        else // ret <= 0
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    channel->enableWriting();
                    return Channel::IoStatus::NeedWrite;
                case EINTR:
                    return Channel::IoStatus::Retry;
                case EPIPE:
                case ECONNRESET:
                    return Channel::IoStatus::Eof;
                default:
                    savedErrno_ = errno;
                    return Channel::IoStatus::Error;
            }
        }
    }

private:
    const void * buf_;
    size_t len_;
    ssize_t wroteLen_{ 0 };
    int savedErrno_{ 0 };
};

} // namespace nitrocoro::io::adapters
