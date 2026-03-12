/**
 * @file HttpParser.h
 * @brief HTTP parser template and specializations
 */
#pragma once
#include <nitrocoro/http/HttpMessage.h>

#include <string>
#include <string_view>

namespace nitrocoro::http
{

enum class HttpParseError
{
    None,
    ConnectionClosed,
    MalformedRequestLine,
    MissingHostHeader,
    AmbiguousContentLength,
    UnsupportedTransferEncoding
};

template <typename T>
struct HttpParseResult
{
    T message;
    HttpParseError errorCode = HttpParseError::None;
    std::string errorMessage;

    bool error() const { return errorCode != HttpParseError::None; }
};

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

    HttpParserState parseLine(std::string_view line);
    HttpParserState state() const { return state_; }
    HttpParseError errorCode() const { return errorCode_; }
    const std::string & errorMessage() const { return errorMessage_; }
    HttpParseResult<HttpRequest> extractResult();

private:
    HttpRequest data_;
    HttpParserState state_ = HttpParserState::ExpectStatusLine;
    HttpParseError errorCode_ = HttpParseError::None;
    std::string errorMessage_;

    void setError(HttpParseError code, std::string message);
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

    HttpParserState parseLine(std::string_view line);
    HttpParserState state() const { return state_; }
    HttpParseError errorCode() const { return errorCode_; }
    const std::string & errorMessage() const { return errorMessage_; }
    HttpParseResult<HttpResponse> extractResult();

private:
    HttpResponse data_;
    HttpParserState state_ = HttpParserState::ExpectStatusLine;
    HttpParseError errorCode_ = HttpParseError::None;
    std::string errorMessage_;

    void setError(HttpParseError code, std::string message);
    bool parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
    bool processHeaders();
    bool processTransferMode();
    bool processConnectionClose();
};

} // namespace nitrocoro::http
