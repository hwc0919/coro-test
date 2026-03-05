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

    std::unique_ptr<PgConnection> release();

private:
    explicit PgTransaction(std::unique_ptr<PgConnection> conn);

    std::unique_ptr<PgConnection> conn_;
    Scheduler * scheduler_;
    bool done_{ false };
};

} // namespace nitrocoro::pg
