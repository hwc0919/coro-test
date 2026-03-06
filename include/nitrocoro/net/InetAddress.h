/**
 * @file InetAddress.h
 * @brief Network address wrapper for IPv4/IPv6
 */
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <string>

namespace nitrocoro::net
{

class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, bool loopback_only = false, bool ipv6 = false);
    InetAddress(const std::string & ip, uint16_t port, bool ipv6 = false);
    explicit InetAddress(const struct sockaddr_in & addr);
    explicit InetAddress(const struct sockaddr_in6 & addr);

    sa_family_t family() const { return addr_.sin_family; }
    bool isIpV6() const { return addr_.sin_family == AF_INET6; }

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    bool isLoopbackIp() const;

    const struct sockaddr * getSockAddr() const
    {
        return reinterpret_cast<const struct sockaddr *>(&addr6_);
    }

    void setSockAddrInet6(const struct sockaddr_in6 & addr6) { addr6_ = addr6; }

    uint32_t ipNetEndian() const { return addr_.sin_addr.s_addr; }
    uint16_t portNetEndian() const { return addr_.sin_port; }

    static InetAddress getLocalAddr(int fd);

private:
    union {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };
};

} // namespace nitrocoro::net
