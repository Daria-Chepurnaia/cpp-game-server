#pragma once 

#include <boost/signals2.hpp>
#include <chrono>
#include "model.h"
#include "model_serialization.h"
#include "data_structures.h"
#include "db_manager.h"

namespace app {

namespace json = boost::json;
namespace sig = boost::signals2;
using milliseconds = std::chrono::milliseconds;
using namespace std::literals;

const std::map<app::Direction, std::string> dir_to_letter = { {app::Direction::NORTH, "U"s},
                                                        {app::Direction::SOUTH, "D"s},
                                                        {app::Direction::WEST, "L"s},
                                                        {app::Direction::EAST, "R"s} };

class Application {
public:
    using TickSignal = sig::signal<void(double delta)>;

    Application(model::Game& game, std::optional<std::string> path, std::shared_ptr<postgres::DBManager> db) 
        : game_(game)        
        , state_save_file_path_(path)
        , db_(db) {}
    const model::Map* FindMap(model::Map::Id(map_id));
    std::shared_ptr<app::Player> JoinGame(const std::string& name, const model::Map* map);

    std::shared_ptr<app::Player> GetPlayerByToken(const std::string& token) const;    
    boost::json::array GetJSONforAllMaps() const;
    boost::json::object GetJSONforMap(const model::Map::Id& id) const; 
    std::string GetPlayersJSONInfo (std::shared_ptr<app::Player> player_ptr);
    std::string GetStateJSONInfo(std::shared_ptr<app::Player> player_ptr);
    std::string GetRecordsJSONInfo(std::optional<int> start_element = std::nullopt, std::optional<int> maxItems = std::nullopt);

    void Move(std::shared_ptr<app::Player> player_ptr, std::string direction);
    void UpdateTime(double time_delta);
    sig::connection DoOnTimeUpdate(const TickSignal::slot_type& handler);
    void SaveState() const;

private:
    model::Game& game_;
    std::optional<std::string> state_save_file_path_;
    TickSignal tick_signal_;
    std::shared_ptr<postgres::DBManager> db_;
};
} //namespace application