/**
 * @file HttpIncomingStream.cc
 * @brief HTTP incoming stream implementations
 */
#include <nitrocoro/http/BodyReader.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/stream/HttpIncomingStream.h>

namespace nitrocoro::http::detail
{

// ============================================================================
// HttpIncomingStreamBase Implementation
// ============================================================================

Task<size_t> HttpIncomingStreamBase::read(char * buf, size_t len)
{
    co_return co_await bodyReader_->read(buf, len);
}

Task<std::string> HttpIncomingStreamBase::read(size_t maxLen)
{
    std::string result(maxLen, '\0');
    size_t n = co_await read(result.data(), maxLen);
    result.resize(n);
    co_return result;
}

} // namespace nitrocoro::http::detail

namespace nitrocoro::http
{

Task<HttpCompleteRequest> HttpIncomingStream<HttpRequest>::toCompleteRequest()
{
    utils::StringBuffer bodyBuf;
    co_await readToEnd(bodyBuf);
    co_return HttpCompleteRequest(std::move(message_), bodyBuf.extract());
}

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    utils::StringBuffer bodyBuf;
    co_await readToEnd(bodyBuf);
    co_return HttpCompleteResponse(std::move(message_), bodyBuf.extract());
}

} // namespace nitrocoro::http
