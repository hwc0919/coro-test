/**
 * @file http_test.cc
 * @brief Tests for HttpServer, HttpClient, and HttpRouter.
 */
#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpRouter.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/net/InetAddress.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/Format.h>

#include <iomanip>
#include <sstream>

using namespace nitrocoro;
using namespace nitrocoro::http;
using namespace std::chrono_literals;

// ── Helpers ───────────────────────────────────────────────────────────────────

static SharedFuture<> start_server(HttpServer & server)
{
    Scheduler::current()->spawn([&server]() -> Task<> { co_await server.start(); });
    return server.started();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

/** GET route returns 200 with expected body. */
NITRO_TEST(http_get_hello)
{
    HttpServer server(0);
    server.route("/hello", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello world");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/hello", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello world");

    co_await server.stop();
}

/** POST route reads request body and echoes it back. */
NITRO_TEST(http_post_echo)
{
    HttpServer server(0);
    server.route("/echo", { "POST" }, [](auto req, auto resp) -> Task<> {
        auto complete = co_await req->toCompleteRequest();
        resp->setBody(complete.body());
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    auto resp = co_await client.post("/echo", "ping");
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "ping");

    co_await server.stop();
}

/** Unregistered route returns 404. */
NITRO_TEST(http_404)
{
    HttpServer server(0);
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/missing", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k404NotFound);

    co_await server.stop();
}

/** Handler can read query parameters from the request. */
NITRO_TEST(http_query_params)
{
    HttpServer server(0);
    server.route("/greet", { "GET" }, [](auto req, auto resp) {
        resp->setBody("Hello, " + req->getQuery("name") + "!");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/greet?name=World", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "Hello, World!");

    co_await server.stop();
}

/** Handler can read request headers; response headers are visible to client. */
NITRO_TEST(http_headers)
{
    HttpServer server(0);
    server.route("/headers", { "GET" }, [](auto req, auto resp) {
        const auto & ua = req->getHeader(HttpHeader::NameCode::UserAgent);
        resp->setHeader(HttpHeader::NameCode::ContentType, "text/plain");
        resp->setBody(ua.empty() ? "no-ua" : "has-ua");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/headers", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.getHeader(HttpHeader::NameCode::ContentType), "text/plain");

    co_await server.stop();
}

/** stop() causes start() to return cleanly. */
NITRO_TEST(http_server_stop)
{
    HttpServer server(0);
    bool stopped = false;

    Scheduler::current()->spawn([TEST_CTX, &server, &stopped]() -> Task<> {
        co_await server.start();
        stopped = true;
    });

    co_await server.started();
    co_await server.stop();
    co_await sleep(10ms);

    NITRO_CHECK(stopped);
}

/** Multiple sequential requests on the same server (keep-alive path). */
NITRO_TEST(http_multiple_requests)
{
    HttpServer server(0);
    int count = 0;
    server.route("/count", { "GET" }, [&count](auto req, auto resp) {
        resp->setBody(std::to_string(++count));
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    for (int i = 1; i <= 3; ++i)
    {
        auto resp = co_await client.get("/count");
        NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
        NITRO_CHECK_EQ(resp.body(), std::to_string(i));
    }

    co_await server.stop();
}

/** Shared router: two servers on different ports serve the same routes. */
NITRO_TEST(router_shared_across_servers)
{
    auto router = std::make_shared<HttpRouter>();
    router->addRoute("/ping", { "GET" }, [](auto req, auto resp) {
        resp->setBody("pong");
    });

    HttpServer s1({ .router = router });
    HttpServer s2({ .router = router });
    co_await start_server(s1);
    co_await start_server(s2);

    auto r1 = co_await get(utils::format("http://127.0.0.1:{}/ping", s1.listeningPort()));
    auto r2 = co_await get(utils::format("http://127.0.0.1:{}/ping", s2.listeningPort()));
    NITRO_CHECK_EQ(r1.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(r1.body(), "pong");
    NITRO_CHECK_EQ(r2.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(r2.body(), "pong");

    co_await s1.stop();
    co_await s2.stop();
}

/** Wrong method on a registered path returns 405. */
NITRO_TEST(router_method_mismatch_405)
{
    HttpServer server(0);
    server.route("/data", { "POST" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/data", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k405MethodNotAllowed);

    co_await server.stop();
}

/** Path with percent-encoded characters is decoded before routing. */
NITRO_TEST(http_path_percent_encoding)
{
    HttpServer server(0);
    server.route("/hello world", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->path());
    });
    co_await start_server(server);

    auto resp = co_await get(
        utils::format("http://127.0.0.1:{}/hello%20world", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "/hello world");

    co_await server.stop();
}

/** Query value with %20 and + are both decoded to space. */
NITRO_TEST(http_query_decode)
{
    HttpServer server(0);
    server.route("/q", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getQuery("a") + "|" + req->getQuery("b"));
    });
    co_await start_server(server);

    auto resp = co_await get(
        utils::format("http://127.0.0.1:{}/q?a=hello%20world&b=hello+world", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello world|hello world");

    co_await server.stop();
}

/** Path with invalid percent-encoded sequence is kept as-is. */
NITRO_TEST(http_path_invalid_encoding)
{
    HttpServer server(0);
    server.route("/foo%zz", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->path());
    });
    co_await start_server(server);

    auto resp = co_await get(
        utils::format("http://127.0.0.1:{}/foo%zz", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "/foo%zz");

    co_await server.stop();
}

/** HEAD request: no body in response, Content-Length matches GET. */
NITRO_TEST(http_head)
{
    HttpServer server(0);
    server.route("/data", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello world");
    });
    co_await start_server(server);

    auto getResp = co_await get(utils::format("http://127.0.0.1:{}/data", server.listeningPort()));
    NITRO_CHECK_EQ(getResp.statusCode(), StatusCode::k200OK);
    auto expectedLen = getResp.getHeader(HttpHeader::NameCode::ContentLength);

    ClientRequest headReq;
    headReq.setMethod(methods::Head);
    headReq.setPath("/data");
    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    auto headResp = co_await client.request(std::move(headReq));
    auto headComplete = co_await headResp.toCompleteResponse();
    NITRO_CHECK_EQ(headComplete.statusCode(), StatusCode::k200OK);
    NITRO_CHECK(headComplete.body().empty());
    NITRO_CHECK_EQ(headComplete.getHeader(HttpHeader::NameCode::ContentLength), expectedLen);

    co_await server.stop();
}

/** OPTIONS on a registered path returns 200 with Allow header. */
NITRO_TEST(http_options_registered_path)
{
    HttpServer server(0);
    server.route("/data", { "GET", "POST" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    ClientRequest optReq;
    optReq.setMethod(methods::Options);
    auto resp = co_await request(utils::format("http://127.0.0.1:{}/data", server.listeningPort()), std::move(optReq));
    auto complete = co_await resp.toCompleteResponse();
    NITRO_CHECK_EQ(complete.statusCode(), StatusCode::k200OK);
    NITRO_CHECK(complete.body().empty());
    auto allow = complete.getHeader(HttpHeader::NameCode::Allow);
    NITRO_CHECK(allow.find("GET") != std::string::npos);
    NITRO_CHECK(allow.find("POST") != std::string::npos);

    co_await server.stop();
}

/** OPTIONS on an unregistered path returns 404. */
NITRO_TEST(http_options_not_found)
{
    HttpServer server(0);
    co_await start_server(server);

    ClientRequest optReq;
    optReq.setMethod(methods::Options);
    auto resp = co_await request(
        utils::format("http://127.0.0.1:{}/missing", server.listeningPort()), std::move(optReq));
    auto complete = co_await resp.toCompleteResponse();
    NITRO_CHECK_EQ(complete.statusCode(), StatusCode::k404NotFound);

    co_await server.stop();
}

/** Handler throws before writing: server returns 500, connection closes, server keeps accepting. */
NITRO_TEST(http_handler_throws)
{
    HttpServer server(0);
    server.route("/throw", { "GET" }, [](auto req, auto resp) {
        throw std::runtime_error("handler error");
    });
    server.route("/ok", { "GET" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/throw", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k500InternalServerError);

    auto resp2 = co_await get(utils::format("http://127.0.0.1:{}/ok", server.listeningPort()));
    NITRO_CHECK_EQ(resp2.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp2.body(), "ok");

    co_await server.stop();
}


/** Chunked POST followed by GET on the same keep-alive connection. */
NITRO_TEST(http_chunked_keepalive)
{
    HttpServer server(0);
    server.route("/echo", { "POST" }, [](auto req, auto resp) -> Task<> {
        auto complete = co_await req->toCompleteRequest();
        resp->setBody(complete.body());
    });
    server.route("/ping", { "GET" }, [](auto req, auto resp) {
        resp->setBody("pong");
    });
    co_await start_server(server);

    auto conn = co_await net::TcpConnection::connect(
        net::InetAddress("127.0.0.1", server.listeningPort()));

    // Chunked POST
    std::string req1 = "POST /echo HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n"
                       "Transfer-Encoding: chunked\r\n"
                       "\r\n"
                       "5\r\nhello\r\n"
                       "0\r\n"
                       "\r\n";
    co_await conn->write(req1.data(), req1.size());

    // Read response 1
    std::string resp1;
    char buf[4096];
    while (resp1.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp1.append(buf, n);
    }
    auto clPos = resp1.find("Content-Length: ");
    NITRO_REQUIRE(clPos != std::string::npos);
    size_t cl = std::stoul(resp1.substr(clPos + 16));
    size_t headerEnd = resp1.find("\r\n\r\n") + 4;
    while (resp1.size() - headerEnd < cl)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp1.append(buf, n);
    }
    NITRO_REQUIRE(resp1.find("200 OK") != std::string::npos);
    NITRO_CHECK_EQ(resp1.substr(headerEnd, cl), "hello");

    // GET on same connection
    std::string req2 = "GET /ping HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n"
                       "\r\n";
    co_await conn->write(req2.data(), req2.size());

    std::string resp2;
    while (resp2.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp2.append(buf, n);
    }
    auto cl2Pos = resp2.find("Content-Length: ");
    NITRO_REQUIRE(cl2Pos != std::string::npos);
    size_t cl2 = std::stoul(resp2.substr(cl2Pos + 16));
    size_t headerEnd2 = resp2.find("\r\n\r\n") + 4;
    while (resp2.size() - headerEnd2 < cl2)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp2.append(buf, n);
    }
    NITRO_REQUIRE(resp2.find("200 OK") != std::string::npos);
    NITRO_CHECK_EQ(resp2.substr(headerEnd2, cl2), "pong");

    co_await server.stop();
}

/** Expect: 100-continue — server sends 100 before handler reads body. */
NITRO_TEST(http_expect_100_continue)
{
    HttpServer server(0);
    server.route("/upload", { "POST" }, [](auto req, auto resp) -> Task<> {
        auto complete = co_await req->toCompleteRequest();
        resp->setBody(complete.body());
    });
    co_await start_server(server);

    auto conn = co_await net::TcpConnection::connect(
        net::InetAddress("127.0.0.1", server.listeningPort()));

    std::string req = "POST /upload HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Length: 5\r\n"
                      "Expect: 100-continue\r\n"
                      "\r\n";
    co_await conn->write(req.data(), req.size());

    // Read 100 Continue
    char buf[4096];
    std::string interim;
    while (interim.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        interim.append(buf, n);
    }
    NITRO_REQUIRE(interim.find("100 Continue") != std::string::npos);

    // Send body
    std::string body = "hello";
    co_await conn->write(body.data(), body.size());

    // Read final response
    std::string resp;
    while (resp.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp.append(buf, n);
    }
    auto clPos = resp.find("Content-Length: ");
    NITRO_REQUIRE(clPos != std::string::npos);
    size_t cl = std::stoul(resp.substr(clPos + 16));
    size_t headerEnd = resp.find("\r\n\r\n") + 4;
    while (resp.size() - headerEnd < cl)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp.append(buf, n);
    }
    NITRO_CHECK(resp.find("200 OK") != std::string::npos);
    NITRO_CHECK_EQ(resp.substr(headerEnd, cl), "hello");

    co_await server.stop();
}

/** Expect: 100-continue — handler rejects with 413, connection stays alive. */
NITRO_TEST(http_expect_100_continue_rejected)
{
    HttpServer server(0);
    server.route("/upload", { "POST" }, [](auto req, auto resp) {
        resp->setStatus(StatusCode::k413RequestEntityTooLarge);
        resp->setBody("Too Large");
    });
    co_await start_server(server);

    auto conn = co_await net::TcpConnection::connect(
        net::InetAddress("127.0.0.1", server.listeningPort()));

    // First request with Expect
    std::string req1 = "POST /upload HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n"
                       "Content-Length: 5\r\n"
                       "Expect: 100-continue\r\n"
                       "\r\n";
    co_await conn->write(req1.data(), req1.size());

    // Read 100 then 413
    char buf[4096];
    std::string resp1;
    // Read until we have both 100 and the final response
    while (resp1.find("413") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp1.append(buf, n);
    }
    NITRO_CHECK(resp1.find("100 Continue") != std::string::npos);
    NITRO_CHECK(resp1.find("413") != std::string::npos);

    // Send body so server can drain and keep connection alive
    std::string body = "hello";
    co_await conn->write(body.data(), body.size());

    // Connection should still be usable
    std::string req2 = "GET /upload HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n"
                       "\r\n";
    co_await conn->write(req2.data(), req2.size());
    std::string resp2;
    while (resp2.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp2.append(buf, n);
    }
    NITRO_CHECK(resp2.find("405") != std::string::npos);

    co_await server.stop();
}

/** Unknown Expect value returns 417 Expectation Failed. */
NITRO_TEST(http_expect_unknown)
{
    HttpServer server(0);
    server.route("/upload", { "POST" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    auto conn = co_await net::TcpConnection::connect(
        net::InetAddress("127.0.0.1", server.listeningPort()));

    std::string req = "POST /upload HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Length: 5\r\n"
                      "Expect: unknown-extension\r\n"
                      "\r\n";
    co_await conn->write(req.data(), req.size());

    char buf[4096];
    std::string resp;
    while (resp.find("\r\n\r\n") == std::string::npos)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        resp.append(buf, n);
    }
    NITRO_CHECK(resp.find("417") != std::string::npos);

    co_await server.stop();
}

/** Date header: present with correct RFC 7231 format by default, absent when disabled. */
NITRO_TEST(http_date_header)
{
    {
        HttpServer server(0);
        server.route("/", { "GET" }, [](auto req, auto resp) {
            resp->setBody("ok");
        });
        co_await start_server(server);

        auto resp = co_await get(utils::format("http://127.0.0.1:{}/", server.listeningPort()));
        const auto & date = resp.getHeader(HttpHeader::NameCode::Date);
        NITRO_CHECK(!date.empty());
        std::tm tm{};
        std::istringstream ss(date);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        NITRO_CHECK(!ss.fail());
        co_await server.stop();
    }

    {
        HttpServer server({ .send_date_header = false });
        server.route("/", { "GET" }, [](auto req, auto resp) {
            resp->setBody("ok");
        });
        co_await start_server(server);

        auto resp = co_await get(utils::format("http://127.0.0.1:{}/", server.listeningPort()));
        NITRO_CHECK(resp.getHeader(HttpHeader::NameCode::Date).empty());
        co_await server.stop();
    }
}

/** Path parameter is accessible via req->pathParams() in handler. */
NITRO_TEST(http_path_params)
{
    HttpServer server(0);
    server.route("/users/:id/posts/:pid", { "GET" }, [](auto req, auto resp) {
        const auto & params = req->pathParams();
        resp->setBody(params.at("id") + "," + params.at("pid"));
    });
    co_await start_server(server);

    auto resp = co_await get(
        utils::format("http://127.0.0.1:{}/users/42/posts/99", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "42,99");

    co_await server.stop();
}

/** routeRegex: capture groups accessible via req->pathParams() as $1, $2. */
NITRO_TEST(http_route_regex)
{
    HttpServer server(0);
    server.routeRegex(R"(/items/(\d+))", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->pathParams().at("$1"));
    });
    co_await start_server(server);

    auto resp = co_await get(
        utils::format("http://127.0.0.1:{}/items/123", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "123");

    co_await server.stop();
}

/** Malformed request returns 400 and closes connection. */
NITRO_TEST(http_bad_request)
{
    HttpServer server(0);
    co_await start_server(server);

    auto conn = co_await net::TcpConnection::connect(
        net::InetAddress("127.0.0.1", server.listeningPort()));

    std::string req = "BADREQUEST\r\n";
    co_await conn->write(req.data(), req.size());

    char buf[4096];
    std::string received;
    while (true)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        if (n == 0)
            break;
        received.append(buf, n);
    }
    NITRO_CHECK(received.find("400") != std::string::npos);

    co_await server.stop();
}

/** Two consecutive requests on the same HttpClient reuse the same connection. */
NITRO_TEST(http_client_keep_alive)
{
    std::atomic<int> connectionCount{ 0 };

    HttpServer server(0);
    server.route("/ping", { "GET" }, [](auto req, auto resp) {
        resp->setBody("pong");
    });
    server.setStreamUpgrader([&connectionCount](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        ++connectionCount;
        co_return std::make_shared<io::Stream>(conn);
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    co_await client.get("/ping");
    co_await client.get("/ping");

    NITRO_CHECK_EQ(connectionCount.load(), 1);

    co_await server.stop();
}

/** Server responds with Connection: close; client opens a new connection for the next request. */
NITRO_TEST(http_client_connection_close)
{
    std::atomic<int> connectionCount{ 0 };

    HttpServer server(0);
    server.route("/ping", { "GET" }, [](auto req, auto resp) {
        resp->setHeader(HttpHeader::NameCode::Connection, "close");
        resp->setBody("pong");
    });
    server.setStreamUpgrader([&connectionCount](net::TcpConnectionPtr conn) -> Task<io::StreamPtr> {
        ++connectionCount;
        co_return std::make_shared<io::Stream>(conn);
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    co_await client.get("/ping");
    co_await client.get("/ping");

    NITRO_CHECK_EQ(connectionCount.load(), 2);

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
