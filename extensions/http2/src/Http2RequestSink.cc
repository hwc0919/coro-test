/**
 * @file Http2RequestSink.cc
 * @brief HTTP/2 RequestSink implementation
 */
#include "Http2RequestSink.h"

#include "Http2ClientSession.h"

namespace nitrocoro::http2
{

namespace
{

class Http2RequestBodyWriter : public http::BodyWriter
{
public:
    Http2RequestBodyWriter(Http2ClientSession & session, uint32_t streamId)
        : session_(session), streamId_(streamId)
    {
    }

    Task<> write(std::string_view data) override
    {
        co_await session_.sendData(streamId_, data, false);
    }

private:
    Http2ClientSession & session_;
    uint32_t streamId_;
};

} // namespace

Http2RequestSink::Http2RequestSink(Http2ClientSession & session, uint32_t streamId)
    : session_(session), streamId_(streamId)
{
}

Task<> Http2RequestSink::write(const http::HttpRequest & req, std::string_view body)
{
    co_await session_.sendHeaders(streamId_, req, body.empty());
    if (!body.empty())
        co_await session_.sendData(streamId_, body, true);
}

Task<> Http2RequestSink::write(const http::HttpRequest & req, const http::BodyWriterFn & bodyWriterFn)
{
    co_await session_.sendHeaders(streamId_, req, false);
    Http2RequestBodyWriter writer(session_, streamId_);
    co_await bodyWriterFn(writer);
    co_await session_.sendData(streamId_, {}, true);
}

} // namespace nitrocoro::http2
