/**
 * @file Cookie.h
 * @brief HTTP Cookie data structure
 */
#pragma once

#include <chrono>
#include <string>

namespace nitrocoro::http
{

struct Cookie
{
    enum class SameSite
    {
        Unset,
        Strict,
        Lax,
        None,
    };

    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    std::string expires; // HTTP-date format, empty means not set
    int maxAge = -1;     // -1 means not set
    SameSite sameSite = SameSite::Unset;
    bool secure = false;
    bool httpOnly = false;

    static Cookie fromString(std::string_view);
    static std::string formatExpires(std::chrono::system_clock::time_point tp);
    std::string toString() const;
};

} // namespace nitrocoro::http
