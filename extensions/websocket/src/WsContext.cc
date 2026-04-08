/**
 * @file WsContext.cc
 * @brief WsContext implementation
 */
#include <nitrocoro/websocket/WsContext.h>

namespace nitrocoro::websocket
{

WsContext::WsContext(http::IncomingRequestPtr req, http::ServerResponsePtr resp, Future<WsConnection> connFuture)
    : req(std::move(req)), resp(std::move(resp)), connFuture_(std::move(connFuture))
{
}

WsContext::~WsContext()
{
    if (!accepted_.test_and_set())
    {
        acceptPromise_.set_value(false);
    }
}

Task<WsConnection> WsContext::accept()
{
    accepted_.test_and_set();
    acceptPromise_.set_value(true);
    co_return co_await connFuture_.get();
}

} // namespace nitrocoro::websocket
