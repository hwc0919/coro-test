/**
 * @file Socket.cc
 * @brief Implementation of Socket
 */
#include <nitrocoro/net/Socket.h>

#include <nitrocoro/utils/Debug.h>

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace nitrocoro::net
{

Socket::~Socket() noexcept
{
    if (fd_ >= 0)
        ::close(fd_);
}

Socket & Socket::operator=(Socket && other) noexcept
{
    if (this != &other)
    {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Socket::shutdownWrite() noexcept
{
    if (::shutdown(fd_, SHUT_WR) < 0 && errno != ENOTCONN)
        NITRO_ERROR("shutdownWrite fd %d failed: %s", fd_, strerror(errno));
}

int Socket::getLastError(int fd) noexcept
{
    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        return errno;
    return error;
}

} // namespace nitrocoro::net
