/**
 * @file RouterCore.h
 * @brief Pure route matching logic without handler types
 */
#pragma once

#include <nitrocoro/http/HttpTypes.h>

#include <map>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nitrocoro::http
{

using PathParams = std::unordered_map<std::string, std::string>;

/**
 * @brief Pure route matching engine that separates routing logic from handler types.
 *
 * This class contains all the complex routing logic (exact match, path parameters,
 * wildcards, regex) but is completely agnostic to handler types. It returns route IDs
 * instead of handlers, allowing different router types to manage their own handler mappings.
 */
class RouterCore
{
public:
    struct MatchResult
    {
        bool matched = false;
        size_t routeId = 0;         // Unique route identifier
        PathParams params;          // Path parameters {:id -> "123"}
        std::string allowedMethods; // For 405 Method Not Allowed responses
    };

    /**
     * @brief Register a route pattern and return its unique ID.
     * @param path Route pattern (e.g., "/users/:id", "/files/*path")
     * @param methods Allowed HTTP methods for this route
     * @return Unique route ID for handler mapping
     */
    size_t addRoute(const std::string & path, const std::vector<HttpMethod> & methods);

    /**
     * @brief Register a regex route pattern and return its unique ID.
     * @param pattern Regex pattern for full path matching
     * @param methods Allowed HTTP methods for this route
     * @return Unique route ID for handler mapping
     */
    size_t addRouteRegex(const std::string & pattern, const std::vector<HttpMethod> & methods);

    /**
     * @brief Match a request and return route ID with parameters.
     * @param method HTTP method
     * @param path Request path
     * @return Match result with route ID and extracted parameters
     */
    MatchResult match(HttpMethod method, const std::string & path) const;

private:
    struct Entry
    {
        size_t routeId{ 0 };
        std::unordered_map<HttpMethod, bool> methods;
        std::string allowedMethods;
    };

    struct RadixNode;
    using RadixNodeMap = std::map<std::string, std::unique_ptr<RadixNode>, std::less<>>;
    struct RadixNode
    {
        Entry entry;
        RadixNodeMap children;         // static segments
        RadixNodeMap paramChildren;    // key = param name (:id → node)
        RadixNodeMap wildcardChildren; // key = wildcard name (*path → node)
    };

    struct RegexEntry
    {
        std::string pattern;
        std::regex regex;
        Entry entry;
    };

    static void addMethodToEntry(Entry & entry, HttpMethod method);
    static void insertRadix(RadixNode & node, std::string_view path, const std::vector<HttpMethod> & methods, size_t routeId);
    static const Entry * matchRadix(const RadixNode & node, std::string_view path, PathParams & params, size_t depth = 0);

    std::unordered_map<std::string, Entry> exactRoutes_;
    RadixNode radixRoot_;
    std::vector<RegexEntry> regexRoutes_;
    size_t nextRouteId_ = 1;
};

} // namespace nitrocoro::http
