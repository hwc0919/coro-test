/**
 * @file WsClient.h
 * @brief WebSocket client — performs HTTP upgrade handshake and returns a WsConnection
 */
#pragma once

#include <nitrocoro/io/Stream.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/Url.h>
#include <nitrocoro/websocket/WsConnection.h>

#include <functional>
#include <string>

namespace nitrocoro::websocket
{

class WsClient
{
public:
    using StreamUpgrader = std::function<Task<io::StreamPtr>(net::TcpConnectionPtr)>;

    explicit WsClient(std::string url);
    explicit WsClient(net::Url url);

    void setStreamUpgrader(StreamUpgrader upgrader) { upgrader_ = std::move(upgrader); }

    Task<WsConnection> connect();

private:
    net::Url url_;
    StreamUpgrader upgrader_;
};

} // namespace nitrocoro::websocket
