/**
 * @file cookie_test.cc
 * @brief Tests for Cookie data structure
 */
#include <nitrocoro/http/Cookie.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::http;

// ── Cookie::toString ──────────────────────────────────────────────────────────

NITRO_TEST(cookie_tostring_basic)
{
    Cookie c{ .name = "session", .value = "abc123" };
    NITRO_CHECK_EQ(c.toString(), "session=abc123");
    co_return;
}

NITRO_TEST(cookie_tostring_all_attributes)
{
    Cookie c{
        .name = "id",
        .value = "42",
        .domain = "example.com",
        .path = "/",
        .expires = "Thu, 01 Jan 2099 00:00:00 GMT",
        .maxAge = 3600,
        .sameSite = Cookie::SameSite::Lax,
        .secure = true,
        .httpOnly = true,
    };
    std::string s = c.toString();
    NITRO_CHECK(s.find("id=42") == 0);
    NITRO_CHECK(s.find("Expires=Thu, 01 Jan 2099 00:00:00 GMT") != std::string::npos);
    NITRO_CHECK(s.find("Max-Age=3600") != std::string::npos);
    NITRO_CHECK(s.find("Domain=example.com") != std::string::npos);
    NITRO_CHECK(s.find("Path=/") != std::string::npos);
    NITRO_CHECK(s.find("Secure") != std::string::npos);
    NITRO_CHECK(s.find("HttpOnly") != std::string::npos);
    NITRO_CHECK(s.find("SameSite=Lax") != std::string::npos);
    co_return;
}

NITRO_TEST(cookie_tostring_samesite_variants)
{
    auto strict = Cookie{ .name = "a", .value = "1", .sameSite = Cookie::SameSite::Strict }.toString();
    auto lax = Cookie{ .name = "a", .value = "1", .sameSite = Cookie::SameSite::Lax }.toString();
    auto none = Cookie{ .name = "a", .value = "1", .sameSite = Cookie::SameSite::None }.toString();
    auto unset = Cookie{ .name = "a", .value = "1", .sameSite = Cookie::SameSite::Unset }.toString();
    NITRO_CHECK(strict.find("SameSite=Strict") != std::string::npos);
    NITRO_CHECK(lax.find("SameSite=Lax") != std::string::npos);
    NITRO_CHECK(none.find("SameSite=None") != std::string::npos);
    NITRO_CHECK(unset.find("SameSite") == std::string::npos);
    co_return;
}

NITRO_TEST(cookie_tostring_no_optional_attributes)
{
    Cookie c{ .name = "x", .value = "y" };
    std::string s = c.toString();
    NITRO_CHECK(s.find("Expires") == std::string::npos);
    NITRO_CHECK(s.find("Max-Age") == std::string::npos);
    NITRO_CHECK(s.find("Domain") == std::string::npos);
    NITRO_CHECK(s.find("Path") == std::string::npos);
    NITRO_CHECK(s.find("Secure") == std::string::npos);
    NITRO_CHECK(s.find("HttpOnly") == std::string::npos);
    NITRO_CHECK(s.find("SameSite") == std::string::npos);
    co_return;
}

// ── Cookie::formatExpires ─────────────────────────────────────────────────────

NITRO_TEST(cookie_format_expires)
{
    // 2000-01-01 00:00:00 UTC = 946684800
    auto tp = std::chrono::system_clock::from_time_t(946684800);
    NITRO_CHECK_EQ(Cookie::formatExpires(tp), "Sat, 01 Jan 2000 00:00:00 GMT");
    co_return;
}

// ── Cookie::fromString ────────────────────────────────────────────────────────

NITRO_TEST(cookie_fromstring_basic)
{
    auto c = Cookie::fromString("session=abc123");
    NITRO_CHECK_EQ(c.name, "session");
    NITRO_CHECK_EQ(c.value, "abc123");
    co_return;
}

NITRO_TEST(cookie_fromstring_all_attributes)
{
    auto c = Cookie::fromString("id=42; Domain=example.com; "
                                "Path=/app; Max-Age=3600; "
                                "Expires=Thu, 01 Jan 2099 00:00:00 GMT; "
                                "Secure; HttpOnly; SameSite=None");
    NITRO_CHECK_EQ(c.name, "id");
    NITRO_CHECK_EQ(c.value, "42");
    NITRO_CHECK_EQ(c.domain, "example.com");
    NITRO_CHECK_EQ(c.path, "/app");
    NITRO_CHECK_EQ(c.maxAge, 3600);
    NITRO_CHECK_EQ(c.expires, "Thu, 01 Jan 2099 00:00:00 GMT");
    NITRO_CHECK(c.secure);
    NITRO_CHECK(c.httpOnly);
    NITRO_CHECK(c.sameSite == Cookie::SameSite::None);
    co_return;
}

NITRO_TEST(cookie_fromstring_samesite_case_insensitive)
{
    auto c1 = Cookie::fromString("a=1; SameSite=strict");
    auto c2 = Cookie::fromString("a=1; samesite=LAX");
    auto c3 = Cookie::fromString("a=1; SAMESITE=None");
    NITRO_CHECK(c1.sameSite == Cookie::SameSite::Strict);
    NITRO_CHECK(c2.sameSite == Cookie::SameSite::Lax);
    NITRO_CHECK(c3.sameSite == Cookie::SameSite::None);
    co_return;
}

NITRO_TEST(cookie_fromstring_invalid)
{
    auto c = Cookie::fromString("invalid");
    NITRO_CHECK(c.name.empty());
    co_return;
}

NITRO_TEST(cookie_roundtrip)
{
    Cookie original{
        .name = "token",
        .value = "xyz",
        .domain = "example.com",
        .path = "/",
        .maxAge = 86400,
        .sameSite = Cookie::SameSite::Lax,
        .secure = true,
        .httpOnly = true,
    };
    auto parsed = Cookie::fromString(original.toString());
    NITRO_CHECK_EQ(parsed.name, original.name);
    NITRO_CHECK_EQ(parsed.value, original.value);
    NITRO_CHECK_EQ(parsed.domain, original.domain);
    NITRO_CHECK_EQ(parsed.path, original.path);
    NITRO_CHECK_EQ(parsed.maxAge, original.maxAge);
    NITRO_CHECK(parsed.sameSite == original.sameSite);
    NITRO_CHECK_EQ(parsed.secure, original.secure);
    NITRO_CHECK_EQ(parsed.httpOnly, original.httpOnly);
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
