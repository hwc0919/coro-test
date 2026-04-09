/**
 * @file wss_test.cc
 * @brief Tests for WssClient (WebSocket over TLS)
 */
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/io/Stream.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/tls/TlsContext.h>
#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/tls/TlsStream.h>
#include <nitrocoro/websocket/WsServer.h>
#include <nitrocoro/wss/WssClient.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

using namespace nitrocoro;
using namespace nitrocoro::websocket;
using namespace nitrocoro::wss;
using namespace nitrocoro::tls;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::pair<std::string, std::string> makeTestCert(const std::string & cn)
{
    EVP_PKEY_CTX * kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY_keygen_init(kctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(kctx, NID_X9_62_prime256v1);
    EVP_PKEY * pkey = nullptr;
    EVP_PKEY_keygen(kctx, &pkey);
    EVP_PKEY_CTX_free(kctx);

    X509 * x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 365 * 24 * 3600);
    X509_set_pubkey(x509, pkey);

    X509_NAME * name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>(cn.c_str()), -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha256());

    auto bioToStr = [](BIO * bio) {
        BUF_MEM * mem;
        BIO_get_mem_ptr(bio, &mem);
        std::string s(mem->data, mem->length);
        BIO_free(bio);
        return s;
    };

    BIO * certBio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(certBio, x509);
    std::string certPem = bioToStr(certBio);

    BIO * keyBio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    std::string keyPem = bioToStr(keyBio);

    X509_free(x509);
    EVP_PKEY_free(pkey);

    return { certPem, keyPem };
}

static http::HttpServer makeWssServer()
{
    auto [certPem, keyPem] = makeTestCert("localhost");
    TlsPolicy sp;
    sp.certPem = certPem;
    sp.keyPem = keyPem;
    sp.validate = false;
    auto serverCtx = TlsContext::createServer(sp);

    http::HttpServer server(0);
    server.setStreamUpgrader([serverCtx](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        auto tlsStream = co_await TlsStream::accept(conn, serverCtx);
        if (!tlsStream)
            co_return nullptr;
        co_return std::make_shared<io::Stream>(tlsStream);
    });
    return server;
}

static WssClientConfig noVerifyConfig()
{
    WssClientConfig config;
    config.tlsPolicy.validate = false;
    return config;
}

static std::string wssUrl(uint16_t port, const std::string & path)
{
    return "wss://127.0.0.1:" + std::to_string(port) + path;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

/** WssClient connects and exchanges messages over TLS. */
NITRO_TEST(wss_echo)
{
    auto server = makeWssServer();
    WsServer ws;
    ws.route("/ws", [](WsContextPtr wsCtx) -> Task<> {
        auto conn = co_await wsCtx->accept();
        while (auto msg = co_await conn.receive())
            co_await conn.send(msg->payload);
    });
    ws.attachTo(server);

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    WssClient client(wssUrl(server.listeningPort(), "/ws"), noVerifyConfig());
    auto conn = co_await client.connect();

    co_await conn.send("hello wss");
    auto msg = co_await conn.receive();
    NITRO_REQUIRE(msg.has_value());
    NITRO_CHECK_EQ(msg->payload, "hello wss");

    co_await server.stop();
}

/** WssClient rejects a server with an untrusted certificate when validate=true. */
NITRO_TEST(wss_bad_cert_rejected)
{
    auto server = makeWssServer();
    WsServer ws;
    ws.route("/ws", [](WsContextPtr wsCtx) -> Task<> {
        auto conn = co_await wsCtx->accept();
        co_await conn.shutdown();
    });
    ws.attachTo(server);

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    WssClientConfig config;
    config.tlsPolicy.validate = true;
    config.tlsPolicy.useSystemCertStore = false;
    WssClient client(wssUrl(server.listeningPort(), "/ws"), std::move(config));

    NITRO_CHECK_THROWS(co_await client.connect());

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
