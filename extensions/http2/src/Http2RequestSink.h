/**
 * @file Http2RequestSink.h
 * @brief HTTP/2 RequestSink implementation
 */
#pragma once
#include <nitrocoro/http/RequestSink.h>

#include <cstdint>
#include <memory>

namespace nitrocoro::http2
{

class Http2ClientSession;

class Http2RequestSink : public http::RequestSink
{
public:
    Http2RequestSink(Http2ClientSession & session, uint32_t streamId);

    Task<> write(const http::HttpRequest & req, std::string_view body) override;
    Task<> write(const http::HttpRequest & req, const http::BodyWriterFn & bodyWriterFn) override;

private:
    Http2ClientSession & session_;
    uint32_t streamId_;
};

} // namespace nitrocoro::http2
