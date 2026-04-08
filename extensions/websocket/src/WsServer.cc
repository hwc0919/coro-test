/**
 * @file WsServer.cc
 * @brief WebSocket upgrade handshake + routing
 */
#include <nitrocoro/websocket/WsServer.h>

#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/utils/Base64.h>
#include <nitrocoro/utils/Sha1.h>
#include <nitrocoro/websocket/WsTypes.h>

static std::string computeAccept(const std::string & key)
{
    auto digest = nitrocoro::utils::sha1(key + std::string{ nitrocoro::websocket::kWebSocketGuid });
    return nitrocoro::utils::base64Encode(std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

namespace nitrocoro::websocket
{

void WsServer::attachTo(http::HttpServer & server)
{
    server.setRequestUpgrader([this](http::IncomingRequestPtr req,
                                     http::ServerResponsePtr resp) -> Task<std::optional<http::HttpServer::StreamHandler>> {
        co_return co_await handleUpgrade(req, resp);
    });
}

Task<std::optional<http::HttpServer::StreamHandler>> WsServer::handleUpgrade(http::IncomingRequestPtr req,
                                                                             http::ServerResponsePtr resp)
{
    using http::HttpHeader;

    // Only handle WebSocket upgrades
    auto & upgrade = req->getHeader(HttpHeader::NameCode::Upgrade);
    if (HttpHeader::toLower(upgrade) != "websocket")
        co_return std::nullopt;

    // Use WsRouter to find handler
    auto result = router_.route(req->path());
    if (!result)
        co_return std::nullopt;

    auto & key = req->getHeader(HttpHeader::NameCode::SecWebSocketKey);
    if (key.empty())
        co_return std::nullopt;

    std::string accept = computeAccept(key);

    resp->setStatus(http::StatusCode::k101SwitchingProtocols);
    resp->setHeader(HttpHeader::NameCode::Upgrade, "websocket");
    resp->setHeader(HttpHeader::NameCode::Connection, "Upgrade");
    resp->setHeader(HttpHeader::NameCode::SecWebSocketAccept, accept);

    auto handler = result.handler;
    auto params = std::move(result.params);
    co_return [handler, params = std::move(params)](io::StreamPtr stream) -> Task<> {
        WsConnection conn(std::move(stream));
        // Set path parameters in connection (if WsConnection supports it)
        co_await handler->invoke(conn);
    };
}

} // namespace nitrocoro::websocket
