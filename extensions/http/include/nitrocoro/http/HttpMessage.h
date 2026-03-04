/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpTypes.h>

#include <map>
#include <string>

namespace nitrocoro::http
{

using HttpHeaderMap = std::map<std::string, HttpHeader, std::less<>>;
using HttpCookieMap = std::map<std::string, std::string, std::less<>>;
using HttpQueryMap = std::map<std::string, std::string, std::less<>>;

struct HttpRequest
{
    std::string method;
    std::string fullPath;
    std::string path;
    std::string query;
    Version version = Version::kHttp11;
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
    StatusCode statusCode = StatusCode::k200OK;
    std::string statusReason;
    Version version = Version::kHttp11;
    HttpHeaderMap headers;
    HttpCookieMap cookies;

    // Metadata for sending
    TransferMode transferMode = TransferMode::ContentLength;
    size_t contentLength = 0;
    bool shouldClose = false;
};

} // namespace nitrocoro::http
