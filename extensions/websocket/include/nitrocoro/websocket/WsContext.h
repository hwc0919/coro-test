/**
 * @file WsContext.h
 * @brief WebSocket request context — carries the upgrade request and response,
 *        and provides the accept() entry point to complete the handshake.
 */
#pragma once
#include <nitrocoro/websocket/WsConnection.h>

#include <nitrocoro/core/Future.h>
#include <nitrocoro/http/HttpStream.h>

#include <atomic>

namespace nitrocoro::websocket
{

class WsContext;
using WsContextPtr = std::shared_ptr<WsContext>;

class WsContext
{
public:
    WsContext(http::IncomingRequestPtr req,
              http::ServerResponsePtr resp,
              Future<WsConnection> connFuture);
    ~WsContext();

    /** Completes the WebSocket handshake and returns the established connection. */
    Task<WsConnection> accept();

    http::IncomingRequestPtr req;
    http::ServerResponsePtr resp;

private:
    friend class WsServer;

    std::atomic_flag accepted_{};
    Promise<bool> acceptPromise_;
    Future<WsConnection> connFuture_;
};

} // namespace nitrocoro::websocket
