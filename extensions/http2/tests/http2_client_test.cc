/**
 * @file http2_client_test.cc
 * @brief Tests for Http2Client against a local Http2Server
 */
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/http2/Http2Client.h>
#include <nitrocoro/http2/Http2Server.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/tls/TlsContext.h>
#include <nitrocoro/tls/TlsPolicy.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

using namespace nitrocoro;
using namespace nitrocoro::http;
using namespace nitrocoro::http2;
using namespace nitrocoro::tls;

static std::pair<std::string, std::string> makeTestCert()
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
                               reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
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

NITRO_TEST(http2client_get)
{
    auto [certPem, keyPem] = makeTestCert();

    TlsPolicy serverPolicy;
    serverPolicy.certPem = certPem;
    serverPolicy.keyPem = keyPem;
    serverPolicy.validate = false;

    HttpServer server(0);
    enableHttp2(server, serverPolicy);
    server.route("/hello", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello from h2");
    });
    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    Http2ClientConfig config;
    config.tls_policy.hostname = "localhost";
    config.tls_policy.validate = false;

    Http2Client client("https://localhost:" + std::to_string(server.listeningPort()), config);
    auto resp = co_await client.get("/hello");

    NITRO_CHECK_EQ(resp.statusCode(), 200);
    NITRO_CHECK_EQ(resp.body(), "hello from h2");

    co_await server.stop();
}

NITRO_TEST(http2client_post)
{
    auto [certPem, keyPem] = makeTestCert();

    TlsPolicy serverPolicy;
    serverPolicy.certPem = certPem;
    serverPolicy.keyPem = keyPem;
    serverPolicy.validate = false;

    HttpServer server(0);
    enableHttp2(server, serverPolicy);
    server.route("/echo", { "POST" }, [](auto req, auto resp) -> Task<> {
        auto complete = co_await req->toCompleteRequest();
        resp->setBody(complete.body());
    });
    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    Http2ClientConfig config;
    config.tls_policy.hostname = "localhost";
    config.tls_policy.validate = false;

    Http2Client client("https://localhost:" + std::to_string(server.listeningPort()), config);

    http::ClientRequest req;
    req.setMethod(http::methods::Post);
    req.setPath("/echo");
    req.setBody("hello body");
    auto resp = co_await client.request(std::move(req));
    auto complete = co_await resp.toCompleteResponse();

    NITRO_CHECK_EQ(complete.statusCode(), 200);
    NITRO_CHECK_EQ(complete.body(), "hello body");

    co_await server.stop();
}

NITRO_TEST(http2client_404)
{
    auto [certPem, keyPem] = makeTestCert();

    TlsPolicy serverPolicy;
    serverPolicy.certPem = certPem;
    serverPolicy.keyPem = keyPem;
    serverPolicy.validate = false;

    HttpServer server(0);
    enableHttp2(server, serverPolicy);
    Scheduler::current()->spawn([&]() -> Task<> { co_await server.start(); });
    co_await server.started();

    Http2ClientConfig config;
    config.tls_policy.hostname = "localhost";
    config.tls_policy.validate = false;

    Http2Client client("https://localhost:" + std::to_string(server.listeningPort()), config);
    auto resp = co_await client.get("/missing");

    NITRO_CHECK_EQ(resp.statusCode(), 404);

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
