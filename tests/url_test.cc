#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/UrlEncode.h>

using namespace nitrocoro::utils;

// ── urlDecode ─────────────────────────────────────────────────────────────────

NITRO_TEST(url_decode_basic)
{
    NITRO_CHECK_EQ(urlDecode("/hello%20world"), "/hello world");
    NITRO_CHECK_EQ(urlDecode("/foo%2Fbar"), "/foo%2Fbar"); // %2F not decoded
    NITRO_CHECK_EQ(urlDecode("/a%2Bb"), "/a+b");           // %2B -> +
    NITRO_CHECK_EQ(urlDecode("/abc-_.~"), "/abc-_.~");
    NITRO_CHECK_EQ(urlDecode("/"), "/");
    NITRO_CHECK_EQ(urlDecode(""), "");
    co_return;
}

NITRO_TEST(url_decode_plus_is_literal)
{
    NITRO_CHECK_EQ(urlDecode("/a+b"), "/a+b");
    co_return;
}

NITRO_TEST(url_decode_invalid)
{
    NITRO_CHECK_EQ(urlDecode("/foo%2"), "/foo%2");   // truncated, keep as-is
    NITRO_CHECK_EQ(urlDecode("/foo%GG"), "/foo%GG"); // invalid hex, keep as-is
    NITRO_CHECK_EQ(urlDecode("/foo%2G"), "/foo%2G"); // invalid hex, keep as-is
    NITRO_CHECK_EQ(urlDecode("/abc/%zz%20"), "/abc/%zz "); // partial invalid
    co_return;
}

// ── urlDecodeComponent ────────────────────────────────────────────────────────

NITRO_TEST(url_decode_component_basic)
{
    NITRO_CHECK_EQ(urlDecodeComponent("hello%20world"), "hello world");
    NITRO_CHECK_EQ(urlDecodeComponent("hello+world"), "hello world"); // + -> space
    NITRO_CHECK_EQ(urlDecodeComponent("a%2Bb"), "a+b");               // %2B -> +
    NITRO_CHECK_EQ(urlDecodeComponent("foo%2Fbar"), "foo/bar");       // %2F decoded
    NITRO_CHECK_EQ(urlDecodeComponent("abc-_.~"), "abc-_.~");
    NITRO_CHECK_EQ(urlDecodeComponent(""), "");
    co_return;
}

NITRO_TEST(url_decode_component_invalid)
{
    NITRO_CHECK_EQ(urlDecodeComponent("foo%2"), "foo%2");
    NITRO_CHECK_EQ(urlDecodeComponent("foo%GG"), "foo%GG");
    co_return;
}

// ── urlEncode ─────────────────────────────────────────────────────────────────

NITRO_TEST(url_encode_basic)
{
    NITRO_CHECK_EQ(urlEncode("/hello world"), "/hello%20world");
    NITRO_CHECK_EQ(urlEncode("/foo/bar"), "/foo/bar"); // / not encoded
    NITRO_CHECK_EQ(urlEncode("/a+b"), "/a%2Bb");       // + -> %2B
    NITRO_CHECK_EQ(urlEncode("/abc-_.~"), "/abc-_.~");
    NITRO_CHECK_EQ(urlEncode(""), "");
    co_return;
}

NITRO_TEST(url_encode_roundtrip)
{
    std::string input = "/user/john doe/profile";
    NITRO_CHECK_EQ(urlDecode(urlEncode(input)), input);
    co_return;
}

// ── urlEncodeComponent ────────────────────────────────────────────────────────

NITRO_TEST(url_encode_component_basic)
{
    NITRO_CHECK_EQ(urlEncodeComponent("hello world"), "hello%20world");
    NITRO_CHECK_EQ(urlEncodeComponent("foo/bar"), "foo%2Fbar"); // / encoded
    NITRO_CHECK_EQ(urlEncodeComponent("a+b"), "a%2Bb");         // + -> %2B
    NITRO_CHECK_EQ(urlEncodeComponent("foo=bar&baz"), "foo%3Dbar%26baz");
    NITRO_CHECK_EQ(urlEncodeComponent("abc-_.~"), "abc-_.~");
    NITRO_CHECK_EQ(urlEncodeComponent(""), "");
    co_return;
}

NITRO_TEST(url_encode_component_roundtrip)
{
    std::string input = "hello world/foo+bar=baz";
    NITRO_CHECK_EQ(urlDecodeComponent(urlEncodeComponent(input)), input);
    co_return;
}

// ── formEncode ────────────────────────────────────────────────────────────────

NITRO_TEST(form_encode_basic)
{
    NITRO_CHECK_EQ(formEncode("hello world"), "hello+world"); // space -> +
    NITRO_CHECK_EQ(formEncode("foo/bar"), "foo%2Fbar");       // / encoded
    NITRO_CHECK_EQ(formEncode("a+b"), "a%2Bb");               // + -> %2B
    NITRO_CHECK_EQ(formEncode("foo=bar&baz"), "foo%3Dbar%26baz");
    NITRO_CHECK_EQ(formEncode("abc-_.~"), "abc-_.~");
    NITRO_CHECK_EQ(formEncode(""), "");
    co_return;
}

NITRO_TEST(form_encode_roundtrip)
{
    std::string input = "hello world/foo+bar=baz";
    NITRO_CHECK_EQ(formDecode(formEncode(input)), input);
    co_return;
}

// ── formDecode ────────────────────────────────────────────────────────────────

NITRO_TEST(form_decode_basic)
{
    NITRO_CHECK_EQ(formDecode("hello+world"), "hello world");
    NITRO_CHECK_EQ(formDecode("hello%20world"), "hello world");
    NITRO_CHECK_EQ(formDecode("a%2Bb"), "a+b");
    NITRO_CHECK_EQ(formDecode("foo%2Fbar"), "foo/bar");
    NITRO_CHECK_EQ(formDecode(""), "");
    co_return;
}

NITRO_TEST(form_decode_invalid)
{
    NITRO_CHECK_EQ(formDecode("foo%2"), "foo%2");
    NITRO_CHECK_EQ(formDecode("foo%GG"), "foo%GG");
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
