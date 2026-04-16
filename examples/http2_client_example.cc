/**
 * @file http2_client_example.cc
 * @brief Simple example demonstrating HTTP/2 client usage
 */
#include <nitrocoro/http2/Http2Client.h>
#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/utils/Debug.h>

#include <iostream>

using namespace nitrocoro;
using namespace nitrocoro::http2;

Task<> example()
{
    try
    {
        // Create HTTP/2 client with TLS configuration
        http2::Http2ClientConfig config;
        config.tls_policy.validate = true;
        config.allow_http1_fallback = true; // Allow fallback to HTTP/1.1 if needed
        
        Http2Client client("https://httpbin.org", config);
        
        // Make a simple GET request
        auto response = co_await client.get("/get?param=value");
        
        std::cout << "Status: " << response.statusCode() << std::endl;
        std::cout << "Body: " << response.body() << std::endl;
        
        // Make a POST request
        http::ClientRequest req;
        req.setMethod(http::HttpMethod::Post);
        req.setPath("/post");
        req.setBody(R"({"message": "Hello HTTP/2!"})");
        
        // Add content-type header
        req.setHeader(http::HttpHeader::NameCode::ContentType, "application/json");
        
        auto streamResponse = co_await client.request(std::move(req));
        auto completeResponse = co_await streamResponse.toCompleteResponse();
        
        std::cout << "POST Status: " << completeResponse.statusCode() << std::endl;
        std::cout << "POST Body length: " << completeResponse.body().size() << std::endl;
    }
    catch (const std::exception & e)
    {
        NITRO_ERROR("HTTP/2 client example failed: %s", e.what());
    }
}

int main()
{
    Scheduler scheduler;
    scheduler.spawn([]() -> Task<> {
        co_await example();
    });
    scheduler.run();
    return 0;
}