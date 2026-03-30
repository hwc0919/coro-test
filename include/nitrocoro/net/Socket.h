/**
 * @file Socket.h
 * @brief RAII wrapper for a socket file descriptor — owns the fd and closes it on destruction
 */
#pragma once

namespace nitrocoro::net
{

class Socket
{
public:
    explicit Socket(int fd) noexcept
        : fd_(fd) {}
    ~Socket() noexcept;

    Socket(Socket && other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }
    Socket & operator=(Socket && other) noexcept;

    Socket(const Socket &) = delete;
    Socket & operator=(const Socket &) = delete;

    int fd() const noexcept { return fd_; }
    bool valid() const noexcept { return fd_ >= 0; }
    void shutdownWrite() noexcept;
    static int getLastError(int fd) noexcept;

private:
    int fd_{ -1 };
};

} // namespace nitrocoro::net
