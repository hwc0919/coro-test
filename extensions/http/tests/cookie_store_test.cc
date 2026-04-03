/**
 * @file cookie_store_test.cc
 * @brief Tests for MemoryCookieStore
 */
#include <nitrocoro/http/CookieStore.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::http;

// ── store / load basic ────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_load_basic)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/" } });
    auto cookies = store.load("/");
    NITRO_REQUIRE_EQ(cookies.size(), 1);
    NITRO_CHECK_EQ(cookies[0].name, "a");
    NITRO_CHECK_EQ(cookies[0].value, "1");
    co_return;
}

NITRO_TEST(cookie_store_load_empty)
{
    MemoryCookieStore store;
    NITRO_CHECK(store.load("/").empty());
    co_return;
}

// ── path matching ─────────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_path_prefix_match)
{
    MemoryCookieStore store;
    store.store("/app/login", { Cookie{ .name = "a", .value = "1", .path = "/app" } });
    NITRO_CHECK_EQ(store.load("/app").size(), 1);
    NITRO_CHECK_EQ(store.load("/app/login").size(), 1);
    NITRO_CHECK_EQ(store.load("/app/other").size(), 1);
    NITRO_CHECK(store.load("/other").empty());
    NITRO_CHECK(store.load("/ap").empty());
    co_return;
}

NITRO_TEST(cookie_store_path_boundary)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/app" } });
    // /appx should NOT match /app
    NITRO_CHECK(store.load("/appx").empty());
    // /app/x should match /app
    NITRO_CHECK_EQ(store.load("/app/x").size(), 1);
    co_return;
}

NITRO_TEST(cookie_store_root_path_matches_all)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/" } });
    NITRO_CHECK_EQ(store.load("/").size(), 1);
    NITRO_CHECK_EQ(store.load("/anything").size(), 1);
    NITRO_CHECK_EQ(store.load("/a/b/c").size(), 1);
    co_return;
}

// ── default path ──────────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_default_path_from_request)
{
    MemoryCookieStore store;
    // cookie with no path — should default to request path directory
    store.store("/app/login", { Cookie{ .name = "a", .value = "1" } });
    NITRO_CHECK_EQ(store.load("/app/login").size(), 1);
    NITRO_CHECK_EQ(store.load("/app/other").size(), 1);
    NITRO_CHECK(store.load("/other").empty());
    co_return;
}

// ── overwrite ─────────────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_overwrite_same_name_path)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/" } });
    store.store("/", { Cookie{ .name = "a", .value = "2", .path = "/" } });
    auto cookies = store.load("/");
    NITRO_REQUIRE_EQ(cookies.size(), 1);
    NITRO_CHECK_EQ(cookies[0].value, "2");
    co_return;
}

// ── Max-Age=0 deletes ─────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_max_age_zero_deletes)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/" } });
    NITRO_REQUIRE_EQ(store.load("/").size(), 1);
    store.store("/", { Cookie{ .name = "a", .value = "", .path = "/", .maxAge = 0 } });
    NITRO_CHECK(store.load("/").empty());
    co_return;
}

// ── expiry ────────────────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_max_age_expiry)
{
    MemoryCookieStore store;
    // Max-Age=1 — not yet expired
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/", .maxAge = 3600 } });
    NITRO_CHECK_EQ(store.load("/").size(), 1);
    co_return;
}

NITRO_TEST(cookie_store_no_expiry_persists)
{
    MemoryCookieStore store;
    store.store("/", { Cookie{ .name = "a", .value = "1", .path = "/" } });
    // trigger store cleanup
    store.store("/", {});
    NITRO_CHECK_EQ(store.load("/").size(), 1);
    co_return;
}

// ── multiple cookies ──────────────────────────────────────────────────────────

NITRO_TEST(cookie_store_multiple_cookies)
{
    MemoryCookieStore store;
    store.store("/", {
        Cookie{ .name = "a", .value = "1", .path = "/" },
        Cookie{ .name = "b", .value = "2", .path = "/" },
        Cookie{ .name = "c", .value = "3", .path = "/other" },
    });
    auto cookies = store.load("/");
    NITRO_CHECK_EQ(cookies.size(), 2);
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
