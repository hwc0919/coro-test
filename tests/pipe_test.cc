/**
 * @file pipe_test.cc
 * @brief Tests for Pipe / PipeSender / PipeReceiver
 */
#include <nitrocoro/core/Pipe.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;

/** send before recv: recv returns the value immediately without suspending. */
NITRO_TEST(pipe_send_before_recv)
{
    auto [tx, rx] = makePipe<int>();
    tx->send(42);
    auto v = co_await rx->recv();
    NITRO_REQUIRE(v.has_value());
    NITRO_CHECK_EQ(*v, 42);
}

/** recv suspends until send delivers a value. */
NITRO_TEST(pipe_recv_suspends)
{
    auto [tx, rx] = makePipe<int>();
    Scheduler::current()->spawn([TEST_CTX, tx]() -> Task<> {
        co_await Scheduler::current()->sleep_for(0.02);
        tx->send(7);
    });
    auto v = co_await rx->recv();
    NITRO_REQUIRE(v.has_value());
    NITRO_CHECK_EQ(*v, 7);
}

/** Multiple sends are received in FIFO order. */
NITRO_TEST(pipe_fifo_order)
{
    auto [tx, rx] = makePipe<int>();
    tx->send(1);
    tx->send(2);
    tx->send(3);
    NITRO_CHECK_EQ(*(co_await rx->recv()), 1);
    NITRO_CHECK_EQ(*(co_await rx->recv()), 2);
    NITRO_CHECK_EQ(*(co_await rx->recv()), 3);
}

/** Sender destroyed with empty queue: recv returns nullopt immediately. */
NITRO_TEST(pipe_sender_drop_empty)
{
    auto [tx, rx] = makePipe<int>();
    tx.reset();
    auto v = co_await rx->recv();
    NITRO_CHECK(!v.has_value());
}

/** Sender destroyed after sends: remaining items drained, then nullopt. */
NITRO_TEST(pipe_sender_drop_drains_remaining)
{
    auto [tx, rx] = makePipe<int>();
    tx->send(10);
    tx->send(20);
    tx.reset();
    NITRO_CHECK_EQ(*(co_await rx->recv()), 10);
    NITRO_CHECK_EQ(*(co_await rx->recv()), 20);
    NITRO_CHECK(!(co_await rx->recv()).has_value());
}

/** recv suspends, then Sender is destroyed, waking receiver with nullopt. */
NITRO_TEST(pipe_sender_drop_wakes_waiter)
{
    auto [tx, rx] = makePipe<int>();
    Scheduler::current()->spawn([TEST_CTX, tx = std::move(tx)]() -> Task<> {
        co_await Scheduler::current()->sleep_for(0.02);
        // tx destroyed here
    });
    auto v = co_await rx->recv();
    NITRO_CHECK(!v.has_value());
}

/** send() after sender is the last copy still works; shared ownership transfers correctly. */
NITRO_TEST(pipe_sender_shared_ownership)
{
    auto [tx, rx] = makePipe<int>();
    auto tx2 = tx; // shared_ptr copy
    tx.reset();    // original gone, tx2 still alive
    tx2->send(99);
    tx2.reset(); // now closed
    NITRO_CHECK_EQ(*(co_await rx->recv()), 99);
    NITRO_CHECK(!(co_await rx->recv()).has_value());
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
