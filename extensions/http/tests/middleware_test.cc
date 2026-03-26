/**
 * @file middleware_test.cc
 * @brief Tests for HttpServer middleware.
 */
#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::http;

// ── Helpers ───────────────────────────────────────────────────────────────────

static SharedFuture<> start_server(HttpServer & server)
{
    Scheduler::current()->spawn([&server]() -> Task<> { co_await server.start(); });
    return server.started();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

/** Single middleware is called; next() forwards to handler. */
NITRO_TEST(middleware_single)
{
    HttpServer server(0);
    bool middlewareCalled = false;

    server.use([&](auto req, auto resp, auto next) -> Task<> {
        middlewareCalled = true;
        co_await next();
    });
    server.route("/", { "GET" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    HttpClient client;
    auto resp = co_await client.get("http://127.0.0.1:" + std::to_string(server.listeningPort()) + "/");
    NITRO_CHECK(middlewareCalled);
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "ok");

    co_await server.stop();
}

/** Multiple middlewares execute in registration order; post-next code runs in reverse order. */
NITRO_TEST(middleware_order)
{
    HttpServer server(0);
    std::string log;

    server.use([&](auto req, auto resp, auto next) -> Task<> {
        log += "A-before;";
        co_await next();
        log += "A-after;";
    });
    server.use([&](auto req, auto resp, auto next) -> Task<> {
        log += "B-before;";
        co_await next();
        log += "B-after;";
    });
    server.route("/", { "GET" }, [&](auto req, auto resp) {
        log += "handler;";
        resp->setBody("ok");
    });
    co_await start_server(server);

    HttpClient client;
    co_await client.get("http://127.0.0.1:" + std::to_string(server.listeningPort()) + "/");
    NITRO_CHECK_EQ(log, "A-before;B-before;handler;B-after;A-after;");

    co_await server.stop();
}

/** Middleware that does not call next() short-circuits the handler. */
NITRO_TEST(middleware_short_circuit)
{
    HttpServer server(0);
    bool handlerCalled = false;

    server.use([](auto req, auto resp, auto next) -> Task<> {
        resp->setStatus(StatusCode::k401Unauthorized);
        resp->setBody("Unauthorized");
        // next() not called
        co_return;
    });
    server.route("/", { "GET" }, [&](auto req, auto resp) {
        handlerCalled = true;
        resp->setBody("ok");
    });
    co_await start_server(server);

    HttpClient client;
    auto resp = co_await client.get("http://127.0.0.1:" + std::to_string(server.listeningPort()) + "/");
    NITRO_CHECK(!handlerCalled);
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k401Unauthorized);
    NITRO_CHECK_EQ(resp.body(), "Unauthorized");

    co_await server.stop();
}

/** Middleware can read path parameters. */
NITRO_TEST(middleware_path_params)
{
    HttpServer server(0);
    std::string capturedId;

    server.use([&](auto req, auto resp, auto next) -> Task<> {
        capturedId = req->pathParams().at("id");
        co_await next();
    });
    server.route("/users/:id", { "GET" }, [](auto req, auto resp) {
        resp->setBody("ok");
    });
    co_await start_server(server);

    HttpClient client;
    auto resp = co_await client.get("http://127.0.0.1:" + std::to_string(server.listeningPort()) + "/users/42");
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(capturedId, "42");

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
