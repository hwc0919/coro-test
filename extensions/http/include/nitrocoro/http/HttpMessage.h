/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <nitrocoro/http/Cookie.h>
#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpTypes.h>

#include <map>
#include <string>
#include <vector>

namespace nitrocoro::http
{

using HttpHeaderMap = std::map<std::string, HttpHeader, std::less<>>;
using HttpCookieMap = std::map<std::string, std::string, std::less<>>;
using HttpQueryMap = std::map<std::string, std::string, std::less<>>;
using HttpMultiQueryMap = std::map<std::string, std::vector<std::string>, std::less<>>;

struct HttpRequest
{
    Version version = Version::kHttp11;
    HttpMethod method = methods::_Invalid;
    std::string path;
    std::string rawPath;
    std::string query;
    HttpHeaderMap headers;
    HttpCookieMap cookies;
    HttpQueryMap queries;

    // Metadata parsed from headers
    TransferMode transferMode = TransferMode::UntilClose;
    size_t contentLength = 0;
    bool keepAlive = false;
};

struct HttpResponse
{
    Version version = Version::kHttp11;
    uint16_t statusCode{ static_cast<uint16_t>(StatusCode::k200OK) };
    std::string statusReason;
    HttpHeaderMap headers;
    std::vector<Cookie> cookies;

    // Metadata for sending
    TransferMode transferMode = TransferMode::ContentLength;
    size_t contentLength = 0;
    bool shouldClose = false;
};

} // namespace nitrocoro::http
