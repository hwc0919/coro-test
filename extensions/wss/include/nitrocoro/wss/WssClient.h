/**
 * @file WssClient.h
 * @brief WSS client — combines nitrocoro-websocket and nitrocoro-tls
 */
#pragma once
#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/websocket/WsClient.h>

namespace nitrocoro::wss
{

struct WssClientConfig
{
    tls::TlsPolicy tlsPolicy = tls::TlsPolicy::defaultClient();
};

class WssClient : public websocket::WsClient
{
public:
    explicit WssClient(std::string url, WssClientConfig config = {});
    explicit WssClient(net::Url url, WssClientConfig config = {});
};

} // namespace nitrocoro::wss
