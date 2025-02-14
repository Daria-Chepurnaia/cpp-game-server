#include "db_manager.h"
#include <pqxx/zview.hxx>

AppConfig GetConfigFromEnv() {
    using namespace std::literals;
    AppConfig config;
    if (const auto* url = std::getenv(DB_URL_ENV_NAME)) {
        config.db_url = url;
    } else {
        throw std::runtime_error(DB_URL_ENV_NAME + " environment variable not found"s);
    }
    return config;
}

namespace postgres {
using pqxx::operator"" _zv;

DBManager::DBManager(ConnectionPool& pool)
: pool_(pool) {
    auto connection = pool_.GetConnection();
    pqxx::work work{*connection};
    work.exec(R"(
    CREATE TABLE IF NOT EXISTS retired_players (
        id SERIAL PRIMARY KEY,
        name varchar(100) NOT NULL,
        total_time double precision NOT NULL,
        score int NOT NULL
    );
    )"_zv);
    work.exec(R"(
    CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name
    ON retired_players (score DESC, total_time ASC, name ASC);
    )"_zv);
    work.commit();
}
void DBManager::SavePlayer(const std::string& name, double total_time, int score) {
    auto connection = pool_.GetConnection();
    pqxx::work work{*connection};
    total_time /= 1000.;
    work.exec_params(R"(
            INSERT 
            INTO retired_players (name, total_time, score) 
            VALUES ($1, $2, $3);
            )"_zv,
            name, total_time, score);
    work.commit();
}

std::vector<RetiredPlayerInfo> DBManager::GetPlayers(std::optional<int> start_element, std::optional<int> maxItems) {
    auto connection = pool_.GetConnection();
    pqxx::work work{*connection};
    std::vector<RetiredPlayerInfo> players;
    int offset = 0;
    int limit = 100;
    if (start_element && *start_element >= 0) {
        offset = *start_element;
    }
    if (maxItems && *maxItems >= 0) {
        limit = *maxItems;
    }
    auto result = work.exec_params(R"(
                                    SELECT name, total_time, score
                                    FROM retired_players                                                     
                                    ORDER BY score DESC, total_time ASC, name ASC
                                    OFFSET $1 LIMIT $2
                                    )"_zv, offset, limit);
    for (const auto& player : result) {
        RetiredPlayerInfo player_info;
        player_info.name = player[0].as<std::string>();
        player_info.total_time_in_game = player[1].as<double>();
        player_info.score = player[2].as<int>();
        players.push_back(player_info);                                            
    }
    return players;
}
}