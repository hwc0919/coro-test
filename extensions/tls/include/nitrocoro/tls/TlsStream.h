/**
 * @file TlsStream.h
 * @brief Coroutine TLS stream wrapping a TcpConnection
 */
#pragma once

#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/tls/TlsContext.h>

#include <string>
#include <vector>

namespace nitrocoro::tls
{

class TlsStream;
using TlsStreamPtr = std::shared_ptr<TlsStream>;

class TlsStream
{
public:
    /** Server-side: perform TLS accept handshake over an existing TCP connection. */
    static Task<TlsStreamPtr> accept(net::TcpConnectionPtr conn, TlsContextPtr ctx);

    /** Client-side: perform TLS connect handshake over an existing TCP connection. */
    static Task<TlsStreamPtr> connect(net::TcpConnectionPtr conn, TlsContextPtr ctx);

    TlsStream(const TlsStream &) = delete;
    TlsStream & operator=(const TlsStream &) = delete;
    TlsStream(TlsStream &&) = delete;
    TlsStream & operator=(TlsStream &&) = delete;

    ~TlsStream() = default;

    Task<size_t> read(void * buf, size_t len);
    Task<size_t> write(const void * buf, size_t len);
    Task<> shutdown();
    bool peerClosed() const { return conn_->peerClosed(); }

    std::string sniName() const;
    std::string negotiatedAlpn() const;

private:
    TlsStream(net::TcpConnectionPtr conn, std::unique_ptr<TlsProvider> provider);

    /** Read one TCP chunk → feedEncrypted → flush encOut. Returns false on EOF/error. */
    Task<FeedResult> feedOnce();

    /** Flush encryptedOutBuf_ to TCP. Returns false if peer closed. */
    Task<bool> flushEncrypted();

    net::TcpConnectionPtr conn_;
    std::unique_ptr<TlsProvider> provider_;

    std::vector<char> plainBuf_;      // leftover decrypted data
    std::vector<char> encryptedOutBuf_; // pending ciphertext to send
    bool eof_{ false };

    Mutex writeMutex_;
};

} // namespace nitrocoro::tls
