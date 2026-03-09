/**
 * @file pg_press_test.cc
 * @brief SELECT pressure test: fixed worker coroutines each looping SELECT.
 *        Prints qps and alive count every second.
 *
 * Usage:
 *   PG_CONN_STR="host=localhost dbname=test user=postgres" ./pg_press_test
 *   ./pg_press_test --duration=10 --pool-size=8 --workers=32
 */
#include "pg_test_utils.h"
#include <nitrocoro/core/CancelToken.h>
#include <nitrocoro/pg/PgException.h>
#include <nitrocoro/pg/PgPool.h>
#include <nitrocoro/testing/Test.h>

#include <atomic>
#include <cstring>

using namespace nitrocoro;
using namespace nitrocoro::pg;
using namespace nitrocoro::pg::test;

static int g_durationSec = 1;
static int g_poolSize = 4;
static int g_workers = 8;

NITRO_TEST(pg_press)
{
    PgPool pool(makePoolConfig(static_cast<size_t>(g_poolSize)));

    {
        auto conn = co_await pool.acquire();
        co_await conn->execute("DROP TABLE IF EXISTS press_test");
        co_await conn->execute("CREATE TABLE press_test (id BIGSERIAL PRIMARY KEY, val TEXT NOT NULL)");
        for (int i = 0; i < 100; ++i)
        {
            std::vector<PgValue> params{ std::string("seed-") + std::to_string(i) };
            co_await conn->execute("INSERT INTO press_test (val) VALUES ($1)", std::move(params));
        }
        NITRO_INFO("insert finish");
    }

    CancelSource stop(Scheduler::current());
    stop.cancelAfter(std::chrono::seconds(g_durationSec));

    std::atomic<uint64_t> completed{ 0 };
    std::atomic<uint64_t> total{ 0 };
    std::atomic<int64_t> alive{ 0 };

    Promise<> allStared(Scheduler::current());
    Promise<> allDone(Scheduler::current());
    std::atomic<int> remaining{ g_workers };

    int started = 0;
    for (int i = 0; i < g_workers; ++i)
    {
        Scheduler::current()->spawn([&, TEST_CTX, token = stop.token()]() mutable -> Task<> {
            ++alive;
            if (++started == g_workers)
            {
                NITRO_INFO("all workers started");
                allStared.set_value();
            }
            while (!token.isCancelled())
            {
                try
                {
                    auto conn = co_await pool.acquire(token);
                    co_await conn->query("SELECT id, val FROM press_test LIMIT 10", token);
                    ++completed;
                }
                catch (const PgCancelledError &)
                {
                    break;
                }
                catch (const PgQueryError &)
                {
                }
                ++total;
            }
            --alive;
            if (--remaining == 0)
                allDone.set_value();
        });
    }

    co_await allStared.get_future().get();
    uint64_t lastCompleted = 0;
    for (int sec = 1; sec <= g_durationSec; ++sec)
    {
        co_await Scheduler::current()->sleep_for(std::chrono::seconds(1));
        uint64_t now = completed.load();
        printf("[%2ds] alive=%-6lld  total=%-8llu  success=%-8llu  qps=%llu\n",
               sec,
               (long long)alive.load(),
               (unsigned long long)total.load(),
               (unsigned long long)now,
               (unsigned long long)(now - lastCompleted));
        lastCompleted = now;
    }

    co_await allDone.get_future().get();
    printf("total=%llu  success=%llu  failed=%llu\n",
           (unsigned long long)total.load(),
           (unsigned long long)completed.load(),
           (unsigned long long)(total - completed));

    {
        auto conn = co_await pool.acquire();
        co_await conn->execute("DROP TABLE IF EXISTS press_test");
    }

    NITRO_CHECK(completed.load() > 0);
}

int main(int argc, char ** argv)
{
    std::vector<char *> remaining;
    remaining.push_back(argv[0]);
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--duration=", 11) == 0)
            g_durationSec = std::stoi(argv[i] + 11);
        else if (std::strncmp(argv[i], "--pool-size=", 12) == 0)
            g_poolSize = std::stoi(argv[i] + 12);
        else if (std::strncmp(argv[i], "--workers=", 10) == 0)
            g_workers = std::stoi(argv[i] + 10);
        else
            remaining.push_back(argv[i]);
    }
    int newArgc = static_cast<int>(remaining.size());
    NITRO_INFO("pg_press: duration=%ds  pool-size=%d  workers=%d", g_durationSec, g_poolSize, g_workers);
    return nitrocoro::test::run_all(newArgc, remaining.data());
}
