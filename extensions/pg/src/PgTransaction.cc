/**
 * @file PgTransaction.cc
 * @brief PgTransaction implementation
 */
#include "nitrocoro/pg/PgTransaction.h"
#include "nitrocoro/pg/PgException.h"
#include <nitrocoro/utils/Debug.h>

namespace nitrocoro::pg
{

Task<std::unique_ptr<PgTransaction>> PgTransaction::begin(std::unique_ptr<PgConnection> conn)
{
    co_await conn->execute("BEGIN");
    co_return std::unique_ptr<PgTransaction>(new PgTransaction(std::move(conn)));
}

PgTransaction::PgTransaction(std::unique_ptr<PgConnection> conn)
    : conn_(std::move(conn))
    , scheduler_(conn_->scheduler())
{
}

PgTransaction::~PgTransaction()
{
    if (!done_)
    {
        auto conn = std::move(conn_);
        const bool doCommit = autoCommit_;
        scheduler_->spawn([conn = std::move(conn), doCommit]() -> Task<> {
            try
            {
                co_await conn->execute(doCommit ? "COMMIT" : "ROLLBACK");
                NITRO_TRACE("PgTransaction: auto %s successful\n", doCommit ? "commit" : "rollback");
            }
            catch (const std::exception & e)
            {
                NITRO_ERROR("PgTransaction: auto %s failed: %s\n", doCommit ? "commit" : "rollback", e.what());
            }
        });
    }
}

Scheduler * PgTransaction::scheduler() const
{
    return scheduler_;
}

bool PgTransaction::isAlive() const
{
    return !done_ && conn_ && conn_->isAlive();
}

Task<PgResult> PgTransaction::query(std::string_view sql, std::vector<PgValue> params)
{
    if (done_)
        throw PgTransactionError("Transaction already finished");
    co_return co_await conn_->query(sql, std::move(params));
}

Task<> PgTransaction::execute(std::string_view sql, std::vector<PgValue> params)
{
    if (done_)
        throw PgTransactionError("Transaction already finished");
    co_await conn_->execute(sql, std::move(params));
}

Task<> PgTransaction::commit()
{
    if (done_)
        throw PgTransactionError("Transaction already finished");
    co_await conn_->execute("COMMIT");
    done_ = true;
}

Task<> PgTransaction::rollback()
{
    if (done_)
        throw PgTransactionError("Transaction already finished");
    co_await conn_->execute("ROLLBACK");
    done_ = true;
}

std::unique_ptr<PgConnection> PgTransaction::release()
{
    if (!done_)
        throw PgTransactionError("Cannot release connection before commit/rollback");
    return std::move(conn_);
}

} // namespace nitrocoro::pg
