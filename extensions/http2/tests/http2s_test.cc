/**
 * @file http2s_test.cc
 * @brief Tests for HTTP/2 over TLS (h2s) and HTTP/1.1 fallback via enableHttp2.
 */
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/http2/Http2Server.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/tls/TlsContext.h>
#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/tls/TlsStream.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstring>
#include <optional>
#include <unordered_map>

using namespace nitrocoro;
using namespace nitrocoro::http;
using namespace nitrocoro::http2;
using namespace nitrocoro::tls;
using namespace nitrocoro::net;
using namespace std::chrono_literals;

// ── Cert helpers ─────────────────────────────────────────────────────────────

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

static std::pair<TlsContextPtr, TlsContextPtr> makeContexts(std::vector<std::string> serverAlpn,
                                                            std::vector<std::string> clientAlpn)
{
    auto [certPem, keyPem] = makeTestCert("localhost");

    TlsPolicy sp;
    sp.certPem = certPem;
    sp.keyPem = keyPem;
    sp.validate = false;
    sp.alpn = std::move(serverAlpn);
    auto serverCtx = TlsContext::createServer(sp);

    TlsPolicy cp;
    cp.hostname = "localhost";
    cp.validate = false;
    cp.alpn = std::move(clientAlpn);
    auto clientCtx = TlsContext::createClient(cp);

    return { serverCtx, clientCtx };
}

// ── Minimal h2 client helpers ─────────────────────────────────────────────────

static Task<> writeExact(TlsStreamPtr tls, const void * buf, size_t len)
{
    size_t sent = 0;
    const auto * p = static_cast<const uint8_t *>(buf);
    while (sent < len)
    {
        size_t n = co_await tls->write(p + sent, len - sent);
        if (n == 0)
            throw std::runtime_error("connection closed during write");
        sent += n;
    }
}

static Task<> readExact(TlsStreamPtr tls, void * buf, size_t len)
{
    size_t got = 0;
    auto * p = static_cast<uint8_t *>(buf);
    while (got < len)
    {
        size_t n = co_await tls->read(p + got, len - got);
        if (n == 0)
            throw std::runtime_error("connection closed during read");
        got += n;
    }
}

struct RawFrameHeader
{
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint32_t streamId;
};

static Task<RawFrameHeader> readFrameHeader(TlsStreamPtr tls)
{
    uint8_t hdr[9];
    co_await readExact(tls, hdr, 9);
    RawFrameHeader f;
    f.length = (static_cast<uint32_t>(hdr[0]) << 16) | (static_cast<uint32_t>(hdr[1]) << 8) | hdr[2];
    f.type = hdr[3];
    f.flags = hdr[4];
    f.streamId = ((static_cast<uint32_t>(hdr[5]) & 0x7f) << 24) | (static_cast<uint32_t>(hdr[6]) << 16)
                 | (static_cast<uint32_t>(hdr[7]) << 8) | hdr[8];
    co_return f;
}

static Task<std::vector<uint8_t>> readFramePayload(TlsStreamPtr tls, uint32_t len)
{
    std::vector<uint8_t> buf(len);
    if (len > 0)
        co_await readExact(tls, buf.data(), len);
    co_return buf;
}

static void writeFrameRaw(std::vector<uint8_t> & out, uint8_t type, uint8_t flags,
                          uint32_t streamId, const void * payload, size_t payLen)
{
    out.push_back((payLen >> 16) & 0xff);
    out.push_back((payLen >> 8) & 0xff);
    out.push_back(payLen & 0xff);
    out.push_back(type);
    out.push_back(flags);
    out.push_back((streamId >> 24) & 0x7f);
    out.push_back((streamId >> 16) & 0xff);
    out.push_back((streamId >> 8) & 0xff);
    out.push_back(streamId & 0xff);
    if (payload)
        out.insert(out.end(), static_cast<const uint8_t *>(payload),
                   static_cast<const uint8_t *>(payload) + payLen);
}

