/**
 * @file WsClient.cc
 * @brief WebSocket client — HTTP upgrade handshake
 */
#include <nitrocoro/websocket/WsClient.h>

#include <nitrocoro/net/Dns.h>
#include <nitrocoro/net/InetAddress.h>
#include <nitrocoro/net/Url.h>
#include <nitrocoro/utils/Base64.h>
#include <nitrocoro/utils/Sha1.h>
#include <nitrocoro/websocket/WsTypes.h>

#include <cstdlib>
#include <stdexcept>

namespace nitrocoro::websocket
{

WsClient::WsClient(std::string url)
    : url_(std::move(url))
{
}

WsClient::WsClient(net::Url url)
    : url_(std::move(url))
{
}

static std::string generateKey()
{
    uint8_t raw[16];
    for (auto & b : raw)
        b = static_cast<uint8_t>(std::rand() & 0xFF);
    return utils::base64Encode(std::string_view(reinterpret_cast<const char *>(raw), sizeof(raw)));
}

static std::string computeExpectedAccept(const std::string & key)
{
    auto digest = utils::sha1(key + std::string{ kWebSocketGuid });
    return utils::base64Encode(std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

static Task<> readUntilHeadersEnd(io::Stream & stream, std::string & buf)
{
    char c;
    while (true)
    {
        size_t n = co_await stream.read(&c, 1);
        if (n == 0)
            throw std::runtime_error("WsClient: connection closed during handshake");
        buf += c;
        if (buf.size() >= 4 && buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0)
            co_return;
    }
}

Task<WsConnection> WsClient::connect()
{
    if (!url_.isValid())
        throw std::invalid_argument("WsClient: invalid URL");

    auto addresses = co_await net::resolve(url_.host());
    if (addresses.empty())
        throw std::runtime_error("WsClient: DNS resolution failed");

    auto tcpConn = co_await net::TcpConnection::connect(
        net::InetAddress(addresses[0].toIp(), url_.port(), addresses[0].isIpV6()));

    io::StreamPtr stream;
    if (url_.scheme() == "wss")
    {
        if (!upgrader_)
            throw std::runtime_error("WsClient: wss:// requires a StreamUpgrader");
        stream = co_await upgrader_(tcpConn);
        if (!stream)
            throw std::runtime_error("WsClient: stream upgrade failed");
    }
    else
    {
        stream = std::make_shared<io::Stream>(tcpConn);
    }

    std::string key = generateKey();
    std::string expectedAccept = computeExpectedAccept(key);

    std::string req = "GET " + url_.fullPath() + " HTTP/1.1\r\n"
                      "Host: " + url_.host() + "\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: " + key + "\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    co_await stream->write(req.data(), req.size());

    std::string resp;
    co_await readUntilHeadersEnd(*stream, resp);

    if (resp.find("101") == std::string::npos)
        throw std::runtime_error("WsClient: server did not return 101");
    if (resp.find(expectedAccept) == std::string::npos)
        throw std::runtime_error("WsClient: invalid Sec-WebSocket-Accept");

    co_return WsConnection(std::move(stream), WsConnection::Role::Client);
}

} // namespace nitrocoro::websocket
