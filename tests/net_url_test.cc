#include <nitrocoro/net/Url.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro::net;

NITRO_TEST(url_parse_http)
{
    Url url("http://example.com/path?key=value");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.scheme(), "http");
    NITRO_CHECK_EQ(url.host(), "example.com");
    NITRO_CHECK_EQ(url.port(), 80);
    NITRO_CHECK_EQ(url.path(), "/path");
    NITRO_CHECK_EQ(url.query(), "key=value");
    NITRO_CHECK_EQ(url.baseUrl(), "http://example.com");
    NITRO_CHECK_EQ(url.fullPath(), "/path?key=value");
    co_return;
}

NITRO_TEST(url_parse_https)
{
    Url url("https://example.com/path");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.scheme(), "https");
    NITRO_CHECK_EQ(url.host(), "example.com");
    NITRO_CHECK_EQ(url.port(), 443);
    NITRO_CHECK_EQ(url.baseUrl(), "https://example.com");
    NITRO_CHECK_EQ(url.fullPath(), "/path");
    co_return;
}

NITRO_TEST(url_parse_custom_port)
{
    Url url("http://localhost:8080/api");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.host(), "localhost");
    NITRO_CHECK_EQ(url.port(), 8080);
    NITRO_CHECK_EQ(url.baseUrl(), "http://localhost:8080");
    NITRO_CHECK_EQ(url.fullPath(), "/api");
    co_return;
}

NITRO_TEST(url_parse_no_path)
{
    Url url("http://example.com");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.host(), "example.com");
    NITRO_CHECK_EQ(url.port(), 80);
    NITRO_CHECK_EQ(url.path(), "/");
    NITRO_CHECK_EQ(url.query(), "");
    NITRO_CHECK_EQ(url.baseUrl(), "http://example.com");
    NITRO_CHECK_EQ(url.fullPath(), "/");
    co_return;
}

NITRO_TEST(url_parse_no_path_with_slash)
{
    Url url("http://example.com/");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.path(), "/");
    NITRO_CHECK_EQ(url.fullPath(), "/");
    co_return;
}

NITRO_TEST(url_parse_query_only)
{
    Url url("http://example.com?foo=bar");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.path(), "/");
    NITRO_CHECK_EQ(url.query(), "foo=bar");
    NITRO_CHECK_EQ(url.fullPath(), "/?foo=bar");
    co_return;
}

NITRO_TEST(url_parse_invalid_no_scheme)
{
    Url url("example.com/path");
    NITRO_CHECK(!url.isValid());
    co_return;
}

NITRO_TEST(url_parse_invalid_empty)
{
    Url url("");
    NITRO_CHECK(!url.isValid());
    co_return;
}

NITRO_TEST(url_parse_invalid_port)
{
    Url url("http://example.com:abc/path");
    NITRO_CHECK(!url.isValid());
    co_return;
}

NITRO_TEST(url_parse_ws)
{
    Url url("ws://example.com/ws");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.scheme(), "ws");
    NITRO_CHECK_EQ(url.port(), 80);
    co_return;
}

NITRO_TEST(url_parse_wss)
{
    Url url("wss://example.com/ws");
    NITRO_CHECK(url.isValid());
    NITRO_CHECK_EQ(url.scheme(), "wss");
    NITRO_CHECK_EQ(url.port(), 443);
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