static void hpackLiteral(std::vector<uint8_t> & out, std::string_view name, std::string_view value)
{
    out.push_back(0x00);
    out.push_back(static_cast<uint8_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());
    out.push_back(static_cast<uint8_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

static std::vector<uint8_t> buildGetHeaders(std::string_view path, std::string_view authority)
{
    std::vector<uint8_t> block;
    block.push_back(0x82); // :method GET
    if (path == "/")
        block.push_back(0x84); // :path /
    else
        hpackLiteral(block, ":path", path);
    block.push_back(0x86); // :scheme http
    hpackLiteral(block, ":authority", authority);
    return block;
}

struct StreamResponse
{
    std::vector<uint8_t> headerBlock;
    std::string body;
};

class H2Client
{
public:
    explicit H2Client(TlsStreamPtr tls)
        : tls_(std::move(tls))
    {
        Scheduler::current()->spawn([this]() -> Task<> { co_await pump(); });
    }

    Task<> send(std::vector<uint8_t> frame)
    {
        co_await writeExact(tls_, frame.data(), frame.size());
    }

    Task<StreamResponse> recv(uint32_t streamId)
    {
        auto & entry = streams_[streamId];
        if (!entry.promise)
            entry.promise.emplace(Scheduler::current());
        co_await entry.promise->get_future().get();
        co_return std::move(entry.response);
    }

private:
    struct StreamEntry
    {
        StreamResponse response;
        std::optional<Promise<>> promise;
    };

    Task<> pump()
    {
        std::vector<uint8_t> buf;
        for (;;)
        {
            RawFrameHeader fh;
            try
            {
                fh = co_await readFrameHeader(tls_);
            }
            catch (...)
            {
                break;
            }
            auto payload = co_await readFramePayload(tls_, fh.length);

            if (fh.type == 0x4 && !(fh.flags & 0x1)) // SETTINGS (not ACK)
            {
                buf.clear();
                writeFrameRaw(buf, 0x4, 0x1, 0, nullptr, 0);
                co_await writeExact(tls_, buf.data(), buf.size());
                continue;
            }

            if (fh.streamId == 0)
                continue;

            auto & entry = streams_[fh.streamId];
            if (!entry.promise)
                entry.promise.emplace(Scheduler::current());

            if (fh.type == 0x1)
                entry.response.headerBlock = std::move(payload);
            if (fh.type == 0x0)
                for (uint8_t b : payload)
                    entry.response.body.push_back(static_cast<char>(b));

            if (fh.flags & 0x1)
                entry.promise->set_value();
        }
    }

    TlsStreamPtr tls_;
    std::unordered_map<uint32_t, StreamEntry> streams_;
};

static Task<std::shared_ptr<H2Client>> h2sConnect(uint16_t port, TlsContextPtr clientCtx)
{
    auto conn = co_await TcpConnection::connect({ "127.0.0.1", port });
    auto tls = co_await TlsStream::connect(conn, clientCtx);

    std::string_view preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    co_await writeExact(tls, preface.data(), preface.size());

    std::vector<uint8_t> buf;
    writeFrameRaw(buf, 0x4, 0, 0, nullptr, 0); // client SETTINGS
    co_await writeExact(tls, buf.data(), buf.size());

    // Read and ACK server SETTINGS
    auto sh = co_await readFrameHeader(tls);
    co_await readFramePayload(tls, sh.length);
    buf.clear();
    writeFrameRaw(buf, 0x4, 0x1, 0, nullptr, 0);
    co_await writeExact(tls, buf.data(), buf.size());

    co_return std::make_shared<H2Client>(std::move(tls));
}

static std::vector<uint8_t> makeGetFrame(uint32_t streamId, std::string_view path,
                                         std::string_view authority)
{
    auto headerBlock = buildGetHeaders(path, authority);
    std::vector<uint8_t> buf;
    writeFrameRaw(buf, 0x1, 0x5, streamId, headerBlock.data(), headerBlock.size());
    return buf;
}

// ── h2s tests ─────────────────────────────────────────────────────────────────

NITRO_TEST(h2s_get)
{
    auto [serverCtx, clientCtx] = makeContexts({ "h2" }, { "h2" });

    HttpServer server(0);
    enableHttp2(server, serverCtx);
    server.route("/hello", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello h2s");
    });

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    auto port = server.listeningPort();
    std::string host = "127.0.0.1:" + std::to_string(port);
    auto client = co_await h2sConnect(port, clientCtx);
    co_await client->send(makeGetFrame(1, "/hello", host));
    auto r = co_await client->recv(1);

    NITRO_CHECK(!r.headerBlock.empty());
    NITRO_CHECK_EQ(r.body, "hello h2s");

    co_await server.stop();
}

NITRO_TEST(h2s_path_params)
{
    auto [serverCtx, clientCtx] = makeContexts({ "h2" }, { "h2" });

    HttpServer server(0);
    enableHttp2(server, serverCtx);
    server.route("/users/:id", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->pathParams().at("id"));
    });

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    auto port = server.listeningPort();
    std::string host = "127.0.0.1:" + std::to_string(port);
    auto client = co_await h2sConnect(port, clientCtx);
    co_await client->send(makeGetFrame(1, "/users/42", host));
    auto r = co_await client->recv(1);

    NITRO_CHECK_EQ(r.body, "42");

    co_await server.stop();
}

