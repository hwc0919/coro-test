/**
 * @file ws_test.cc
 * @brief Tests for WsServer, WsConnection, and WsClient
 */
#include <nitrocoro/websocket/WsClient.h>
#include <nitrocoro/websocket/WsRouter.h>
#include <nitrocoro/websocket/WsServer.h>

#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/TcpServer.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/Base64.h>
#include <nitrocoro/utils/Sha1.h>

#include <string>

using namespace nitrocoro;
using namespace nitrocoro::websocket;
using namespace std::chrono_literals;

// ── Raw helpers ───────────────────────────────────────────────────────────────

static std::string computeAccept(const std::string & key)
{
    auto digest = utils::sha1(key + std::string{ kWebSocketGuid });
    return utils::base64Encode(
        std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

static Task<std::string> readHttpResponse(net::TcpConnection & conn)
{
    std::string buf;
    char c;
    while (true)
    {
        co_await conn.read(&c, 1);
        buf += c;
        if (buf.size() >= 4 && buf.substr(buf.size() - 4) == "\r\n\r\n")
            co_return buf;
    }
}

static Task<> sendMaskedText(net::TcpConnection & conn, std::string_view text)
{
    std::vector<uint8_t> frame;
    frame.push_back(0x81);
    size_t len = text.size();
    if (len < 126)
        frame.push_back(0x80 | static_cast<uint8_t>(len));
    else if (len < 65536)
    {
        frame.push_back(0x80 | 126);
        frame.push_back(len >> 8);
        frame.push_back(len & 0xFF);
    }
    uint8_t mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < len; ++i)
        frame.push_back(static_cast<uint8_t>(text[i]) ^ mask[i % 4]);
    co_await conn.write(frame.data(), frame.size());
}

static Task<std::string> recvTextFrame(net::TcpConnection & conn)
{
    uint8_t header[2];
    size_t got = 0;
    while (got < 2)
        got += co_await conn.read(header + got, 2 - got);
    uint64_t payloadLen = header[1] & 0x7F;
    if (payloadLen == 126)
    {
        uint8_t ext[2];
        got = 0;
        while (got < 2)
            got += co_await conn.read(ext + got, 2 - got);
        payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    }
    std::string payload(payloadLen, '\0');
    got = 0;
    while (got < payloadLen)
        got += co_await conn.read(payload.data() + got, payloadLen - got);
    co_return payload;
}

// Complete WS handshake over raw TCP, return the connection
static Task<net::TcpConnectionPtr> rawWsHandshake(uint16_t port, const std::string & path)
{
    auto conn = co_await net::TcpConnection::connect({ "127.0.0.1", port });
    std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string req = "GET " + path + " HTTP/1.1\r\n"
                                      "Host: localhost\r\n"
                                      "Upgrade: websocket\r\n"
                                      "Connection: Upgrade\r\n"
                                      "Sec-WebSocket-Key: "
                      + key + "\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "\r\n";
    co_await conn->write(req.data(), req.size());
    co_await readHttpResponse(*conn);
    co_return conn;
}

// A minimal raw WS server: completes handshake then runs handler(conn)
template <typename F>
static Task<net::TcpServer *> startRawWsServer(net::TcpServer & server, F handler)
{
    Scheduler::current()->spawn([&server, handler = std::move(handler)]() mutable -> Task<> {
        co_await server.start([handler](net::TcpConnectionPtr conn) mutable -> Task<> {
            // Read HTTP upgrade request
            std::string buf;
            char c;
            while (true)
            {
                co_await conn->read(&c, 1);
                buf += c;
                if (buf.size() >= 4 && buf.substr(buf.size() - 4) == "\r\n\r\n")
                    break;
            }
            // Extract key and send 101
            std::string key;
            auto pos = buf.find("Sec-WebSocket-Key: ");
            if (pos != std::string::npos)
            {
                pos += 19;
                key = buf.substr(pos, buf.find("\r\n", pos) - pos);
            }
            std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: "
                               + computeAccept(key) + "\r\n\r\n";
            co_await conn->write(resp.data(), resp.size());
            co_await handler(conn);
        });
    });
    co_await server.started().get();
    co_return &server;
}

// ── Test helpers ──────────────────────────────────────────────────────────────

static Task<> startServer(http::HttpServer & server)
{
    Scheduler::current()->spawn([&server]() -> Task<> { co_await server.start(); });
    co_await sleep(50ms);
}

static std::string wsUrl(uint16_t port, const std::string & path)
{
    return "ws://127.0.0.1:" + std::to_string(port) + path;
}

// ── WsServer tests ────────────────────────────────────────────────────────────

/** Server echoes text messages back to client. */
NITRO_TEST(ws_echo)
{
    http::HttpServer server(0);
    WsServer ws;
    ws.route("/ws", [](WsContextPtr wsCtx) -> Task<> {
        auto conn = co_await wsCtx->accept();
        while (auto msg = co_await conn.receive())
            co_await conn.send(msg->payload);
    });
    ws.attachTo(server);
    co_await startServer(server);

    WsClient client(wsUrl(server.listeningPort(), "/ws"));
    auto conn = co_await client.connect();

    co_await conn.send("hello");
    auto msg = co_await conn.receive();
    NITRO_REQUIRE(msg.has_value());
    NITRO_CHECK_EQ(msg->payload, "hello");

    co_await conn.send("world");
    msg = co_await conn.receive();
    NITRO_REQUIRE(msg.has_value());
    NITRO_CHECK_EQ(msg->payload, "world");

    co_await server.stop();
}

/** Non-WebSocket requests on the same server still work normally. */
NITRO_TEST(ws_http_coexist)
{
    http::HttpServer server(0);
    server.route("/ping", { "GET" }, [](auto req, auto resp) {
        resp->setBody("pong");
    });
    WsServer ws;
    ws.route("/ws", [](WsContextPtr wsCtx) -> Task<> {
        auto conn = co_await wsCtx->accept();
        co_await conn.shutdown();
    });
    ws.attachTo(server);
    co_await startServer(server);

    auto resp = co_await http::get("http://127.0.0.1:" + std::to_string(server.listeningPort()) + "/ping");
    NITRO_CHECK_EQ(resp.statusCode(), http::StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "pong");

    WsClient client(wsUrl(server.listeningPort(), "/ws"));
    auto conn = co_await client.connect();
    auto msg = co_await conn.receive(); // server sends close
    NITRO_CHECK(!msg.has_value());

    co_await server.stop();
}

/** Upgrade request to an unregistered path is ignored (falls through to 404). */
NITRO_TEST(ws_unknown_path)
{
    http::HttpServer server(0);
    WsServer ws;
    ws.attachTo(server);
    co_await startServer(server);

    // Must use raw TCP — need to verify the 404 status code, WsClient would just throw
    auto conn = co_await net::TcpConnection::connect({ "127.0.0.1", server.listeningPort() });
    std::string req = "GET /no-such-path HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    co_await conn->write(req.data(), req.size());

    auto resp = co_await readHttpResponse(*conn);
    NITRO_CHECK(resp.find("404") != std::string::npos);

    co_await server.stop();
}

/** Test WsRouter basic functionality. */
NITRO_TEST(ws_router_basic)
{
    WsRouter router;

    router.addRoute("/ws/chat", [](WsContextPtr) -> Task<> { co_return; });

    auto result = router.route("/ws/chat");
    NITRO_CHECK(result);
    NITRO_CHECK(result.handler);
    NITRO_CHECK(result.params.empty());

    result = router.route("/ws/unknown");
    NITRO_CHECK(!result);

    router.addRoute("/ws/room/:id", [](WsContextPtr) -> Task<> { co_return; });

    result = router.route("/ws/room/123");
    NITRO_CHECK(result);
    NITRO_CHECK_EQ(result.params.size(), 1u);
    NITRO_CHECK_EQ(result.params.at("id"), "123");

    router.addRouteRegex(R"(/ws/user/(\d+))", [](WsContextPtr) -> Task<> { co_return; });

    result = router.route("/ws/user/456");
    NITRO_CHECK(result);
    NITRO_CHECK_EQ(result.params.size(), 1u);
    NITRO_CHECK_EQ(result.params.at("$1"), "456");

    co_return;
}

/** Test WsRouter path parameters. */
NITRO_TEST(ws_router_path_params)
{
    http::HttpServer server(0);
    WsServer ws;
    std::string receivedId;

    ws.route("/ws/room/:id", [&receivedId](WsContextPtr wsCtx) -> Task<> {
        receivedId = wsCtx->req()->pathParams().at("id");
        auto conn = co_await wsCtx->accept();
        co_await conn.send(receivedId);
        co_await conn.shutdown();
    });
    ws.attachTo(server);
    co_await startServer(server);

    WsClient client(wsUrl(server.listeningPort(), "/ws/room/123"));
    auto conn = co_await client.connect();

    auto msg = co_await conn.receive();
    NITRO_REQUIRE(msg.has_value());
    NITRO_CHECK_EQ(msg->payload, "123");
    NITRO_CHECK_EQ(receivedId, "123");

    co_await server.stop();
}

/** Handler reads req headers and sets custom resp headers before accept(). */
NITRO_TEST(ws_context_req_resp)
{
    http::HttpServer server(0);
    WsServer ws;

    ws.route("/ws", [&TEST_CTX](WsContextPtr wsCtx) -> Task<> {
        auto & key = wsCtx->req()->getHeader(http::HttpHeader::NameCode::SecWebSocketKey);
        NITRO_REQUIRE(!key.empty());
        wsCtx->resp()->setHeader("X-Custom-Header", "nitrocoro-ws");
        auto conn = co_await wsCtx->accept();
        co_await conn.shutdown();
    });
    ws.attachTo(server);
    co_await startServer(server);

    // Must use raw TCP — need to inspect the 101 response headers directly
    auto conn = co_await net::TcpConnection::connect({ "127.0.0.1", server.listeningPort() });
    std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string req = "GET /ws HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: "
                      + key + "\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "\r\n";
    co_await conn->write(req.data(), req.size());

    auto resp = co_await readHttpResponse(*conn);
    NITRO_CHECK(resp.find("101") != std::string::npos);
    NITRO_CHECK(resp.find("X-Custom-Header: nitrocoro-ws") != std::string::npos);

    co_await server.stop();
}

/** Handler rejects the upgrade by not calling accept() — server returns 403. */
NITRO_TEST(ws_context_reject)
{
    http::HttpServer server(0);
    WsServer ws;

    ws.route("/ws", [](WsContextPtr wsCtx) -> Task<> {
        wsCtx->resp()->setStatus(http::StatusCode::k403Forbidden);
        wsCtx->resp()->setBody("forbidden");
        co_return;
    });
    ws.attachTo(server);
    co_await startServer(server);

    // Must use raw TCP — need to verify the 403 status code, WsClient would just throw
    auto conn = co_await net::TcpConnection::connect({ "127.0.0.1", server.listeningPort() });
    std::string req = "GET /ws HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "\r\n";
    co_await conn->write(req.data(), req.size());

    auto resp = co_await readHttpResponse(*conn);
    NITRO_CHECK(resp.find("403") != std::string::npos);

    co_await server.stop();
}

/** Test WsRouter wildcard routes. */
NITRO_TEST(ws_router_wildcard)
{
    http::HttpServer server(0);
    WsServer ws;

    ws.route("/ws/files/*path", [](WsContextPtr wsCtx) -> Task<> {
        auto conn = co_await wsCtx->accept();
        co_await conn.send("file_handler_called");
        co_await conn.shutdown();
    });
    ws.attachTo(server);
    co_await startServer(server);

    WsClient client(wsUrl(server.listeningPort(), "/ws/files/a/b/c.txt"));
    auto conn = co_await client.connect();

    auto msg = co_await conn.receive();
    NITRO_REQUIRE(msg.has_value());
    NITRO_CHECK_EQ(msg->payload, "file_handler_called");

    co_await server.stop();
}

// ── WsClient tests ────────────────────────────────────────────────────────────

/** Client frames must be masked — verify the mask bit on the wire via a raw server. */
NITRO_TEST(ws_client_sends_masked_frames)
{
    net::TcpServer tcpServer(0);
    bool maskBitSet = false;

    co_await startRawWsServer(tcpServer, [&maskBitSet](net::TcpConnectionPtr conn) -> Task<> {
        // Read the first frame header byte 2: bit 7 is the mask bit
        uint8_t header[2];
        size_t got = 0;
        while (got < 2)
            got += co_await conn->read(header + got, 2 - got);
        maskBitSet = (header[1] & 0x80) != 0;
    });

    WsClient client(wsUrl(tcpServer.port(), "/ws"));
    auto conn = co_await client.connect();
    co_await conn.send("hello");

    co_await sleep(50ms);
    NITRO_CHECK(maskBitSet);

    co_await tcpServer.stop();
}

/** Client receives a masked frame from server — should throw (masking violation). */
NITRO_TEST(ws_client_mask_violation)
{
    net::TcpServer tcpServer(0);

    co_await startRawWsServer(tcpServer, [](net::TcpConnectionPtr conn) -> Task<> {
        // Send a masked frame — servers must NOT mask, this is a protocol violation
        uint8_t frame[] = {
            0x81,                   // FIN + Text
            0x85,                   // mask bit set + len=5
            0x12, 0x34, 0x56, 0x78, // mask key
            'h' ^ 0x12, 'e' ^ 0x34, 'l' ^ 0x56, 'l' ^ 0x78, 'o' ^ 0x12
        };
        co_await conn->write(frame, sizeof(frame));
    });

    WsClient client(wsUrl(tcpServer.port(), "/ws"));
    auto conn = co_await client.connect();

    NITRO_CHECK_THROWS(co_await conn.receive());

    co_await tcpServer.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
