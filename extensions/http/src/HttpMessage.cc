/**
 * @file HttpMessage.cc
 * @brief Implementation of HttpMessage
 */
#include <nitrocoro/http/HttpMessage.h>

#include <nitrocoro/utils/UrlEncode.h>

namespace nitrocoro::http
{

namespace
{
const std::string & getHeaderImpl(const HttpHeaderMap & headers, std::string_view name)
{
    static const std::string emptyValue{};
    auto it = headers.find(HttpHeader::toLower(name));
    return it != headers.end() ? it->second.value() : emptyValue;
}

const std::string & getHeaderImpl(const HttpHeaderMap & headers, HttpHeader::NameCode code)
{
    static const std::string emptyValue{};
    auto it = headers.find(HttpHeader::codeToName(code));
    return it != headers.end() ? it->second.value() : emptyValue;
}

} // namespace

// HttpRequestAccessor

HttpRequestAccessor::HttpRequestAccessor(HttpRequest message)
    : message_(std::move(message))
{
}

const std::string & HttpRequestAccessor::getQuery(std::string_view name) const
{
    static const std::string emptyValue{};
    auto it = message_.queries.find(name);
    return it != message_.queries.end() ? it->second : emptyValue;
}

HttpMultiQueryMap HttpRequestAccessor::multiQueries() const
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

const std::string & HttpRequestAccessor::getHeader(std::string_view name) const
{
    return getHeaderImpl(message_.headers, name);
}

const std::string & HttpRequestAccessor::getHeader(HttpHeader::NameCode code) const
{
    return getHeaderImpl(message_.headers, code);
}

const std::string & HttpRequestAccessor::getCookie(std::string_view name) const
{
    static const std::string emptyValue{};
    auto it = message_.cookies.find(name);
    return it != message_.cookies.end() ? it->second : emptyValue;
}

// HttpResponseAccessor

HttpResponseAccessor::HttpResponseAccessor(HttpResponse message)
    : message_(std::move(message))
{
}

const std::string & HttpResponseAccessor::getHeader(std::string_view name) const
{
    return getHeaderImpl(message_.headers, name);
}

const std::string & HttpResponseAccessor::getHeader(HttpHeader::NameCode code) const
{
    return getHeaderImpl(message_.headers, code);
}

} // namespace nitrocoro::http
