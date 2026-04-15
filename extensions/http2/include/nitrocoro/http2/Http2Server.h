/**
 * @file Http2Server.h
 * @brief HTTP/2 support — attaches to HttpServer via StreamUpgrader
 */
#pragma once

#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/tls/TlsPolicy.h>

namespace nitrocoro::http2
{

/** Attach HTTP/2 over TLS to an existing HttpServer.
 *  ALPN is automatically configured as {"h2", "http/1.1"} for fallback support. */
void enableHttp2(http::HttpServer & server, tls::TlsPolicy policy);

/** Attach h2c plaintext HTTP/2 to an existing HttpServer. */
void enableHttp2(http::HttpServer & server);

} // namespace nitrocoro::http2
