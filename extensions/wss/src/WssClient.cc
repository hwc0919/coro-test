/**
 * @file WssClient.cc
 * @brief WSS client implementation
 */
#include <nitrocoro/tls/TlsContext.h>
#include <nitrocoro/tls/TlsStream.h>
#include <nitrocoro/wss/WssClient.h>

namespace nitrocoro::wss
{

static websocket::WsClient::StreamUpgrader makeUpgrader(const net::Url & url, WssClientConfig config)
{
    tls::TlsPolicy policy = std::move(config.tlsPolicy);
    if (policy.hostname.empty())
        policy.hostname = url.host();
    auto tlsContext = tls::TlsContext::createClient(policy);
    return [tlsContext](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        auto tlsStream = co_await tls::TlsStream::connect(std::move(conn), tlsContext);
        co_return std::make_shared<io::Stream>(std::move(tlsStream));
    };
}

WssClient::WssClient(std::string url, WssClientConfig config)
    : WssClient(net::Url(std::move(url)), std::move(config))
{
}

WssClient::WssClient(net::Url url, WssClientConfig config)
    : websocket::WsClient(url)
{
    setStreamUpgrader(makeUpgrader(url, std::move(config)));
}

} // namespace nitrocoro::wss
