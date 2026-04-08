/**
 * @file WsRouter.h
 * @brief WebSocket router with advanced routing capabilities
 */
#pragma once

#include <nitrocoro/http/RouterCore.h>
#include <nitrocoro/websocket/WsConnection.h>

#include <nitrocoro/core/Task.h>

#include <memory>
#include <string>
#include <vector>

namespace nitrocoro::websocket
{

struct WsHandlerBase
{
    virtual Task<> invoke(WsConnection & conn) = 0;
    virtual ~WsHandlerBase() = default;
};

using WsHandlerPtr = std::shared_ptr<WsHandlerBase>;

template <typename F>
struct WsHandler : WsHandlerBase
{
    explicit WsHandler(F f)
        : f_(std::move(f)) {}

    Task<> invoke(WsConnection & conn) override
    {
        co_await f_(conn);
    }

    F f_;
};

template <typename F>
WsHandlerPtr makeWsHandler(F && f)
{
    return std::make_shared<WsHandler<std::decay_t<F>>>(std::forward<F>(f));
}

/**
 * @brief WebSocket router with advanced routing capabilities.
 *
 * Supports the same routing features as HttpRouter:
 * - Exact match: `/ws/chat`
 * - Path parameters: `/ws/room/:id`
 * - Wildcards: `/ws/files/*path`
 * - Regex routes: via addRouteRegex()
 */
class WsRouter
{
public:
    using PathParams = http::PathParams;

    struct RouteResult
    {
        enum class Reason
        {
            Ok,
            NotFound
        };

        WsHandlerPtr handler;
        PathParams params;
        Reason reason{ Reason::NotFound };

        explicit operator bool() const { return handler != nullptr; }
    };

    template <typename F>
    void addRoute(const std::string & path, F && handler);
    template <typename F>
    void addRouteRegex(const std::string & pattern, F && handler);

    // Returns {handler, params} for the matched route, or {nullptr, {}} if not found.
    RouteResult route(const std::string & path) const;

private:
    http::RouterCore core_;
    std::vector<WsHandlerPtr> handlers_;
};

template <typename F>
void WsRouter::addRoute(const std::string & path, F && handler)
{
    WsHandlerPtr handlerPtr;
    if constexpr (std::is_same_v<std::decay_t<F>, WsHandlerPtr>)
        handlerPtr = std::forward<F>(handler);
    else
        handlerPtr = makeWsHandler(std::forward<F>(handler));

    // WebSocket only uses GET for upgrade, so we use GET as default
    size_t routeId = core_.addRoute(path, { http::methods::Get });
    if (routeId >= handlers_.size())
    {
        handlers_.resize(routeId + 1);
    }
    handlers_[routeId] = handlerPtr;
}

template <typename F>
void WsRouter::addRouteRegex(const std::string & pattern, F && handler)
{
    WsHandlerPtr handlerPtr = makeWsHandler(std::forward<F>(handler));
    size_t routeId = core_.addRouteRegex(pattern, { http::methods::Get });
    if (routeId >= handlers_.size())
    {
        handlers_.resize(routeId + 1);
    }
    handlers_[routeId] = handlerPtr;
}

} // namespace nitrocoro::websocket
