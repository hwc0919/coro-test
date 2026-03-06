/**
 * @file TcpServer.cc
 * @brief Implementation of coroutine-based TCP server
 */
#include <nitrocoro/net/TcpServer.h>

#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/net/Socket.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/utils/Debug.h>

#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

namespace nitrocoro::net
{
using io::Channel;
using net::Socket;

TcpServer::TcpServer(uint16_t port, Scheduler * scheduler)
    : TcpServer(InetAddress(port), scheduler)
{
}

TcpServer::TcpServer(const InetAddress & addr, Scheduler * scheduler)
    : addr_(addr)
    , scheduler_(scheduler)
    , stopPromise_(scheduler)
    , stopFuture_(stopPromise_.get_future().share())
{
    setup_socket();
}

TcpServer::~TcpServer() = default;

void TcpServer::setup_socket()
{
    int fd = ::socket(addr_.family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        throw std::runtime_error("Failed to create socket");
    listenSocketPtr_ = std::make_shared<Socket>(fd);

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    if (addr_.isIpV6())
    {
        int v6only = 1;
        ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
    }

    socklen_t addrLen = addr_.isIpV6() ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    if (::bind(fd, addr_.getSockAddr(), addrLen) < 0)
        throw std::runtime_error(std::string("Failed to bind socket: ") + strerror(errno));

    if (addr_.toPort() == 0)
    {
        sockaddr_storage ss{};
        socklen_t ssLen = sizeof(ss);
        if (::getsockname(fd, reinterpret_cast<sockaddr *>(&ss), &ssLen) == 0)
        {
            if (addr_.isIpV6())
                addr_.setSockAddrInet6(*reinterpret_cast<sockaddr_in6 *>(&ss));
            else
                addr_ = InetAddress(*reinterpret_cast<sockaddr_in *>(&ss));
        }
    }
}

struct Acceptor
{
    Channel::IoStatus operator()(int fd, Channel *)
    {
        socklen_t len = sizeof(sockaddr_in6);
        int connFd = ::accept4(fd, reinterpret_cast<struct sockaddr *>(&clientAddr_), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connFd >= 0)
        {
            socket_ = std::make_shared<Socket>(connFd);
            return Channel::IoStatus::Success;
        }
        switch (errno)
        {
            case EAGAIN:
#if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
#endif
                return Channel::IoStatus::NeedRead;
            case EINTR:
                return Channel::IoStatus::Retry;
            default:
                return Channel::IoStatus::Error;
        }
    }

    std::shared_ptr<Socket> takeSocket() { return std::move(socket_); }
    InetAddress takeClientAddr()
    {
        if (clientAddr_.sin6_family == AF_INET6)
            return InetAddress(clientAddr_);
        return InetAddress(*reinterpret_cast<sockaddr_in *>(&clientAddr_));
    }

private:
    sockaddr_in6 clientAddr_{};
    std::shared_ptr<Socket> socket_;
};

Task<> TcpServer::start(ConnectionHandler handler)
{
    co_await scheduler_->switch_to();
    if (started_.exchange(true))
    {
        throw std::logic_error("TcpServer already started");
    }

    if (::listen(listenSocketPtr_->fd(), 128) < 0)
    {
        stopped_.store(true);
        stopPromise_.set_value();
        throw std::runtime_error(std::string("Failed to listen: ") + strerror(errno));
    }
    NITRO_DEBUG("TcpServer listening on port %hu", addr_.toPort());

    auto handlerPtr = std::make_shared<ConnectionHandler>(std::move(handler));
    std::weak_ptr<ConnectionSet> weakConnSet{ connSetPtr_ };
    listenChannel_ = std::make_unique<Channel>(listenSocketPtr_->fd(), TriggerMode::LevelTriggered, scheduler_);
    listenChannel_->setGuard(listenSocketPtr_);
    listenChannel_->enableReading();
    while (!stopped_.load())
    {
        Acceptor acceptor;
        auto result = co_await listenChannel_->performRead(&acceptor);
        if (result == Channel::IoResult::Canceled)
        {
            NITRO_DEBUG("TcpServer::close() called, break accepting loop");
            break;
        }
        if (result != Channel::IoResult::Success)
        {
            NITRO_ERROR("Accept error: IoResult=%d", static_cast<int>(result));
            break;
        }

        NITRO_DEBUG("Accepted connection");
        auto socket = acceptor.takeSocket();
        auto peerAddr = acceptor.takeClientAddr();
        auto ioChannelPtr = std::make_unique<Channel>(socket->fd(), TriggerMode::EdgeTriggered, scheduler_);
        ioChannelPtr->setGuard(socket);
        auto connPtr = std::make_shared<TcpConnection>(std::move(ioChannelPtr), socket, addr_, peerAddr);
        connSetPtr_->insert(connPtr);
        scheduler_->spawn([scheduler = scheduler_, handlerPtr, connPtr, weakConnSet]() mutable -> Task<> {
            try
            {
                co_await (*handlerPtr)(connPtr);
            }
            catch (const std::exception & ex)
            {
                NITRO_ERROR("TcpServer handler unhandled exception: %s", ex.what());
            }
            catch (...)
            {
                NITRO_ERROR("TcpServer handler unknown exception");
            }
            co_await scheduler->switch_to();
            // Handler returned — connection's logical lifetime is over.
            // Erase from the set so stop() no longer tries to shut it down.
            if (auto connSetPtr = weakConnSet.lock())
            {
                connSetPtr->erase(connPtr);
            }
        });
    }
    listenChannel_->disableAll();
    stopPromise_.set_value();
    NITRO_DEBUG("TcpServer::start() quit");
}

Task<> TcpServer::stop()
{
    co_await scheduler_->switch_to();
    if (stopped_.exchange(true))
        co_return;

    NITRO_DEBUG("TcpServer::stop() requested");
    listenChannel_->disableAll(); // stop listening first
    listenChannel_->cancelAll();

    std::vector<TcpConnectionPtr> conns(connSetPtr_->begin(), connSetPtr_->end());
    for (auto & c : conns)
    {
        co_await c->shutdown();
    }
    co_await stopFuture_.get();
}

Task<> TcpServer::wait() const
{
    co_await stopFuture_.get();
}

} // namespace nitrocoro::net
