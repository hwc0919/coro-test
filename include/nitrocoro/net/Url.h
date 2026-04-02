/**
 * @file Url.h
 * @brief URL parser for network requests
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace nitrocoro::net
{

class Url
{
public:
    Url() = default;
    explicit Url(std::string_view url);

    const std::string & baseUrl() const { return baseUrl_; }
    const std::string & scheme() const { return scheme_; }
    const std::string & host() const { return host_; }
    uint16_t port() const { return port_; }
    const std::string & path() const { return path_; }
    const std::string & query() const { return query_; }
    const std::string & fullPath() const { return fullPath_; }

    bool isValid() const { return valid_; }

private:
    void parse(std::string_view url);
    uint16_t getDefaultPort() const;

    std::string baseUrl_;
    std::string scheme_;
    std::string host_;
    uint16_t port_ = 0;
    std::string path_;
    std::string query_;
    std::string fullPath_;
    bool valid_ = false;
};

} // namespace nitrocoro::net
