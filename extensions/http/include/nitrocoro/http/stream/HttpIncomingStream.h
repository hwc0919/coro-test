/**
 * @file HttpIncomingStream.h
 * @brief HTTP incoming stream for reading requests and responses
 */
#pragma once
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/BodyReader.h>
#include <nitrocoro/http/HttpDataAccessor.h>
#include <nitrocoro/http/HttpMessage.h>

#include <memory>
#include <string>

namespace nitrocoro::http
{

namespace detail
{

template <typename DataType>
class HttpIncomingStreamBase
{
public:
    HttpIncomingStreamBase(DataType message, std::shared_ptr<BodyReader> bodyReader)
        : data_(std::move(message)), bodyReader_(std::move(bodyReader)) {}

    const DataType & getData() const { return data_; }

    Task<size_t> read(char * buf, size_t maxLen);
    Task<std::string> read(size_t maxLen);

    template <utils::ExtendableBuffer T>
    Task<size_t> readToEnd(T & buf)
    {
        co_return co_await bodyReader_->readToEnd(buf);
    }

protected:
    DataType data_;
    std::shared_ptr<BodyReader> bodyReader_;
};

} // namespace detail

// Forward declaration
template <typename T>
class HttpIncomingStream;

class HttpCompleteResponse;

// ============================================================================
// HttpIncomingStream<HttpRequest> - Read HTTP Request
// ============================================================================

template <>
class HttpIncomingStream<HttpRequest>
    : public HttpRequestAccessor<HttpIncomingStream<HttpRequest>>,
      public detail::HttpIncomingStreamBase<HttpRequest>
{
public:
    using detail::HttpIncomingStreamBase<HttpRequest>::HttpIncomingStreamBase;

    // Task<HttpCompleteRequest> toCompleteRequest();
};

// ============================================================================
// HttpIncomingStream<HttpResponse> - Read HTTP Response
// ============================================================================

template <>
class HttpIncomingStream<HttpResponse>
    : public HttpResponseAccessor<HttpIncomingStream<HttpResponse>>,
      public detail::HttpIncomingStreamBase<HttpResponse>
{
public:
    using detail::HttpIncomingStreamBase<HttpResponse>::HttpIncomingStreamBase;

    Task<HttpCompleteResponse> toCompleteResponse();
};

} // namespace nitrocoro::http
