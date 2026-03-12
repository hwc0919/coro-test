/**
 * @file http_parser_test.cc
 * @brief Tests for HttpParser
 */
#include "../src/HttpParser.h"
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro::http;

// ── Request Parser Tests ──────────────────────────────────────────────────────

NITRO_TEST(http_parser_request_basic)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET /hello HTTP/1.1");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("Host: example.com");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("");
    NITRO_CHECK_EQ(state, HttpParserState::HeaderComplete);

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.method, methods::Get);
    NITRO_CHECK_EQ(result.message.path, "/hello");
    NITRO_CHECK_EQ(result.message.version, Version::kHttp11);
    co_return;
}

NITRO_TEST(http_parser_request_with_query)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /search?q=hello+world&page=1 HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.path, "/search");
    NITRO_CHECK_EQ(result.message.query, "q=hello+world&page=1");
    NITRO_CHECK(result.message.queries.contains("q"));
    NITRO_CHECK_EQ(result.message.queries.at("q"), "hello world");
    NITRO_CHECK(result.message.queries.contains("page"));
    NITRO_CHECK_EQ(result.message.queries.at("page"), "1");
    co_return;
}

NITRO_TEST(http_parser_request_malformed_line)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET /hello"); // Missing HTTP version
    NITRO_CHECK_EQ(state, HttpParserState::Error);

    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::MalformedRequestLine);
    co_return;
}

NITRO_TEST(http_parser_request_missing_space)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET"); // Missing path and version
    NITRO_CHECK_EQ(state, HttpParserState::Error);

    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::MalformedRequestLine);
    co_return;
}

NITRO_TEST(http_parser_request_content_length)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Content-Length: 100");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 100);
    co_return;
}

NITRO_TEST(http_parser_request_chunked)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: chunked");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::Chunked);
    co_return;
}

NITRO_TEST(http_parser_request_unsupported_encoding)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: gzip");
    auto state = parser.parseLine("");

    NITRO_CHECK_EQ(state, HttpParserState::Error);
    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::UnsupportedTransferEncoding);
    co_return;
}

NITRO_TEST(http_parser_request_keep_alive_http11)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.keepAlive); // HTTP/1.1 default
    co_return;
}

NITRO_TEST(http_parser_request_keep_alive_http10)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.0");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(!result.message.keepAlive); // HTTP/1.0 default
    co_return;
}

NITRO_TEST(http_parser_request_connection_close)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Connection: close");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(!result.message.keepAlive);
    co_return;
}

// ── Response Parser Tests ─────────────────────────────────────────────────────

NITRO_TEST(http_parser_response_basic)
{
    HttpParser<HttpResponse> parser;

    auto state = parser.parseLine("HTTP/1.1 200 OK");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("Content-Type: text/plain");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("");
    NITRO_CHECK_EQ(state, HttpParserState::HeaderComplete);

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.version, Version::kHttp11);
    NITRO_CHECK_EQ(result.message.statusCode, StatusCode::k200OK);
    NITRO_CHECK_EQ(result.message.statusReason, "OK");
    co_return;
}

NITRO_TEST(http_parser_response_content_length)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Content-Length: 50");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 50);
    co_return;
}

NITRO_TEST(http_parser_response_chunked)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Transfer-Encoding: chunked");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::Chunked);
    co_return;
}

NITRO_TEST(http_parser_response_until_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine(""); // No Content-Length or Transfer-Encoding

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::UntilClose);
    co_return;
}

NITRO_TEST(http_parser_response_connection_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Connection: close");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.shouldClose);
    co_return;
}

NITRO_TEST(http_parser_response_http10_default_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.0 200 OK");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.shouldClose); // HTTP/1.0 default
    co_return;
}

NITRO_TEST(http_parser_response_unsupported_encoding)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Transfer-Encoding: deflate");
    auto state = parser.parseLine("");

    NITRO_CHECK_EQ(state, HttpParserState::Error);
    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::UnsupportedTransferEncoding);
    co_return;
}

// ── Edge Cases ────────────────────────────────────────────────────────────────

NITRO_TEST(http_parser_empty_header_value)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("X-Empty:"); // Empty header value
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.headers.contains("x-empty"));
    NITRO_CHECK_EQ(result.message.headers.at("x-empty").value(), "");
    co_return;
}

NITRO_TEST(http_parser_header_with_spaces)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("  X-Spaced  :  value with spaces  ");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.headers.contains("x-spaced"));
    NITRO_CHECK_EQ(result.message.headers.at("x-spaced").value(), "value with spaces");
    co_return;
}

NITRO_TEST(http_parser_identity_encoding)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: identity");
    parser.parseLine("Content-Length: 10");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 10);
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
