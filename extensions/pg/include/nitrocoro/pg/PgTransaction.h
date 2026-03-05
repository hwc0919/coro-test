/**
 * @file PgTransaction.h
 * @brief RAII PostgreSQL transaction with automatic rollback on destruction
 */
#pragma once

#include "nitrocoro/pg/PgConnection.h"
#include <nitrocoro/core/Task.h>

#include <memory>
#include <string_view>
#include <vector>

namespace nitrocoro::pg
{

class PgTransaction : public PgConnection
{
public:
    static Task<std::unique_ptr<PgTransaction>> begin(std::unique_ptr<PgConnection> conn);

    ~PgTransaction() override;

    PgTransaction(const PgTransaction &) = delete;
    PgTransaction & operator=(const PgTransaction &) = delete;
    PgTransaction(PgTransaction &&) = delete;
    PgTransaction & operator=(PgTransaction &&) = delete;

    Scheduler * scheduler() const override;
    bool isAlive() const override;

    Task<PgResult> query(std::string_view sql, std::vector<PgValue> params = {}) override;
    Task<> execute(std::string_view sql, std::vector<PgValue> params = {}) override;

    Task<> commit();
    Task<> rollback();

    /**
     * @brief Controls the destructor behavior when the transaction is not explicitly finished.
     *
     * @warning **Exception-unsafe**: if autoCommit is true, the destructor will commit even
     * if an exception was thrown — you may commit partial or unintended changes.
     * Prefer explicit commit() unless you are certain no exception can occur.
     *
     * Default: false (destructor rolls back).
     */
    void setAutoCommit(bool autoCommit) { autoCommit_ = autoCommit; }

    /**
     * @brief Returns the underlying connection after the transaction has been committed or rolled-back.
     *
     * The returned connection can be reused for further queries outside a transaction.
     * Throws PgTransactionError if called before commit() or rollback().
     *
     * @note If the transaction was created from a pooled connection, the released connection
     * is still automatically recycled to the pool when destroyed.
     */
    std::unique_ptr<PgConnection> release();

private:
    explicit PgTransaction(std::unique_ptr<PgConnection> conn);

    std::unique_ptr<PgConnection> conn_;
    Scheduler * scheduler_;
    bool done_{ false };
    bool autoCommit_{ false };
};

} // namespace nitrocoro::pg
