/**
 * @file http_client_test.cc
 * @brief Integration tests for HttpClient
 */
#include <nitrocoro/http/Cookie.h>
#include <nitrocoro/http/CookieStore.h>
#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/Format.h>

using namespace nitrocoro;
using namespace nitrocoro::http;
using namespace std::chrono_literals;

static SharedFuture<> start_server(HttpServer & server)
{
    Scheduler::current()->spawn([&server]() -> Task<> { co_await server.start(); });
    return server.started();
}

// ── Free functions ───────────────────────────────────────────────────────────

/** http::get free function returns 200 with body. */
NITRO_TEST(http_free_get)
{
    HttpServer server(0);
    server.route("/hello", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/hello", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello");

    co_await server.stop();
}

/** http::request free function with custom method and headers. */
NITRO_TEST(http_free_request)
{
    HttpServer server(0);
    server.route("/data", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getHeader("X-Custom"));
    });
    co_await start_server(server);

    ClientRequest req;
    req.setMethod(methods::Get);
    req.setHeader("X-Custom", "value123");
    auto resp = co_await request(
        utils::format("http://127.0.0.1:{}/data", server.listeningPort()), std::move(req));
    auto complete = co_await resp.toCompleteResponse();
    NITRO_CHECK_EQ(complete.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(complete.body(), "value123");

    co_await server.stop();
}

// ── HttpClient instance ───────────────────────────────────────────────────────

/** HttpClient::get returns 200 with body. */
NITRO_TEST(http_client_get)
{
    HttpServer server(0);
    server.route("/hello", { "GET" }, [](auto req, auto resp) {
        resp->setBody("hello");
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    auto resp = co_await client.get("/hello");
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello");

    co_await server.stop();
}

/** HttpClient::post sends body; server echoes it back. */
NITRO_TEST(http_client_post)
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

/** HttpClient::request with streaming response body. */
NITRO_TEST(http_client_request_stream)
{
    HttpServer server(0);
    server.route("/data", { "GET" }, [](auto req, auto resp) {
        resp->setBody("streamed");
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    ClientRequest req;
    req.setMethod(methods::Get);
    req.setPath("/data");
    auto resp = co_await client.request(std::move(req));
    auto complete = co_await resp.toCompleteResponse();
    NITRO_CHECK_EQ(complete.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(complete.body(), "streamed");

    co_await server.stop();
}

/** HttpClient with invalid base URL throws on construction. */
NITRO_TEST(http_client_invalid_url)
{
    NITRO_CHECK_THROWS(HttpClient("not-a-url"));
    co_return;
}

// ── Cookie send/receive ───────────────────────────────────────────────────────

/** Server sets a cookie; client receives it in the response. */
NITRO_TEST(http_client_server_set_cookie)
{
    HttpServer server(0);
    server.route("/set", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{
            .name = "session",
            .value = "abc123",
            .path = "/",
            .httpOnly = true,
        });
        resp->setBody("ok");
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/set", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), 200);
    NITRO_REQUIRE_EQ(resp.cookies().size(), 1);
    NITRO_CHECK_EQ(resp.cookies()[0].name, "session");
    NITRO_CHECK_EQ(resp.cookies()[0].value, "abc123");
    NITRO_CHECK_EQ(resp.cookies()[0].path, "/");
    NITRO_CHECK(resp.cookies()[0].httpOnly);

    co_await server.stop();
}

/** Client manually sets a cookie on the request; server receives it. */
NITRO_TEST(http_client_send_cookie)
{
    HttpServer server(0);
    server.route("/echo", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("token"));
    });
    co_await start_server(server);

    ClientRequest req;
    req.setMethod(methods::Get);
    req.setCookie("token", "secret");
    auto resp = co_await request(
        utils::format("http://127.0.0.1:{}/echo", server.listeningPort()), std::move(req));
    auto complete = co_await resp.toCompleteResponse();
    NITRO_CHECK_EQ(complete.statusCode(), 200);
    NITRO_CHECK_EQ(complete.body(), "secret");

    co_await server.stop();
}

/** Server sets multiple cookies; all are visible in the response. */
NITRO_TEST(http_client_multiple_set_cookies)
{
    HttpServer server(0);
    server.route("/multi", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "a", .value = "1" });
        resp->addCookie(Cookie{ .name = "b", .value = "2", .secure = true });
        resp->addCookie(Cookie{ .name = "c", .value = "3", .sameSite = Cookie::SameSite::Strict });
    });
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/multi", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), 200);
    NITRO_REQUIRE_EQ(resp.cookies().size(), 3);
    NITRO_CHECK_EQ(resp.cookies()[0].name, "a");
    NITRO_CHECK_EQ(resp.cookies()[1].name, "b");
    NITRO_CHECK(resp.cookies()[1].secure);
    NITRO_CHECK_EQ(resp.cookies()[2].name, "c");
    NITRO_CHECK(resp.cookies()[2].sameSite == Cookie::SameSite::Strict);

    co_await server.stop();
}

