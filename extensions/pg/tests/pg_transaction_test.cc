/**
 * @file pg_transaction_test.cc
 * @brief Tests for PgTransaction
 *
 * Requires a running PostgreSQL instance. Set the connection string via
 * environment variable PG_CONN_STR, e.g.:
 *   PG_CONN_STR="host=localhost dbname=test user=postgres password=secret" ./pg_transaction_test
 */
#include <nitrocoro/pg/PgConnection.h>
#include <nitrocoro/pg/PgException.h>
#include <nitrocoro/pg/PgPool.h>
#include <nitrocoro/pg/PgTransaction.h>
#include <nitrocoro/testing/Test.h>

#include <cstdlib>

using namespace nitrocoro;
using namespace nitrocoro::pg;

static std::string connStr()
{
    const char * env = std::getenv("PG_CONN_STR");
    return env ? env : "host=localhost dbname=test user=postgres";
}

Task<std::unique_ptr<PgConnection>> makeConn()
{
    co_return co_await PgConnection::connect(connStr());
}

NITRO_TEST(transaction_raii_rollback)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_raii_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx->execute("INSERT INTO tx_raii_test VALUES (1)");
    }
    co_await Scheduler::current()->sleep_for(1.0);
    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_raii_test");
    NITRO_CHECK(std::get<int64_t>(result.get(0, 0)) == 0);
}

NITRO_TEST(transaction_commit)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_commit_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx->execute("INSERT INTO tx_commit_test VALUES (42)");
        co_await tx->commit();
        NITRO_CHECK_THROWS_AS(co_await tx->execute("INSERT INTO tx_commit_test VALUES (99)"), PgTransactionError);
    }
    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT v FROM tx_commit_test");
    NITRO_CHECK(std::get<int64_t>(result.get(0, 0)) == 42);
}

NITRO_TEST(transaction_rollback)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_rollback_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx->execute("INSERT INTO tx_rollback_test VALUES (99)");
        co_await tx->rollback();
    }
    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_rollback_test");
    NITRO_CHECK(std::get<int64_t>(result.get(0, 0)) == 0);
}

NITRO_TEST(transaction_query)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_query_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx->execute("INSERT INTO tx_query_test VALUES (1), (2), (3)");
        auto result = co_await tx->query("SELECT SUM(v) FROM tx_query_test");
        NITRO_CHECK_EQ(result.rowCount(), 1);
        NITRO_CHECK_EQ(result.colCount(), 1);
        NITRO_CHECK_EQ(std::get<int64_t>(result.get(0, 0)), 6);
        co_await tx->commit();
    }
}

NITRO_TEST(transaction_pool_return)
{
    PgPool pool(1, makeConn);
    {
        auto tx = co_await pool.newTransaction();
        NITRO_CHECK_EQ(pool.idleCount(), 0);
        co_await tx->rollback();
    }
    co_await Scheduler::current()->sleep_for(0.5);
    NITRO_CHECK_EQ(pool.idleCount(), 1);
}

NITRO_TEST(transaction_from_connection)
{
    auto conn = co_await PgConnection::connect(connStr());
    co_await conn->execute("DROP TABLE IF EXISTS tx_conn_test");
    co_await conn->execute("CREATE TABLE tx_conn_test (v INT)");

    {
        auto tx = co_await PgTransaction::begin(std::move(conn));
        co_await tx->execute("INSERT INTO tx_conn_test VALUES (100)");
        co_await tx->commit();
    }

    auto conn2 = co_await PgConnection::connect(connStr());
    auto result = co_await conn2->query("SELECT v FROM tx_conn_test");
    NITRO_CHECK_EQ(result.rowCount(), 1);
    NITRO_CHECK_EQ(std::get<int64_t>(result.get(0, 0)), 100);

    co_await conn2->execute("DROP TABLE tx_conn_test");
}

NITRO_TEST(transaction_release_and_reuse)
{
    auto conn = co_await PgConnection::connect(connStr());
    co_await conn->execute("DROP TABLE IF EXISTS tx_release_test");
    co_await conn->execute("CREATE TABLE tx_release_test (v INT)");

    {
        auto tx = co_await PgTransaction::begin(std::move(conn));
        co_await tx->execute("INSERT INTO tx_release_test VALUES (1)");
        co_await tx->commit();
        conn = tx->release();
    }

    {
        auto tx = co_await PgTransaction::begin(std::move(conn));
        co_await tx->execute("INSERT INTO tx_release_test VALUES (2)");
        co_await tx->commit();
        conn = tx->release();
    }

    auto result = co_await conn->query("SELECT SUM(v) FROM tx_release_test");
    NITRO_CHECK_EQ(std::get<int64_t>(result.get(0, 0)), 3);

    co_await conn->execute("DROP TABLE tx_release_test");
}

NITRO_TEST(transaction_release_before_commit)
{
    auto conn = co_await PgConnection::connect(connStr());
    auto tx = co_await PgTransaction::begin(std::move(conn));
    NITRO_CHECK_THROWS_AS(tx->release(), PgTransactionError);
    co_await tx->rollback();
}

int main()
{
    return nitrocoro::test::run_all();
}
