/**
 * @file WsConnection.h
 * @brief WebSocket connection — framed message read/write over an io::Stream
 */
#pragma once

#include <nitrocoro/core/Task.h>
#include <nitrocoro/io/Stream.h>
#include <nitrocoro/websocket/WsTypes.h>

#include <string>

namespace nitrocoro::websocket
{

struct WsMessage
{
    WsMessageType type;
    std::string payload;
};

class WsConnection
{
public:
    enum class Role
    {
        Server,
        Client,
    };

    explicit WsConnection(io::StreamPtr stream, Role role)
        : stream_(std::move(stream)), role_(role) {}

    /** Read one complete (possibly fragmented) message. Returns nullopt on close. */
    Task<std::optional<WsMessage>> receive();

    Task<> send(std::string_view data, WsMessageType type = WsMessageType::Text);
    Task<> shutdown(CloseCode code = CloseCode::NormalClosure, std::string_view reason = "");
    Task<> forceClose();

private:
    Task<> sendFrame(uint8_t opcode, const void * data, size_t len);

    io::StreamPtr stream_;
    Role role_;
};

} // namespace nitrocoro::websocket
