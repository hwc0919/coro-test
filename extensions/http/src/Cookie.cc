#include <nitrocoro/http/Cookie.h>
#include <nitrocoro/http/HttpHeader.h>

#include <ctime>

namespace nitrocoro::http
{

Cookie Cookie::fromString(std::string_view raw)
{
    auto trim = [](std::string_view s) {
        size_t l = s.find_first_not_of(' ');
        size_t r = s.find_last_not_of(' ');
        return (l == std::string_view::npos) ? std::string_view{} : s.substr(l, r - l + 1);
    };

    Cookie cookie;

    size_t semiPos = raw.find(';');
    std::string_view first = trim(raw.substr(0, semiPos));
    size_t eqPos = first.find('=');
    if (eqPos == std::string_view::npos)
    {
        return cookie;
    }

    cookie.name = trim(first.substr(0, eqPos));
    cookie.value = trim(first.substr(eqPos + 1));
    if (cookie.name.empty())
    {
        return cookie;
    }

    size_t start = (semiPos == std::string_view::npos) ? raw.size() : semiPos + 1;
    while (start < raw.size())
    {
        semiPos = raw.find(';', start);
        size_t end = (semiPos == std::string_view::npos) ? raw.size() : semiPos;
        std::string_view token = trim(raw.substr(start, end - start));

        auto tokenLower = HttpHeader::toLower(token);
        if (tokenLower == "secure")
            cookie.secure = true;
        else if (tokenLower == "httponly")
            cookie.httpOnly = true;
        else
        {
            size_t eq = token.find('=');
            if (eq != std::string_view::npos)
            {
                auto attrName = HttpHeader::toLower(trim(token.substr(0, eq)));
                auto attrValue = trim(token.substr(eq + 1));
                if (attrName == "path")
                    cookie.path = attrValue;
                else if (attrName == "domain")
                    cookie.domain = attrValue;
                else if (attrName == "expires")
                    cookie.expires = attrValue;
                else if (attrName == "max-age")
                {
                    try
                    {
                        cookie.maxAge = std::stoi(std::string(attrValue));
                    }
                    catch (...)
                    {
                    }
                }
                else if (attrName == "samesite")
                {
                    auto v = HttpHeader::toLower(attrValue);
                    if (v == "strict")
                        cookie.sameSite = SameSite::Strict;
                    else if (v == "lax")
                        cookie.sameSite = SameSite::Lax;
                    else if (v == "none")
                        cookie.sameSite = SameSite::None;
                }
            }
        }

        if (semiPos == std::string_view::npos)
            break;
        start = semiPos + 1;
    }

    return cookie;
}

std::string Cookie::formatExpires(std::chrono::system_clock::time_point tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    return buf;
}

std::string Cookie::toString() const
{
    std::string s;
    s.append(name).append("=").append(value);
    if (!expires.empty())
        s.append("; Expires=").append(expires);
    if (maxAge >= 0)
        s.append("; Max-Age=").append(std::to_string(maxAge));
    if (!domain.empty())
        s.append("; Domain=").append(domain);
    if (!path.empty())
        s.append("; Path=").append(path);
    if (secure)
        s.append("; Secure");
    if (httpOnly)
        s.append("; HttpOnly");
    switch (sameSite)
    {
        case SameSite::Strict:
            s.append("; SameSite=Strict");
            break;
        case SameSite::Lax:
            s.append("; SameSite=Lax");
            break;
        case SameSite::None:
            s.append("; SameSite=None");
            break;
        default:
            break;
    }
    return s;
}

} // namespace nitrocoro::http
