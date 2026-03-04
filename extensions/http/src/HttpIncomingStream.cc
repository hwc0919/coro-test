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

template <typename DataType>
Task<size_t> HttpIncomingStreamBase<DataType>::read(char * buf, size_t len)
{
    co_return co_await bodyReader_->read(buf, len);
}

template <typename DataType>
Task<std::string> HttpIncomingStreamBase<DataType>::read(size_t maxLen)
{
    std::string result(maxLen, '\0');
    size_t n = co_await read(result.data(), maxLen);
    result.resize(n);
    co_return result;
}

// Explicit instantiations
template class HttpIncomingStreamBase<HttpRequest>;
template class HttpIncomingStreamBase<HttpResponse>;

} // namespace nitrocoro::http::detail

namespace nitrocoro::http
{

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    utils::StringBuffer bodyBuf;
    co_await readToEnd(bodyBuf);
    co_return HttpCompleteResponse(std::move(data_), bodyBuf.extract());
}

} // namespace nitrocoro::http
