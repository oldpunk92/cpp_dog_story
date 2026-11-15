#pragma once

#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/result>

#include "connection_pool.h"
#include "../src/game_model/model.h"

namespace postgres {

class Database : public model::IRecordRepository {
public:
    explicit Database(ConnectionPool& pool) : pool_(pool) {
        CreateTables();
    }

    void CreateTables();

    void SaveRecord(const model::PlayerRecord& record) override;
    std::vector<model::PlayerRecord> GetLeaderboard(int start, int max_items) override;

private:
    ConnectionPool& pool_;
};

}  // namespace postgres
