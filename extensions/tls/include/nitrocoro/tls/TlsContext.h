/**
 * @file TlsContext.h
 * @brief Pre-built TLS context shared across connections
 */
#pragma once

#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/tls/TlsProvider.h>

#include <functional>
#include <memory>
#include <string_view>

namespace nitrocoro::tls
{

class TlsContext;
using TlsContextPtr = std::shared_ptr<TlsContext>;

class TlsContext
{
public:
    static TlsContextPtr createServer(const TlsPolicy & policy);
    static TlsContextPtr createClient(const TlsPolicy & policy);

    virtual ~TlsContext() = default;

    TlsContext(const TlsContext &) = delete;
    TlsContext & operator=(const TlsContext &) = delete;

    /** Create a per-connection provider instance. */
    virtual std::unique_ptr<TlsProvider> newProvider() = 0;

    /**
     * @brief Optional SNI-based context selector (server side).
     *
     * If set, called during handshake with the client's SNI hostname.
     * Return a different TlsContext to switch certificates, or nullptr
     * to keep the current one.
     */
    std::function<TlsContextPtr(std::string_view sni)> sniResolver;

protected:
    TlsContext() = default;
};

} // namespace nitrocoro::tls
