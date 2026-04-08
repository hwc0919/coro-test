/**
 * @file WsServer.h
 * @brief WebSocket server — attaches to HttpServer via RequestUpgrader
 */
#pragma once

#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/websocket/WsConnection.h>
#include <nitrocoro/websocket/WsRouter.h>

#include <functional>
#include <string>

namespace nitrocoro::websocket
{

class WsServer
{
public:
    using Handler = std::function<Task<>(WsConnection &)>;

    template <typename F>
    void route(const std::string & path, F && handler);
    template <typename F>
    void routeRegex(const std::string & pattern, F && handler);

    /** Registers this WsServer as the RequestUpgrader on the given HttpServer. */
    void attachTo(http::HttpServer & server);

private:
    Task<std::optional<http::HttpServer::StreamHandler>> handleUpgrade(http::IncomingRequestPtr req,
                                                                       http::ServerResponsePtr resp);
    WsRouter router_;
};

template <typename F>
void WsServer::route(const std::string & path, F && handler)
{
    router_.addRoute(path, std::forward<F>(handler));
}

template <typename F>
void WsServer::routeRegex(const std::string & pattern, F && handler)
{
    router_.addRouteRegex(pattern, std::forward<F>(handler));
}

} // namespace nitrocoro::websocket
