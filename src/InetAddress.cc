/**
 * @file InetAddress.cc
 * @brief Network address implementation
 */
#include <nitrocoro/net/InetAddress.h>

#include <cstring>

#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif

namespace nitrocoro::net
{

static constexpr in_addr_t kInAddrAny = INADDR_ANY;
static constexpr in_addr_t kInAddrLoopback = INADDR_LOOPBACK;

InetAddress::InetAddress(uint16_t port, bool loopback_only, bool ipv6)
{
    if (ipv6)
    {
        memset(&addr6_, 0, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopback_only ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = htons(port);
    }
    else
    {
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopback_only ? kInAddrLoopback : kInAddrAny;
        addr_.sin_addr.s_addr = htonl(ip);
        addr_.sin_port = htons(port);
    }
}

InetAddress::InetAddress(const std::string & ip, uint16_t port, bool ipv6)
{
    if (ipv6)
    {
        memset(&addr6_, 0, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_port = htons(port);
        inet_pton(AF_INET6, ip.c_str(), &addr6_.sin6_addr);
    }
    else
    {
        memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    }
}

InetAddress::InetAddress(const struct sockaddr_in & addr)
{
    memset(&addr_, 0, sizeof(addr_));
    addr_ = addr;
}

InetAddress::InetAddress(const struct sockaddr_in6 & addr)
{
    memset(&addr6_, 0, sizeof(addr6_));
    addr6_ = addr;
}

std::string InetAddress::toIp() const
{
    char buf[INET6_ADDRSTRLEN];
    if (addr_.sin_family == AF_INET)
    {
        inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    }
    else if (addr_.sin_family == AF_INET6)
    {
        inet_ntop(AF_INET6, &addr6_.sin6_addr, buf, sizeof(buf));
    }
    return buf;
}

std::string InetAddress::toIpPort() const
{
    char buf[64];
    snprintf(buf, sizeof(buf), ":%u", ntohs(addr_.sin_port));
    return toIp() + std::string(buf);
}

uint16_t InetAddress::toPort() const
{
    return ntohs(portNetEndian());
}

bool InetAddress::isLoopbackIp() const
{
    if (addr_.sin_family == AF_INET)
    {
        uint32_t ip_addr = ntohl(addr_.sin_addr.s_addr);
        return ip_addr == 0x7f000001;
    }
    else if (addr_.sin_family == AF_INET6)
    {
        const uint32_t * addrP = reinterpret_cast<const uint32_t *>(&addr6_.sin6_addr);
        return addrP[0] == 0 && addrP[1] == 0 && addrP[2] == 0 && ntohl(addrP[3]) == 1;
    }
    return false;
}

InetAddress InetAddress::getLocalAddr(int fd)
{
    sockaddr_in6 ss{};
    socklen_t len = sizeof(ss);
    ::getsockname(fd, reinterpret_cast<sockaddr *>(&ss), &len);
    if (ss.sin6_family == AF_INET6)
        return InetAddress(ss);
    return InetAddress(*reinterpret_cast<sockaddr_in *>(&ss));
}

} // namespace nitrocoro::net
