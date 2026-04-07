/**
 * @file RouterCore.cc
 * @brief Pure route matching logic implementation
 */
#include <nitrocoro/http/RouterCore.h>
#include <stdexcept>

namespace nitrocoro::http
{

// ── Radix Tree helpers ────────────────────────────────────────────────────────

static std::string_view nextSegment(std::string_view path, size_t & pos)
{
    if (pos >= path.size())
        return {};
    if (path[pos] == '/')
        ++pos;
    size_t start = pos;
    while (pos < path.size() && path[pos] != '/')
        ++pos;
    return path.substr(start, pos - start);
}

void RouterCore::addMethodToEntry(Entry & entry, HttpMethod method)
{
    entry.methods[method] = true;
    if (method == methods::Get && !entry.methods.contains(methods::Head))
    {
        entry.methods[methods::Head] = true;
    }

    entry.allowedMethods.clear();
    for (const auto & [m, _] : entry.methods)
    {
        if (!entry.allowedMethods.empty())
        {
            entry.allowedMethods += ", ";
        }
        entry.allowedMethods += m.toString();
    }
}

void RouterCore::insertRadix(RadixNode & node, std::string_view path, const std::vector<HttpMethod> & methods, size_t routeId)
{
    size_t pos = 0;
    RadixNode * cur = &node;

    while (true)
    {
        std::string_view seg = nextSegment(path, pos);
        if (seg.empty())
        {
            cur->entry.routeId = routeId;
            for (const auto & m : methods)
            {
                addMethodToEntry(cur->entry, m);
            }
            return;
        }

        if (seg[0] == ':')
        {
            std::string name(seg.substr(1));
            auto & child = cur->paramChildren[name];
            if (!child)
                child = std::make_unique<RadixNode>();
            cur = child.get();
        }
        else if (seg[0] == '*')
        {
            std::string name(seg.substr(1));
            auto & child = cur->wildcardChildren[name];
            if (!child)
                child = std::make_unique<RadixNode>();
            child->entry.routeId = routeId;
            for (const auto & m : methods)
                addMethodToEntry(child->entry, m);
            return;
        }
        else
        {
            auto & child = cur->children[std::string(seg)];
            if (!child)
                child = std::make_unique<RadixNode>();
            cur = child.get();
        }
    }
}

static constexpr size_t kMaxPathLength = 2048;
static constexpr size_t kMaxPathSegments = 32;

const RouterCore::Entry * RouterCore::matchRadix(const RadixNode & node, std::string_view path, PathParams & params, size_t depth)
{
    if (depth > kMaxPathSegments)
        return nullptr;
    size_t pos = 0;
    const RadixNode * cur = &node;

    std::string_view seg = nextSegment(path, pos);
    if (seg.empty())
    {
        // Empty remaining path can still match a wildcard (e.g. /static/ → *path = "")
        for (const auto & [wname, wnode] : cur->wildcardChildren)
        {
            if (!wnode->entry.methods.empty())
            {
                params[wname] = "";
                return &wnode->entry;
            }
        }
        return cur->entry.methods.empty() ? nullptr : &cur->entry;
    }

    // 1. static
    auto it = cur->children.find(seg);
    if (it != cur->children.end())
    {
        if (auto * entry = matchRadix(*it->second, path.substr(pos), params, depth + 1))
            return entry;
    }

    // 2. param — try all named param branches
    for (const auto & [pname, pnode] : cur->paramChildren)
    {
        params[pname] = std::string(seg);
        if (auto * entry = matchRadix(*pnode, path.substr(pos), params, depth + 1))
            return entry;
        params.erase(pname);
    }

    // 3. wildcard — try all named wildcard branches
    for (const auto & [wname, wnode] : cur->wildcardChildren)
    {
        if (!wnode->entry.methods.empty())
        {
            params[wname] = std::string(path.substr(pos - seg.size()));
            return &wnode->entry;
        }
    }

    return nullptr;
}

// ── Public API ────────────────────────────────────────────────────────────────

size_t RouterCore::addRoute(const std::string & path, const std::vector<HttpMethod> & methods)
{
    for (const auto & m : methods)
        if (m == methods::_Invalid)
            throw std::invalid_argument("RouterCore: invalid HTTP method");

    size_t routeId = nextRouteId_++;

    auto isParamOrWild = [](std::string_view p, char c) {
        if (!p.empty() && p[0] == c)
            return true;
        for (size_t i = 1; i < p.size(); ++i)
            if (p[i] == c && p[i - 1] == '/')
                return true;
        return false;
    };
    bool hasParam = isParamOrWild(path, ':');
    bool hasWild = isParamOrWild(path, '*');

    if (!hasParam && !hasWild)
    {
        auto & entry = exactRoutes_[path];
        entry.routeId = routeId;
        for (const auto & m : methods)
            addMethodToEntry(entry, m);
    }
    else
    {
        insertRadix(radixRoot_, path, methods, routeId);
    }

    return routeId;
}

size_t RouterCore::addRouteRegex(const std::string & pattern, const std::vector<HttpMethod> & methods)
{
    for (const auto & m : methods)
        if (m == methods::_Invalid)
            throw std::invalid_argument("RouterCore: invalid HTTP method");

    size_t routeId = nextRouteId_++;

    for (auto & r : regexRoutes_)
    {
        if (r.pattern == pattern)
        {
            r.entry.routeId = routeId;
            for (const auto & method : methods)
                addMethodToEntry(r.entry, method);
            return routeId;
        }
    }

    Entry entry;
    entry.routeId = routeId;
    for (const auto & method : methods)
        addMethodToEntry(entry, method);
    regexRoutes_.push_back({ pattern, std::regex(pattern), std::move(entry) });

    return routeId;
}

RouterCore::MatchResult RouterCore::match(HttpMethod method, const std::string & path) const
{
    if (path.size() > kMaxPathLength)
        return {};

    auto lookupMethod = [&](const Entry & entry) -> MatchResult {
        auto it = entry.methods.find(method);
        if (it != entry.methods.end())
            return { true, entry.routeId, {}, {} };
        return { false, 0, {}, entry.allowedMethods };
    };

    // 1. exact
    auto exactIt = exactRoutes_.find(path);
    if (exactIt != exactRoutes_.end())
    {
        return lookupMethod(exactIt->second);
    }

    // 2. radix (param / wildcard)
    PathParams params;
    if (const Entry * entry = matchRadix(radixRoot_, path, params))
    {
        auto r = lookupMethod(*entry);
        r.params = std::move(params);
        return r;
    }

    // 3. regex
    for (const auto & r : regexRoutes_)
    {
        std::smatch m;
        if (std::regex_match(path, m, r.regex))
        {
            PathParams regexParams;
            for (size_t i = 1; i < m.size(); ++i)
                regexParams["$" + std::to_string(i)] = m[i].str();
            auto result = lookupMethod(r.entry);
            result.params = std::move(regexParams);
            return result;
        }
    }

    return {};
}

} // namespace nitrocoro::http