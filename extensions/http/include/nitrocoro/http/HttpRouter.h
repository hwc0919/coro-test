/**
 * @file HttpRouter.h
 * @brief HTTP request router
 */
#pragma once

#include <nitrocoro/http/HttpHandler.h>

#include <map>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nitrocoro::http
{

/**
 * @brief HTTP request router with three-tier matching.
 *
 * Routes are matched in the following priority order:
 *
 * 1. **Exact match** — registered via `addRoute()` with a static path.
 *    Matched in O(1). `params` is empty on match.
 *    @code
 *    router.addRoute("GET", "/users/me", handler);
 *    // GET /users/me  →  params: {}
 *    @endcode
 *
 * 2. **Path parameters** (`:name`) — registered via `addRoute()`. Each `:name`
 *    segment matches exactly one path segment (no `/`). Captured into `params`
 *    by name. Multiple parameters per route are supported.
 *    @code
 *    router.addRoute("GET", "/users/:id", handler);
 *    // GET /users/42          →  params: {"id": "42"}
 *    // GET /users/42/profile  →  no match (segment count mismatch)
 *
 *    router.addRoute("GET", "/users/:uid/posts/:pid", handler);
 *    // GET /users/1/posts/99  →  params: {"uid": "1", "pid": "99"}
 *    @endcode
 *
 * 3. **Wildcard** (`*name`) — registered via `addRoute()`. Must appear at the
 *    end of the pattern. Captures all remaining segments including `/`.
 *    @code
 *    router.addRoute("GET", "/files/*path", handler);
 *    // GET /files/a/b/c.txt  →  params: {"path": "a/b/c.txt"}
 *    @endcode
 *
 * 4. **Regex** — registered via `addRouteRegex()`. Full path match via
 *    `std::regex_match`. Capture groups are exposed as `$1`, `$2`, etc.
 *    Evaluated last; linear scan over all registered regex routes.
 *    @code
 *    router.addRouteRegex("GET", R"(/items/(\d+))", handler);
 *    // GET /items/123  →  params: {"$1": "123"}
 *    @endcode
 *
 * When no route matches, `route()` returns a `RouteResult` with a null handler.
 *
 * ## Security limits
 * - Paths longer than 2048 bytes are rejected (prevents ReDoS on regex routes).
 * - Radix tree matching is limited to 32 path segments (prevents CWE-674 stack overflow).
 *
 * ## Handler signatures
 * Any callable returning `Task<>` is accepted. Supported parameter forms:
 * @code
 * // Full signature
 * [](HttpIncomingStream<HttpRequest> req, HttpOutgoingStream<HttpResponse> resp, Params p) -> Task<> {}
 * // Without params
 * [](HttpIncomingStream<HttpRequest> req, HttpOutgoingStream<HttpResponse> resp) -> Task<> {}
 * // Response only
 * [](HttpOutgoingStream<HttpResponse> resp) -> Task<> {}
 * @endcode
 */

class HttpRouter
{
public:
    struct RouteResult
    {
        HttpHandlerPtr handler;
        Params params;

        explicit operator bool() const { return handler != nullptr; }
    };

    template <typename F>
    void addRoute(const std::string & method, const std::string & path, F && handler);
    template <typename F>
    void addRouteRegex(const std::string & method, const std::string & pattern, F && handler);

    // Returns {handler, params} for the matched route, or {nullptr, {}} if not found.
    RouteResult route(const std::string & method, const std::string & path) const;

private:
    struct RouteNode
    {
        HttpHandlerPtr handler;
        std::map<std::string, std::unique_ptr<RouteNode>, std::less<>> children; // static segments
        std::unique_ptr<RouteNode> paramChild;                                   // :name
        std::string paramName;
        std::unique_ptr<RouteNode> wildcardChild; // *name
        std::string wildcardName;
    };

    struct MethodRoutes
    {
        std::unordered_map<std::string, HttpHandlerPtr> exact;
        RouteNode radixRoot;
        std::vector<std::pair<std::regex, HttpHandlerPtr>> regexRoutes;
    };

    void addRouteImpl(const std::string & method, const std::string & path, HttpHandlerPtr handler);

    static void insertRadix(RouteNode & node, std::string_view path, HttpHandlerPtr handler);
    static HttpHandlerPtr matchRadix(const RouteNode & node, std::string_view path, Params & params, size_t depth = 0);

    std::unordered_map<std::string, MethodRoutes> routes_;
};

template <typename F>
void HttpRouter::addRoute(const std::string & method, const std::string & path, F && handler)
{
    addRouteImpl(method, path, makeHttpHandler(std::forward<F>(handler)));
}

template <typename F>
void HttpRouter::addRouteRegex(const std::string & method, const std::string & pattern, F && handler)
{
    routes_[method].regexRoutes.emplace_back(std::regex(pattern),
                                             makeHttpHandler(std::forward<F>(handler)));
}

} // namespace nitrocoro::http
