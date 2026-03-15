/**
 * @file HttpMessageAccessor.h
 * @brief Base classes for read-only HTTP message access
 */
#pragma once
#include <nitrocoro/http/Cookie.h>
#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/HttpTypes.h>
#include <nitrocoro/utils/UrlEncode.h>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace nitrocoro::http
{

template <typename Message>
class HttpMessageAccessor
{
public:
    explicit HttpMessageAccessor(Message message);

    const HttpHeaderMap & headers() const;
    const std::string & getHeader(std::string_view name) const;
    const std::string & getHeader(HttpHeader::NameCode code) const;
    const std::string & getCookie(std::string_view name) const;

protected:
    Message message_;
};

class HttpRequestAccessor : public HttpMessageAccessor<HttpRequest>
{
public:
    using HttpMessageAccessor::HttpMessageAccessor;

    const HttpCookieMap & cookies() const { return message_.cookies; }

    Version version() const { return message_.version; }
    HttpMethod method() const { return message_.method; }
    const std::string & path() const { return message_.path; }
    const std::string & queryString() const { return message_.query; }
    const HttpQueryMap & queries() const { return message_.queries; }

    const std::string & getQuery(std::string_view name) const
    {
        static const std::string emptyValue{};
        auto it = message_.queries.find(name);
        return it != message_.queries.end() ? it->second : emptyValue;
    }

    // Returns all query parameters, with multiple values per key.
    // NOTE: will parse the raw query string on every call.
    HttpMultiQueryMap multiQueries() const
    {
        HttpMultiQueryMap result;
        std::string_view raw = message_.query;
        size_t start = 0;
        while (start < raw.size())
        {
            size_t ampPos = raw.find('&', start);
            size_t end = (ampPos == std::string_view::npos) ? raw.size() : ampPos;
            std::string_view pair = raw.substr(start, end - start);
            size_t eqPos = pair.find('=');
            if (eqPos != std::string_view::npos)
            {
                auto key = utils::urlDecodeComponent(pair.substr(0, eqPos));
                auto value = utils::urlDecodeComponent(pair.substr(eqPos + 1));
                result[key].push_back(std::move(value));
            }
            else if (!pair.empty())
            {
                result[utils::urlDecodeComponent(pair)];
            }
            if (ampPos == std::string_view::npos)
                break;
            start = ampPos + 1;
        }
        return result;
    }
};

class HttpResponseAccessor : public HttpMessageAccessor<HttpResponse>
{
public:
    using HttpMessageAccessor::HttpMessageAccessor;

    const std::vector<Cookie> & cookies() const { return message_.cookies; }

    Version version() const { return message_.version; }
    uint16_t statusCode() const { return message_.statusCode; }
    const std::string & statusReason() const { return message_.statusReason; }
};

template <typename Message>
HttpMessageAccessor<Message>::HttpMessageAccessor(Message message)
    : message_(std::move(message))
{
}

template <typename Message>
const HttpHeaderMap & HttpMessageAccessor<Message>::headers() const
{
    return message_.headers;
}

template <typename Message>
const std::string & HttpMessageAccessor<Message>::getHeader(std::string_view name) const
{
    static const std::string emptyValue{};
    std::string lowerName{ name };
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    auto it = message_.headers.find(lowerName);
    return it != message_.headers.end() ? it->second.value() : emptyValue;
}

template <typename Message>
const std::string & HttpMessageAccessor<Message>::getHeader(HttpHeader::NameCode code) const
{
    static const std::string emptyValue{};
    auto it = message_.headers.find(HttpHeader::codeToName(code));
    return it != message_.headers.end() ? it->second.value() : emptyValue;
}

template <typename Message>
const std::string & HttpMessageAccessor<Message>::getCookie(std::string_view name) const
{
    static const std::string emptyValue{};
    auto it = message_.cookies.find(name);
    return it != message_.cookies.end() ? it->second : emptyValue;
}

} // namespace nitrocoro::http
