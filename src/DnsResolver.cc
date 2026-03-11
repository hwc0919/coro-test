/**
 * @file DnsResolver.cc
 * @brief Asynchronous DNS resolver implementation
 */
#include <nitrocoro/net/DnsException.h>
#include <nitrocoro/net/DnsResolver.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace nitrocoro::net
{

using Addresses = std::vector<InetAddress>;
using TimePoint = std::chrono::steady_clock::time_point;

struct CacheEntry
{
    Addresses addresses;
    TimePoint expiry;
};

struct ExpiryEntry
{
    TimePoint expiry;
    std::string key;
    bool operator>(const ExpiryEntry & o) const { return expiry > o.expiry; }
};

struct DnsResolver::State
{
    std::mutex mutex;
    std::chrono::seconds ttl{ std::chrono::seconds(300) };
    std::atomic<uint32_t> writeCount{ 0 };
    std::unordered_map<std::string, CacheEntry> cache;
    std::unordered_map<std::string, std::vector<Promise<Addresses>>> pending;
    std::priority_queue<ExpiryEntry, std::vector<ExpiryEntry>, std::greater<>> expiryQueue;
};

static void doResolve(const std::weak_ptr<DnsResolver::State> & weakState,
                      const std::string & key,
                      const std::string & hostname,
                      const std::string & service,
                      int family)
{
    std::exception_ptr ex;
    std::vector<Promise<Addresses>> waiters;

    struct addrinfo hints = {};
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo * res = nullptr;
    int error = getaddrinfo(hostname.c_str(),
                            service.empty() ? nullptr : service.c_str(),
                            &hints,
                            &res);

    auto state = weakState.lock();
    if (!state)
    {
        return;
    }

    do
    {
        if (!res)
        {
            ex = std::make_exception_ptr(DnsException("no result", error));
            break;
        }
        if (error != 0)
        {
            freeaddrinfo(res);
#ifdef _WIN32
            ex = std::make_exception_ptr(DnsException(gai_strerrorA(error), error));
#else
            ex = std::make_exception_ptr(DnsException(gai_strerror(error), error));
#endif
            break;
        }

        Addresses addresses;
        for (struct addrinfo * p = res; p != nullptr; p = p->ai_next)
        {
            if (p->ai_family == AF_INET && p->ai_addr)
                addresses.emplace_back(*reinterpret_cast<struct sockaddr_in *>(p->ai_addr));
            else if (p->ai_family == AF_INET6 && p->ai_addr)
                addresses.emplace_back(*reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr));
        }
        freeaddrinfo(res);

        if (addresses.empty())
        {
            ex = std::make_exception_ptr(DnsException("no usable addresses", 0));
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto expiry = now + state->ttl;
        {
            std::lock_guard lock(state->mutex);
            state->cache[key] = { addresses, expiry };
            state->expiryQueue.push({ expiry, key });
            auto pendingIt = state->pending.find(key);
            if (pendingIt != state->pending.end())
            {
                waiters = std::move(pendingIt->second);
                state->pending.erase(pendingIt);
            }
        }
        if ((state->writeCount.fetch_add(1, std::memory_order_relaxed) & 15) == 0)
        {
            std::lock_guard lock(state->mutex);
            while (!state->expiryQueue.empty() && state->expiryQueue.top().expiry <= now)
            {
                auto & top = state->expiryQueue.top();
                auto cacheIt = state->cache.find(top.key);
                if (cacheIt != state->cache.end() && cacheIt->second.expiry <= now)
                    state->cache.erase(cacheIt);
                state->expiryQueue.pop();
            }
        }
        for (auto & p : waiters)
            p.set_value(addresses);
    } while (0);

    if (ex)
    {
        {
            std::lock_guard lock(state->mutex);
            auto pendingIt = state->pending.find(key);
            if (pendingIt != state->pending.end())
            {
                waiters = std::move(pendingIt->second);
                state->pending.erase(pendingIt);
            }
        }
        for (auto & p : waiters)
            p.set_exception(ex);
    }
}

DnsResolver::DnsResolver(std::chrono::seconds ttl, TaskQueueProvider newTaskQueue)
    : taskQueue_(newTaskQueue()), state_(std::make_shared<State>())
{
    state_->ttl = ttl;
}

DnsResolver::~DnsResolver() = default;

std::string DnsResolver::cacheKey(const std::string & hostname, const std::string & service, int family)
{
    return hostname + "|" + service + "|" + std::to_string(family);
}

Task<DnsResolver::Addresses> DnsResolver::resolveImpl(const std::string & hostname,
                                                      const std::string & service,
                                                      int family,
                                                      Scheduler * scheduler)
{
    const std::string key = cacheKey(hostname, service, family);

    Promise<Addresses> promise(scheduler);
    auto future = promise.get_future();
    bool shouldDispatch = false;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(state_->mutex);

        auto cacheIt = state_->cache.find(key);
        if (cacheIt != state_->cache.end() && now < cacheIt->second.expiry)
        {
            promise.set_value(cacheIt->second.addresses);
        }
        else if (auto pendingIt = state_->pending.find(key); pendingIt != state_->pending.end())
        {
            pendingIt->second.push_back(std::move(promise));
        }
        else
        {
            state_->pending[key].push_back(std::move(promise));
            shouldDispatch = true;
        }
    }

    if (shouldDispatch)
    {
        taskQueue_->post([weakState = std::weak_ptr(state_), key, hostname, service, family] {
            doResolve(weakState, key, hostname, service, family);
        });
    }

    co_return co_await future.get();
}

Task<DnsResolver::Addresses> DnsResolver::resolve(const std::string & hostname,
                                                  const std::string & service,
                                                  Scheduler * scheduler)
{
    co_return co_await resolveImpl(hostname, service, AF_UNSPEC, scheduler);
}

Task<DnsResolver::Addresses> DnsResolver::resolve(const std::string & hostname,
                                                  int family,
                                                  Scheduler * scheduler)
{
    co_return co_await resolveImpl(hostname, "", family, scheduler);
}

} // namespace nitrocoro::net
