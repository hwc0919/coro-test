/**
 * @file Http2ClientStream.h
 * @brief Per-stream state for an HTTP/2 client connection
 */
#pragma once

#include "hpack/Hpack.h"
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Pipe.h>
#include <nitrocoro/http/HttpMessage.h>

namespace nitrocoro::http2
{

// Lifecycle of a single HTTP/2 stream from the client's perspective.
// Created when sending a request, completed when response is fully received.
struct Http2ClientStream
{
    uint32_t streamId;

    // Response promise - resolved when headers are received
    Promise<http::HttpResponse> responsePromise;
    SharedFuture<http::HttpResponse> responseFuture;

    // Filled by HEADERS/CONTINUATION frames
    hpack::DecodedHeaders decodedHeaders;
    bool headersComplete{ false };

    // Response body data frames are pushed here; closed on END_STREAM
    std::shared_ptr<PipeSender<std::string>> bodySender;
    std::unique_ptr<PipeReceiver<std::string>> bodyReceiver;

    explicit Http2ClientStream(uint32_t id, Scheduler * sched)
        : streamId(id)
        , responsePromise(sched)
        , responseFuture(responsePromise.get_future().share())
    {
        auto [tx, rx] = makePipe<std::string>(sched);
        bodySender = std::move(tx);
        bodyReceiver = std::move(rx);
    }
};

} // namespace nitrocoro::http2