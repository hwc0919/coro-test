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

template <typename DataType>
class HttpParser;

// ============================================================================
// HttpParser<HttpRequest> - Parse HTTP Request
// ============================================================================

template <>
class HttpParser<HttpRequest>
{
public:
    HttpParser() = default;

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return state_ == State::Complete; }
    HttpRequest && extractMessage() { return std::move(data_); }

private:
    enum class State
    {
        ExpectStatusLine,
        ExpectHeader,
        Complete
    };

    HttpRequest data_;
    State state_ = State::ExpectStatusLine;

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString(std::string_view queryStr);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
    void processTransferMode();
    void processKeepAlive();
};

// ============================================================================
// HttpParser<HttpResponse> - Parse HTTP Response
// ============================================================================

template <>
class HttpParser<HttpResponse>
{
public:
    HttpParser() = default;

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return state_ == State::Complete; }
    HttpResponse && extractMessage() { return std::move(data_); }

private:
    enum class State
    {
        ExpectStatusLine,
        ExpectHeader,
        Complete
    };

    HttpResponse data_;
    State state_ = State::ExpectStatusLine;

    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
    void processTransferMode();
    void processConnectionClose();
};

} // namespace nitrocoro::http
