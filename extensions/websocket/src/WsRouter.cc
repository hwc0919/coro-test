/**
 * @file WsRouter.cc
 * @brief WebSocket router implementation
 */
#include <nitrocoro/websocket/WsRouter.h>

namespace nitrocoro::websocket
{

WsRouter::RouteResult WsRouter::route(const std::string & path) const
{
    // WebSocket upgrade always uses GET method
    auto result = core_.match(http::methods::Get, path);

    if (result.matched)
    {
        if (result.routeId < handlers_.size() && handlers_[result.routeId])
        {
            return { handlers_[result.routeId], std::move(result.params) };
        }
    }

    return {};
}

} // namespace nitrocoro::websocket
