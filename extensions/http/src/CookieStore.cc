#include <nitrocoro/http/CookieStore.h>

#include <algorithm>
#include <ctime>

namespace nitrocoro::http
{

static std::chrono::system_clock::time_point parseHttpDate(const std::string & s)
{
    std::tm tm{};
    // Try RFC 1123: "Thu, 01 Jan 1970 00:00:00 GMT"
    if (strptime(s.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm))
        return std::chrono::system_clock::from_time_t(timegm(&tm));
    return std::chrono::system_clock::time_point::min();
}

bool MemoryCookieStore::isExpired(const Entry & e) const
{
    return std::chrono::system_clock::now() >= e.expiresAt;
}

void MemoryCookieStore::store(std::string_view path, const std::vector<Cookie> & cookies)
{
    auto now = std::chrono::system_clock::now();

    // Clean up expired entries
    std::erase_if(entries_, [&](const Entry & e) { return isExpired(e); });

    for (const auto & cookie : cookies)
    {
        if (cookie.name.empty())
            continue;

        // Remove existing entry with same name+path
        std::erase_if(entries_, [&](const Entry & e) {
            return e.cookie.name == cookie.name && e.cookie.path == cookie.path;
        });

        // Max-Age=0 means delete
        if (cookie.maxAge == 0)
            continue;

        Entry entry;
        entry.cookie = cookie;

        // Default path to the request path directory
        if (entry.cookie.path.empty())
        {
            size_t slash = std::string(path).rfind('/');
            entry.cookie.path = (slash == std::string::npos) ? "/" : std::string(path).substr(0, slash + 1);
        }

        if (cookie.maxAge > 0)
        {
            entry.expiresAt = now + std::chrono::seconds(cookie.maxAge);
        }
        else if (!cookie.expires.empty())
        {
            entry.expiresAt = parseHttpDate(cookie.expires);
        }
        else
        {
            entry.expiresAt = std::chrono::system_clock::time_point::max();
        }

        entries_.push_back(std::move(entry));
    }
}

std::vector<Cookie> MemoryCookieStore::load(std::string_view path) const
{
    std::vector<Cookie> result;
    for (const auto & e : entries_)
    {
        if (isExpired(e))
            continue;
        const std::string & cookiePath = e.cookie.path;
        if (!path.starts_with(cookiePath))
            continue;
        if (cookiePath.back() != '/' && path.size() > cookiePath.size() && path[cookiePath.size()] != '/')
            continue;
        result.push_back(e.cookie);
    }
    return result;
}

} // namespace nitrocoro::http
