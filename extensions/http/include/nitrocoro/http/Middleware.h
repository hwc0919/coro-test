/**
 * @file Middleware.h
 * @brief HTTP middleware types
 */
#pragma once
#include <nitrocoro/http/HttpHandler.h>
#include <nitrocoro/http/HttpStream.h>

#include <nitrocoro/core/Task.h>

#include <functional>

namespace nitrocoro::http
{

using NextHandler = std::function<Task<>()>;
using Middleware = std::function<Task<>(IncomingRequestPtr, ServerResponsePtr, NextHandler)>;

} // namespace nitrocoro::http
