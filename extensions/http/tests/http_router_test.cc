/**
 * @file http_router_test.cc
 * @brief Unit tests for HttpRouter route matching logic.
 */
#include <nitrocoro/http/HttpRouter.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::http;

// ── Helpers ───────────────────────────────────────────────────────────────────

static auto dummyHandler()
{
    return [](auto req, auto resp) -> Task<> { co_return; };
}

static HttpRouter::RouteResult match(const HttpRouter & router,
                                     const std::string & method,
                                     const std::string & path)
{
    return router.route(method, path);
}

// ── Exact match ───────────────────────────────────────────────────────────────

// GET /hello → exact match
NITRO_TEST(router_exact_match)
{
    HttpRouter router;
    router.addRoute("GET", "/hello", dummyHandler());

    auto [h, p] = match(router, "GET", "/hello");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK(p.empty());
    co_return;
}

// GET /world → no route registered for this path
NITRO_TEST(router_exact_no_match)
{
    HttpRouter router;
    router.addRoute("GET", "/hello", dummyHandler());

    auto [h, p] = match(router, "GET", "/world");
    NITRO_CHECK(h == nullptr);
    co_return;
}

// GET /data → route registered as POST, method mismatch
NITRO_TEST(router_method_mismatch)
{
    HttpRouter router;
    router.addRoute("POST", "/data", dummyHandler());

    auto [h, p] = match(router, "GET", "/data");
    NITRO_CHECK(h == nullptr);
    co_return;
}

// ── Param match ───────────────────────────────────────────────────────────────

// GET /users/42 → /users/:id, single param
NITRO_TEST(router_param_match)
{
    HttpRouter router;
    router.addRoute("GET", "/users/:id", dummyHandler());

    auto [h, p] = match(router, "GET", "/users/42");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("id"), "42");
    co_return;
}

// GET /users/1/posts/99 → /users/:uid/posts/:pid, two params
NITRO_TEST(router_multi_param_match)
{
    HttpRouter router;
    router.addRoute("GET", "/users/:uid/posts/:pid", dummyHandler());

    auto [h, p] = match(router, "GET", "/users/1/posts/99");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("uid"), "1");
    NITRO_CHECK_EQ(p.at("pid"), "99");
    co_return;
}

// ── Wildcard match ────────────────────────────────────────────────────────────

// GET /files/a/b/c.txt → /files/*path, wildcard captures multiple segments
NITRO_TEST(router_wildcard_match)
{
    HttpRouter router;
    router.addRoute("GET", "/files/*path", dummyHandler());

    auto [h, p] = match(router, "GET", "/files/a/b/c.txt");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("path"), "a/b/c.txt");
    co_return;
}

// GET /files/readme.txt → /files/*path, wildcard captures single segment
NITRO_TEST(router_wildcard_single_segment)
{
    HttpRouter router;
    router.addRoute("GET", "/files/*path", dummyHandler());

    auto [h, p] = match(router, "GET", "/files/readme.txt");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("path"), "readme.txt");
    co_return;
}

// ── Regex match ───────────────────────────────────────────────────────────────

// GET /items/123 → regex /items/(\d+), capture group $1
NITRO_TEST(router_regex_match)
{
    HttpRouter router;
    router.addRouteRegex("GET", R"(/items/(\d+))", dummyHandler());

    auto [h, p] = match(router, "GET", "/items/123");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("$1"), "123");
    co_return;
}

// GET /items/abc → regex /items/(\d+), non-digit fails to match
NITRO_TEST(router_regex_no_match)
{
    HttpRouter router;
    router.addRouteRegex("GET", R"(/items/(\d+))", dummyHandler());

    auto [h, p] = match(router, "GET", "/items/abc");
    NITRO_CHECK(h == nullptr);
    co_return;
}

// GET /users/42 → regex /(\w+)/(\d+), two capture groups $1 $2
NITRO_TEST(router_regex_multi_capture)
{
    HttpRouter router;
    router.addRouteRegex("GET", R"(/(\w+)/(\d+))", dummyHandler());

    auto [h, p] = match(router, "GET", "/users/42");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK_EQ(p.at("$1"), "users");
    NITRO_CHECK_EQ(p.at("$2"), "42");
    co_return;
}

// ── Segment count mismatch ──────────────────────────────────────────────────

// GET /id/123/456 → /id/:id, extra segment causes no match
NITRO_TEST(router_param_segment_count_mismatch)
{
    HttpRouter router;
    router.addRoute("GET", "/id/:id", dummyHandler());

    auto [h, p] = match(router, "GET", "/id/123/456");
    NITRO_CHECK(h == nullptr);
    co_return;
}

// ── Empty router ───────────────────────────────────────────────────────────────

// GET /anything → empty router, no routes registered
NITRO_TEST(router_empty_no_match)
{
    HttpRouter router;
    auto result = router.route("GET", "/anything");
    NITRO_CHECK(!result);
    co_return;
}

// GET /api:v1 → colon not at segment start, must match as exact route
NITRO_TEST(router_colon_not_at_segment_start)
{
    HttpRouter router;
    router.addRoute("GET", "/api:v1", dummyHandler());

    auto [h, p] = match(router, "GET", "/api:v1");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK(p.empty()); // exact match, no params
    co_return;
}

// path exceeding 2048 bytes → rejected, no match
NITRO_TEST(router_path_too_long)
{
    HttpRouter router;
    router.addRoute("GET", "/*path", dummyHandler());

    std::string longPath(2049, 'a');
    longPath[0] = '/';
    auto result = router.route("GET", longPath);
    NITRO_CHECK(!result);
    co_return;
}

// path with more than 32 segments → rejected, no match
NITRO_TEST(router_too_many_segments)
{
    HttpRouter router;
    router.addRoute("GET", "/*path", dummyHandler());

    std::string deepPath;
    for (int i = 0; i < 33; ++i)
        deepPath += "/a";
    auto result = router.route("GET", deepPath);
    NITRO_CHECK(!result);
    co_return;
}

// ── Priority: exact > param > wildcard ───────────────────────────────────────

// GET /users/me → /users/me and /users/:id both registered, exact takes priority
NITRO_TEST(router_exact_beats_param)
{
    HttpRouter router;
    bool exactCalled = false;
    router.addRoute("GET", "/users/me", [&exactCalled](auto req, auto resp) -> Task<> {
        exactCalled = true;
        co_return;
    });
    router.addRoute("GET", "/users/:id", dummyHandler());

    auto [h, p] = match(router, "GET", "/users/me");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK(p.empty());
    co_return;
}

// GET /a/hello → /a/:b and /a/*rest both registered, param takes priority over wildcard
NITRO_TEST(router_param_beats_wildcard)
{
    HttpRouter router;
    router.addRoute("GET", "/a/:b", dummyHandler());
    router.addRoute("GET", "/a/*rest", dummyHandler());

    auto [h, p] = match(router, "GET", "/a/hello");
    NITRO_CHECK(h != nullptr);
    NITRO_CHECK(p.count("b"));
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
