/**
 * @file Http2Server.cc
 */
#include <nitrocoro/http2/Http2Server.h>

#include "Http2Session.h"
#include <nitrocoro/io/Stream.h>
#include <nitrocoro/tls/TlsStream.h>

namespace nitrocoro::http2
{

void enableHttp2(http::HttpServer & server, tls::TlsPolicy policy)
{
    policy.alpn = { "h2", "http/1.1" };
    auto ctx = tls::TlsContext::createServer(policy);
    auto router = server.router();
    server.setStreamUpgrader([ctx, router](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        auto tlsStream = co_await tls::TlsStream::accept(conn, ctx);
        if (tlsStream->negotiatedAlpn() == "h2")
        {
            auto stream = std::make_shared<io::Stream>(std::move(tlsStream));
            auto session = std::make_shared<Http2Session>(std::move(stream), router, Scheduler::current());
            co_await session->run();
            co_return nullptr;
        }
        co_return std::make_shared<io::Stream>(std::move(tlsStream));
    });
}

void enableHttp2(http::HttpServer & server)
{
    auto router = server.router();
    server.setStreamUpgrader([router](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        auto stream = std::make_shared<io::Stream>(conn);
        auto session = std::make_shared<Http2Session>(stream, router, Scheduler::current());
        co_await session->run();
        co_return nullptr;
    });
}

} // namespace nitrocoro::http2