NITRO_TEST(h2s_multiple_streams)
{
    auto [serverCtx, clientCtx] = makeContexts({ "h2" }, { "h2" });

    HttpServer server(0);
    enableHttp2(server, serverCtx);
    server.route("/a", { "GET" }, [](auto req, auto resp) { resp->setBody("aaa"); });
    server.route("/b", { "GET" }, [](auto req, auto resp) { resp->setBody("bbb"); });

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    auto port = server.listeningPort();
    std::string host = "127.0.0.1:" + std::to_string(port);
    auto client = co_await h2sConnect(port, clientCtx);

    co_await client->send(makeGetFrame(1, "/a", host));
    co_await client->send(makeGetFrame(3, "/b", host));

    auto r1 = co_await client->recv(1);
    auto r3 = co_await client->recv(3);

    NITRO_CHECK_EQ(r1.body, "aaa");
    NITRO_CHECK_EQ(r3.body, "bbb");

    co_await server.stop();
}

// ── Fallback tests ────────────────────────────────────────────────────────────

// Client negotiates "http/1.1" → falls back, HTTP/1.1 request succeeds.
NITRO_TEST(h2s_fallback_http1)
{
    auto [serverCtx, clientCtx] = makeContexts({ "h2", "http/1.1" }, { "http/1.1" });

    HttpServer server(0);
    enableHttp2(server, serverCtx);
    server.route("/", { "GET" }, [](auto req, auto resp) {
        resp->setBody("http1");
    });

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    auto conn = co_await TcpConnection::connect({ "127.0.0.1", server.listeningPort() });
    auto tls = co_await TlsStream::connect(conn, clientCtx);
    NITRO_CHECK_EQ(tls->negotiatedAlpn(), std::string("http/1.1"));

    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    co_await tls->write(req.data(), req.size());

    std::string resp;
    char buf[4096];
    size_t n;
    while ((n = co_await tls->read(buf, sizeof(buf))) > 0)
        resp.append(buf, n);

    NITRO_CHECK(resp.find("http1") != std::string::npos);
    co_await server.stop();
}

// Client negotiates "h2" while server supports both → h2 selected, not fallback.
NITRO_TEST(h2s_fallback_h2_preferred)
{
    auto [serverCtx, clientCtx] = makeContexts({ "h2", "http/1.1" }, { "h2" });

    HttpServer server(0);
    enableHttp2(server, serverCtx);
    server.route("/", { "GET" }, [](auto req, auto resp) {
        resp->setBody("h2ok");
    });

    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    auto port = server.listeningPort();
    std::string host = "127.0.0.1:" + std::to_string(port);
    auto client = co_await h2sConnect(port, clientCtx);
    co_await client->send(makeGetFrame(1, "/", host));
    auto r = co_await client->recv(1);

    NITRO_CHECK_EQ(r.body, "h2ok");

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
