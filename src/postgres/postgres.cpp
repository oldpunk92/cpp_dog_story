#include "postgres.h"

#include <pqxx/zview.hxx>
#include <boost/log/trivial.hpp>

namespace postgres {

    using namespace std::literals;
    using pqxx::operator""_zv;

    void Database::CreateTables() {
        auto conn_wrapper = pool_.GetConnection();
        pqxx::work w(*conn_wrapper);
        w.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                score INT NOT NULL,
                play_time_sec DOUBLE PRECISION NOT NULL
            );
        )");
        w.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_leaderboard
            ON retired_players (score DESC, play_time_sec ASC, name ASC);
        )");
        w.commit();
    }

    void Database::SaveRecord(const model::PlayerRecord& record) {
        try {
            auto conn_wrapper = pool_.GetConnection();
            pqxx::work w(*conn_wrapper);
            w.exec_params(
                "INSERT INTO retired_players (name, score, play_time_sec) VALUES ($1, $2, $3);",
                record.name, record.score, record.play_time
                );
            w.commit();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "SaveRecord failed: " << e.what();
        }
    }

    std::vector<model::PlayerRecord> Database::GetLeaderboard(int start, int max_items) {
        auto conn_wrapper = pool_.GetConnection();
        pqxx::work w(*conn_wrapper);
        pqxx::result res = w.exec_params(
            "SELECT name, score, play_time_sec FROM retired_players "
            "ORDER BY score DESC, play_time_sec ASC, name ASC "
            "LIMIT $1 OFFSET $2;",
            max_items, start
            );

        std::vector<model::PlayerRecord> records;
        for (const auto& row : res) {
            records.push_back({
                row["name"].as<std::string>(),
                row["score"].as<int>(),
                row["play_time_sec"].as<double>()
            });
        }
        return records;
    }

}  // namespace postgres
