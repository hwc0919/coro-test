/**
 * @file CookieStore.h
 * @brief Cookie storage interface and in-memory implementation
 */
#pragma once

#include <nitrocoro/http/Cookie.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace nitrocoro::http
{

class CookieStore
{
public:
    virtual ~CookieStore() = default;

    virtual void store(std::string_view path, const std::vector<Cookie> & cookies) = 0;
    virtual std::vector<Cookie> load(std::string_view path) const = 0;
};

class MemoryCookieStore : public CookieStore
{
public:
    void store(std::string_view path, const std::vector<Cookie> & cookies) override;
    std::vector<Cookie> load(std::string_view path) const override;

private:
    struct Entry
    {
        Cookie cookie;
        std::chrono::system_clock::time_point expiresAt;
    };

    bool isExpired(const Entry & e) const;

    std::vector<Entry> entries_;
};

using CookieStoreFactory = std::function<std::unique_ptr<CookieStore>()>;

inline CookieStoreFactory memoryCookieStore()
{
    return []() { return std::make_unique<MemoryCookieStore>(); };
}

} // namespace nitrocoro::http
