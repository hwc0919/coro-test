/**
 * @file WsConnection.cc
 * @brief WebSocket framing (RFC 6455)
 */
#include <nitrocoro/websocket/WsConnection.h>

#include <stdexcept>
#include <vector>

namespace
{
enum class Opcode : uint8_t
{
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA,
};
} // namespace

namespace nitrocoro::websocket
{

// ── helpers ──────────────────────────────────────────────────────────────────

static Task<size_t> readExact(io::Stream & s, void * buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        size_t n = co_await s.read(static_cast<char *>(buf) + total, len - total);
        if (n == 0)
            throw std::runtime_error("WsConnection: connection closed");
        total += n;
    }
    co_return total;
}

// ── receive ──────────────────────────────────────────────────────────────────

Task<std::optional<WsMessage>> WsConnection::receive()
{
    std::string payload;
    Opcode finalOpcode = Opcode::Continuation;

    while (true)
    {
        // Read 2-byte frame header
        uint8_t header[2];
        co_await readExact(*stream_, header, 2);

        bool fin = (header[0] & 0x80) != 0;
        Opcode opcode = static_cast<Opcode>(header[0] & 0x0F);
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7F;

        if (payloadLen == 126)
        {
            uint8_t ext[2];
            co_await readExact(*stream_, ext, 2);
            payloadLen = (uint64_t(ext[0]) << 8) | ext[1];
        }
        else if (payloadLen == 127)
        {
            uint8_t ext[8];
            co_await readExact(*stream_, ext, 8);
            payloadLen = 0;
            for (int i = 0; i < 8; ++i)
                payloadLen = (payloadLen << 8) | ext[i];
        }

        uint8_t maskKey[4] = {};
        if (masked)
            co_await readExact(*stream_, maskKey, 4);

        size_t offset = payload.size();
        payload.resize(offset + payloadLen);
        co_await readExact(*stream_, payload.data() + offset, payloadLen);

        if (masked)
            for (size_t i = 0; i < payloadLen; ++i)
                payload[offset + i] ^= maskKey[i % 4];

        // Control frames (ping/pong/close) are never fragmented
        if (opcode == Opcode::Ping)
        {
            co_await sendFrame(static_cast<uint8_t>(Opcode::Pong), payload.data() + offset, payloadLen);
            payload.resize(offset);
            continue;
        }
        if (opcode == Opcode::Close)
            co_return std::nullopt;

        if (opcode != Opcode::Continuation)
            finalOpcode = opcode;

        if (fin)
        {
            WsMessageType type = (finalOpcode == Opcode::Binary) ? WsMessageType::Binary : WsMessageType::Text;
            co_return WsMessage{ type, std::move(payload) };
        }
    }
}

// ── send ─────────────────────────────────────────────────────────────────────

Task<> WsConnection::sendFrame(uint8_t opcode, const void * data, size_t len, bool mask)
{
    std::vector<uint8_t> frame;
    frame.reserve(10 + len);

    frame.push_back(0x80 | opcode); // FIN + opcode

    uint8_t maskBit = mask ? 0x80 : 0x00;
    if (len < 126)
    {
        frame.push_back(maskBit | static_cast<uint8_t>(len));
    }
    else if (len < 65536)
    {
        frame.push_back(maskBit | 126);
        frame.push_back(static_cast<uint8_t>(len >> 8));
        frame.push_back(static_cast<uint8_t>(len));
    }
    else
    {
        frame.push_back(maskBit | 127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>(len >> (i * 8)));
    }

    const auto * bytes = static_cast<const uint8_t *>(data);
    frame.insert(frame.end(), bytes, bytes + len);

    co_await stream_->write(frame.data(), frame.size());
}

Task<> WsConnection::send(std::string_view data, WsMessageType type)
{
    Opcode op = (type == WsMessageType::Binary) ? Opcode::Binary : Opcode::Text;
    co_await sendFrame(static_cast<uint8_t>(op), data.data(), data.size());
}

Task<> WsConnection::shutdown(CloseCode code, std::string_view reason)
{
    uint16_t c = static_cast<uint16_t>(code);
    std::string payload(2 + reason.size(), '\0');
    payload[0] = static_cast<char>(c >> 8);
    payload[1] = static_cast<char>(c);
    payload.replace(2, reason.size(), reason);
    co_await sendFrame(static_cast<uint8_t>(Opcode::Close), payload.data(), payload.size());
}

Task<> WsConnection::forceClose()
{
    co_await stream_->shutdown();
    stream_.reset();
}

} // namespace nitrocoro::websocket
