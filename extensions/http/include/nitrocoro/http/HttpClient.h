/**
 * @file HttpClient.h
 * @brief HTTP client for making requests
 */
#pragma once
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/http/CookieStore.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/HttpStream.h>

#include <nitrocoro/core/Task.h>
#include <nitrocoro/io/Stream.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/Url.h>

#include <chrono>
#include <deque>
#include <string>
#include <vector>

namespace nitrocoro::http
{

struct HttpClientConfig
{
    size_t max_idle_connections = 8;
    std::chrono::seconds idle_timeout = std::chrono::seconds(60);
    bool add_host_header = true;
    CookieStoreFactory cookieStoreFactory = nullptr;
};

class HttpClient
{
public:
    using StreamUpgrader = std::function<Task<io::StreamPtr>(net::TcpConnectionPtr)>;

    explicit HttpClient(std::string baseUrl, HttpClientConfig config = {});
    explicit HttpClient(net::Url url, HttpClientConfig config = {});

    virtual ~HttpClient();

    HttpClient(const HttpClient &) = delete;
    HttpClient & operator=(const HttpClient &) = delete;
    HttpClient(HttpClient &&) = delete;
    HttpClient & operator=(HttpClient &&) = delete;

    void setStreamUpgrader(StreamUpgrader upgrader) { upgrader_ = std::move(upgrader); }

    Task<HttpCompleteResponse> get(std::string path);
    Task<HttpCompleteResponse> post(std::string path, std::string body);
    Task<IncomingResponse> request(ClientRequest req);

protected:
    struct IdleConnection
    {
        io::StreamPtr stream;
        std::chrono::steady_clock::time_point idleAt;
    };

    Task<io::StreamPtr> acquireConnection();
    Task<> releaseConnection(io::StreamPtr stream);
    Task<io::StreamPtr> connect();
    void injectCookies(ClientRequest & req, const std::string & path);
    void collectCookies(const std::string & path, const std::vector<Cookie> & cookies);

    net::Url baseUrl_;
    HttpClientConfig config_;
    StreamUpgrader upgrader_;
    bool isHttps_;
    Mutex cookieMutex_;
    std::unique_ptr<CookieStore> cookieStore_;
    Mutex mutex_;
    std::deque<IdleConnection> idleConnections_;
};

Task<HttpCompleteResponse> get(std::string url);
Task<HttpCompleteResponse> post(std::string url, std::string body);
Task<IncomingResponse> request(std::string url, ClientRequest req);
Task<IncomingResponse> requestStream(std::string url, ClientRequest req);

} // namespace nitrocoro::http
