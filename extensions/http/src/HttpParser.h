/**
 * @file HttpParser.h
 * @brief HTTP parser template and specializations
 */
#pragma once
#include <nitrocoro/http/HttpMessage.h>

#include <string>
#include <string_view>
#include <variant>

namespace nitrocoro::http
{

struct HttpParseError
{
    enum Code
    {
        MalformedRequestLine,
        AmbiguousContentLength,
        UnsupportedTransferEncoding
    };

    Code code;
    std::string message;
};

template <typename T>
using HttpParseResult = std::variant<T, HttpParseError, std::monostate>;

enum class HttpParserState
{
    ExpectStatusLine,
    ExpectHeader,
    HeaderComplete,
    Error
};

template <typename DataType>
class HttpParser;

// ============================================================================
// HttpParser<HttpRequest> - Parse HTTP Request
// ============================================================================

template <>
class HttpParser<HttpRequest>
{
public:
    using Result = HttpParseResult<HttpRequest>;
    using State = HttpParserState;
    HttpParser() = default;

    HttpParserState feedLine(std::string_view line);
    HttpParserState state() const { return state_; }
    HttpParseResult<HttpRequest> extractResult();

private:
    HttpRequest data_;
    HttpParserState state_ = HttpParserState::ExpectStatusLine;
    HttpParseError error_;

    void setError(HttpParseError::Code code, std::string message);
    bool parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString(std::string_view queryStr);
    void parseCookies(const std::string & cookieHeader);
    bool processHeaders();
    bool processTransferMode();
    bool processKeepAlive();
};

// ============================================================================
// HttpParser<HttpResponse> - Parse HTTP Response
// ============================================================================

template <>
class HttpParser<HttpResponse>
{
public:
    using Result = HttpParseResult<HttpResponse>;
    using State = HttpParserState;
    HttpParser() = default;

    HttpParserState feedLine(std::string_view line);
    HttpParserState state() const { return state_; }
    HttpParseResult<HttpResponse> extractResult();

private:
    HttpResponse data_;
    HttpParserState state_ = HttpParserState::ExpectStatusLine;
    HttpParseError error_;

    void setError(HttpParseError::Code code, std::string message);
    bool parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
    bool processHeaders();
    bool processTransferMode();
    bool processConnectionClose();
};

} // namespace nitrocoro::http
