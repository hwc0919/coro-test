/**
 * @file HttpRouter.cc
 * @brief HTTP request router implementation
 */
#include <nitrocoro/http/HttpRouter.h>

namespace nitrocoro::http
{

HttpRouter::RouteResult HttpRouter::route(HttpMethod method, const std::string & path) const
{
    auto result = core_.match(method, path);
    
    if (result.matched)
    {
        if (result.routeId < handlers_.size() && handlers_[result.routeId])
        {
            return { handlers_[result.routeId], std::move(result.params), RouteResult::Reason::Ok, {} };
        }
    }
    
    if (!result.allowedMethods.empty())
    {
        return { nullptr, {}, RouteResult::Reason::MethodNotAllowed, result.allowedMethods };
    }
    
    return { nullptr, {}, RouteResult::Reason::NotFound, {} };
}

} // namespace nitrocoro::http
