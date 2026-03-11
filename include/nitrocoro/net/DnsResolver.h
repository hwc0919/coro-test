/**
 * @file DnsResolver.h
 * @brief Asynchronous DNS resolver using thread pool
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>
#include <nitrocoro/utils/TaskQueue.h>

#include <chrono>
#include <memory>
#include <string>

namespace nitrocoro::net
{

class DnsResolver
{
public:
    explicit DnsResolver(std::chrono::seconds ttl = std::chrono::seconds(60),
                         TaskQueueProvider newTaskQueue = defaultTaskQueueProvider());
    ~DnsResolver();

    DnsResolver(const DnsResolver &) = delete;
    DnsResolver & operator=(const DnsResolver &) = delete;

    Task<std::vector<InetAddress>> resolve(const std::string & hostname,
                                           const std::string & service = "",
                                           Scheduler * scheduler = Scheduler::current());
    Task<std::vector<InetAddress>> resolve(const std::string & hostname,
                                           int family,
                                           Scheduler * scheduler = Scheduler::current());

    struct State;

private:
    using Addresses = std::vector<InetAddress>;

    static std::string cacheKey(const std::string & hostname, const std::string & service, int family);
    Task<Addresses> resolveImpl(const std::string & hostname,
                                const std::string & service,
                                int family,
                                Scheduler * scheduler);

    std::shared_ptr<TaskQueue> taskQueue_;
    std::shared_ptr<State> state_;
};

} // namespace nitrocoro::net