// ── CookieStore integration ───────────────────────────────────────────────────

/** Server sets a cookie; client stores it and sends it on the next request. */
NITRO_TEST(http_client_cookie_store_and_send)
{
    HttpServer server(0);
    server.route("/set", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "session", .value = "abc123", .path = "/" });
        resp->setBody("ok");
    });
    server.route("/check", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("session"));
    });
    co_await start_server(server);

    HttpClientConfig config;
    config.cookieStoreFactory = memoryCookieStore();
    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()), std::move(config));

    co_await client.get("/set");
    auto resp = co_await client.get("/check");
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "abc123");

    co_await server.stop();
}

/** Cookie with path=/api is not sent for requests to /other. */
NITRO_TEST(http_client_cookie_path_not_sent)
{
    HttpServer server(0);
    server.route("/set", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "token", .value = "secret", .path = "/api" });
        resp->setBody("ok");
    });
    server.route("/other", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("token"));
    });
    co_await start_server(server);

    HttpClientConfig config;
    config.cookieStoreFactory = memoryCookieStore();
    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()), std::move(config));

    co_await client.get("/set");
    auto resp = co_await client.get("/other");
    NITRO_CHECK_EQ(resp.body(), "");

    co_await server.stop();
}

/** Secure cookie is not sent over plain HTTP. */
NITRO_TEST(http_client_secure_cookie_not_sent_over_http)
{
    HttpServer server(0);
    server.route("/set", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "secure_token", .value = "secret", .path = "/", .secure = true });
        resp->setBody("ok");
    });
    server.route("/check", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("secure_token"));
    });
    co_await start_server(server);

    HttpClientConfig config;
    config.cookieStoreFactory = memoryCookieStore();
    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()), std::move(config));

    co_await client.get("/set");
    auto resp = co_await client.get("/check");
    NITRO_CHECK_EQ(resp.body(), "");

    co_await server.stop();
}

/** Cookie is updated when server sends a new value for the same name+path. */
NITRO_TEST(http_client_cookie_overwrite)
{
    HttpServer server(0);
    int count = 0;
    server.route("/set", { "GET" }, [&count](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "n", .value = std::to_string(++count), .path = "/" });
        resp->setBody("ok");
    });
    server.route("/check", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("n"));
    });
    co_await start_server(server);

    HttpClientConfig config;
    config.cookieStoreFactory = memoryCookieStore();
    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()), std::move(config));

    co_await client.get("/set");
    co_await client.get("/set");
    auto resp = co_await client.get("/check");
    NITRO_CHECK_EQ(resp.body(), "2");

    co_await server.stop();
}

/** Without CookieStore, cookies are not automatically sent. */
NITRO_TEST(http_client_no_cookie_store)
{
    HttpServer server(0);
    server.route("/set", { "GET" }, [](auto req, auto resp) {
        resp->addCookie(Cookie{ .name = "session", .value = "abc123", .path = "/" });
        resp->setBody("ok");
    });
    server.route("/check", { "GET" }, [](auto req, auto resp) {
        resp->setBody(req->getCookie("session"));
    });
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    co_await client.get("/set");
    auto resp = co_await client.get("/check");
    NITRO_CHECK_EQ(resp.body(), "");

    co_await server.stop();
}

/** Dead connection in idle pool is discarded; client reconnects transparently. */
NITRO_TEST(http_client_reconnects_after_peer_close)
{
    HttpServer server1(0);
    server1.route("/hello", { "GET" }, [](auto req, auto resp) { resp->setBody("first"); });
    co_await start_server(server1);
    int port = server1.listeningPort();

    HttpClient client(utils::format("http://127.0.0.1:{}", port));
    auto resp1 = co_await client.get("/hello");
    NITRO_CHECK_EQ(resp1.body(), "first");

    co_await server1.stop();
    co_await Scheduler::current()->sleep_for(std::chrono::milliseconds(50));

    HttpServer server2(port);
    server2.route("/hello", { "GET" }, [](auto req, auto resp) { resp->setBody("second"); });
    co_await start_server(server2);

    auto resp2 = co_await client.get("/hello");
    NITRO_CHECK_EQ(resp2.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp2.body(), "second");

    co_await server2.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
