#include "model.h"
#include "player.h"

#include <stdexcept>
#include <cmath>
#include <limits>

namespace model {
using namespace std::literals;

int GameSession::session_id_counter_ = 0;
int GameSession::loot_counter_ = 0;
constexpr double EPSILON = 1e-6;

bool isFractionInRange(double num) {
    double intPart;
    double fracPart = modf(num, &intPart);    

    return (fracPart > 0.4 + EPSILON && fracPart < 0.6 - EPSILON); 
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
    
    if (road.IsHorizontal()) {
        road_ptrs_.push_back(std::make_shared<Road>(Road::HORIZONTAL, road.GetStart(), road.GetEnd().x));
        horizontal_roads_[road.GetStart().y].push_back(road_ptrs_.back());
    } else {
        road_ptrs_.push_back(std::make_shared<Road>(Road::VERTICAL, road.GetStart(), road.GetEnd().y));
        vertical_roads_[road.GetStart().x].push_back(road_ptrs_.back());
    }
} 

void Map::SetLootValues(const std::vector<int>& values) {
    loot_type_id_to_value_ = values;
}

int Map::GetLootValue(int id) const {
    return loot_type_id_to_value_.at(id);
}

void Map::SetLootNumber(int loot_number) {
    loot_number_ = loot_number;
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::SetDefaultSpeed(double speed) {
    speed_ = speed;
}

void Map::SetDefaultBagCapacity(int capacity) {
    capacity_ = capacity;
}

int Map::GetBagCapacity() const {
    return capacity_;
}

double Map::GetMapSpeed() const {
    return speed_;
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}


PositionOnRoads Map::GetRoadsByCoordinates(app::Coordinates coordinates) const {
    PositionOnRoads roads;
    bool update_vertical = true;
    bool update_horizontal = true;

    if (isFractionInRange(coordinates.x)) {
        update_vertical = false;
    }
    if (isFractionInRange(coordinates.y)) { 
        update_horizontal = false;
    }

    int x = std::round(coordinates.x);
    int y = std::round(coordinates.y);
    
    if(update_vertical) {
        if (vertical_roads_.contains(x)) {
            for (auto road : vertical_roads_.at(x)) {
                double start_y = std::min(road->GetStart().y, road->GetEnd().y) - 0.4;
                double end_y = std::max(road->GetStart().y, road->GetEnd().y) + 0.4;
                if(static_cast<double>(y) >= start_y &&
                   static_cast<double>(y) <= end_y) {
                    roads.vertical = road;
                    break;
                }
            }
        }
    }
    if(update_horizontal) {
        if (horizontal_roads_.contains(y)) {
            for (auto road : horizontal_roads_.at(y)) {
                double start_x = std::min(road->GetStart().x, road->GetEnd().x) - 0.4;
                double end_x = std::max(road->GetStart().x, road->GetEnd().x) + 0.4;
                if(static_cast<double>(x) >= start_x &&
                   static_cast<double>(x) <= end_x) {
                    roads.horizontal = road;
                    break;
                }
            }
        }
    }
    return roads;
}

int Map::GetRandomLootType() const {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));    
    return std::rand() % loot_number_;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::map<int, std::shared_ptr<app::Player>> Game::GetPlayers() {
    return players_.GetPlayers();
}

LootProperties Game::GetLootInfo(std::string map_id) {
    return loot_objects_info_.GetLootInfo(map_id);
}   

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

void Game::SetLootGenerator(LootGeneratorConfig config) {
    loot_generator_ = std::make_shared<loot_gen::LootGenerator>(config.period, config.probability);
}

void Game::SetLootObjectsInfo(LootObjectsInfo loot_objects_info) {
    loot_objects_info_ = loot_objects_info;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

std::shared_ptr<GameSession> Game::AddSession(const Map* map) {
    if (map_to_sessions_.contains(map)) {
        //returns the first session of the map
        return id_to_sessions_.at(map_to_sessions_.at(map).at(0));
    }       
    std::shared_ptr<GameSession> session_ptr = std::make_shared<GameSession>(map, spawn_points_randomized_, loot_generator_);    
    id_to_sessions_[session_ptr->GetId()] = session_ptr;   
    map_to_sessions_[map].push_back(session_ptr->GetId()); 
    return session_ptr;    
}

std::shared_ptr<app::Player> Game::JoinGame(const std::string& name, const Map* map) {
    std::shared_ptr<GameSession> session_ptr = AddSession(map);
    auto player = players_.AddPlayer(name, session_ptr);
    return player;
}

std::shared_ptr<app::Player> Game::InitializePlayerForRestore(std::string name,  
                                            std::string token, int id, const Map* map) {
    std::shared_ptr<GameSession> session_ptr = AddSession(map);
    auto player = players_.AddPlayer(name, token, id, session_ptr);
    return player;
}

std::shared_ptr<app::Player> Game::GetPlayerByToken(const std::string& token) const {
    return players_.GetPlayerByToken(token);
} 

void Game::UpdateTime(double time_delta) {
    for (auto [id, session] : id_to_sessions_ ) {
        session->UpdateTime(time_delta);
    }
}

void Game::SetPlayersStartPointRandomizing(bool randomize_spawn_points) {
    spawn_points_randomized_ = randomize_spawn_points;
}

void  Game::RestoreLootForAllSessions(std::map<int, LostObjects> session_id_to_loot) {
    for (const auto& [id, loot] : session_id_to_loot) {
        id_to_sessions_.at(id)->RestoreLostObjects(loot);
    }
}
std::map<int, Game::LostObjects>   Game::RetrieveLootForBackup() const {
    std::map<int, LostObjects> session_id_to_loot;
    for (const auto& [id, session] : id_to_sessions_) {
        session_id_to_loot[id] = session->GetLostObjects();
    }
    return session_id_to_loot;
}

int GameSession::GetId() const {
    return session_id_;
}

void GameSession::AddPlayer(std::shared_ptr<app::Player>& player) {
    players_.push_back(player);
    UpdateRoadsDataForPlayer(player);
}

void GameSession::RestoreLostObjects(std::map<int, LostObject> loot) {
    loot_ = loot;
}

std::string GameSession::GetMapID() const {
    return *(map_->GetId());
}

std::vector<std::shared_ptr<app::Player>> GameSession::GetPlayers() const {
    return  players_;
}

app::Coordinates GameSession::GetRandomCoordinates() const {
    double x, y;
    auto roads = map_->GetRoads();

    std::uniform_int_distribution<int> road_index_distribution(0, roads.size() - 1);
    std::uniform_real_distribution<double> road_coordinates_distribution(0.0, 1.0);        
    int road_index = road_index_distribution(generator_);
    double random_number = road_coordinates_distribution(generator_);

    auto road = roads.at(road_index);        

    if (road.IsVertical()) {
        x = road.GetStart().x;
        y = road.GetStart().y + random_number * (road.GetEnd().y - road.GetStart().y);
    } else {
        y = road.GetStart().y;
        x = road.GetStart().x + random_number * (road.GetEnd().x - road.GetStart().x);
    }
    
    return {x,y};
}

void GameSession::DeletePlayerFromSession(std::string token) {
    auto it = std::find_if(players_.begin(), players_.end(),
    [&token](const std::shared_ptr<app::Player>& player) {
        return player->GetToken() == token; 
    });

    if (it != players_.end()) {
        players_.erase(it); 
    }
}

app::Coordinates GameSession::GetDefaultCoordinates() const {
    auto start_point = map_->GetRoads().at(0).GetStart();
    return {static_cast<double>(start_point.x),static_cast<double>(start_point.y)};
}

app::Coordinates GameSession::GetSpawnCoordinates() const {
    if (spawn_points_randomized_) {
        return GetRandomCoordinates();
    } else {
        return GetDefaultCoordinates();
    }
}

std::map<int, LostObject> GameSession::GetLostObjects() const {
    return loot_;
}

double GameSession::GetMapSpeed() const {
    return map_->GetMapSpeed();
}

void GameSession::UpdateTime(double time_delta) {
    collision_detector::GameItemGathererProvider provider;
    std::map<int, int> items_index_to_loot;
    std::map<int, std::shared_ptr<app::Player>> gatherer_index_to_player;
    std::vector<std::shared_ptr<app::Player>> players_to_exclude;

    int items_counter = 0;
    int gatherers_counter = 0;

    //update players position 
    for (auto [player, roads] : player_to_roads_) {                   
        MoveInfo move_info;
        auto dir = player->GetDirection();
        if (dir == app::Direction::WEST || dir == app::Direction::EAST) {
            if(roads.horizontal) {
                move_info = CalculateNewPosition(player->GetCoordinates(), player->GetSpeed(), time_delta, roads.horizontal);
            } else {
                move_info = CalculateNewPosition(player->GetCoordinates(), player->GetSpeed(), time_delta, roads.vertical);
            }
        } else {
            if (roads.vertical) {
                move_info = CalculateNewPosition(player->GetCoordinates(), player->GetSpeed(), time_delta, roads.vertical);
            } else {
                move_info = CalculateNewPosition(player->GetCoordinates(), player->GetSpeed(), time_delta, roads.horizontal);
            }
        }
        // This method returns false if the player has been idle for too long.
        bool is_player_in_game = player->UpdatePosition(time_delta, move_info);
         
        //add player to provider
        provider.AddGatherer({geom::Point2D(move_info.start_coordinates.x, move_info.start_coordinates.y), 
                              geom::Point2D(move_info.end_coordinates.x, move_info.end_coordinates.y), 0.3});
        gatherer_index_to_player[gatherers_counter++] = player;

        UpdateRoadsDataForPlayer(player);
        if (!is_player_in_game) {
             players_to_exclude.push_back(player);
        } 
    }

    //add items to provider 
    for (const auto& [id, loot] : loot_) {
        collision_detector::Item item {geom::Point2D(loot.coordinates.x, loot.coordinates.y), 0.};
        provider.AddItem(item);
        items_index_to_loot[items_counter++] = id;
    }
    //add offices to provider 
    for (const auto& office : map_->GetOffices()) {
        collision_detector::Item item {geom::Point2D(office.GetPosition().x,office.GetPosition().y ), 0.25};
        provider.AddItem(item);
        items_counter++;
    }
    //get gather events and process each event
    auto events = collision_detector::FindGatherEvents(provider);
    for (const auto& event : events) {
        auto player = gatherer_index_to_player.at(event.gatherer_id);
        //collision with office
        if (!items_index_to_loot.contains(event.item_id)) {
            player->ClearBag();
            continue;
        }
        //collision with item
        //if item hasn't been collected by other players yet and player's bag is not full
        if (loot_.contains(items_index_to_loot.at(event.item_id)) &&
            player->GetBag().size() < map_->GetBagCapacity()) {            
            player->CollectItem(loot_.at(items_index_to_loot.at(event.item_id)).item);
            //delete item from map
            loot_.erase(items_index_to_loot.at(event.item_id));    
        }           
    }
    //exclude players
    ExcludePlayers(players_to_exclude);

    // moved loot generation after the logic of excluding players 
    GenerateLoot(std::chrono::milliseconds(
        static_cast<uint64_t>(std::lround(time_delta))
    ));
}

void GameSession::ExcludePlayers(const std::vector<std::shared_ptr<app::Player>>& players_to_exclude) {
    for (const auto& player : players_to_exclude) {
        DeletePlayerFromSession(player->GetToken());
        UpdateRoadsDataForPlayer(player, true);
        player->LeaveGame();
    }
}

void GameSession::GenerateLoot(std::chrono::milliseconds time_delta) {
    int amount_loot_to_add = loot_generator_->Generate(time_delta, loot_.size(), players_.size());
    for (int i = 0; i < amount_loot_to_add; --amount_loot_to_add) {
        LostObject lost_object;
        lost_object.item.id = loot_counter_++;
        lost_object.item.type = map_->GetRandomLootType();
        lost_object.item.value = map_->GetLootValue(lost_object.item.type);
        lost_object.coordinates = GetRandomCoordinates();
        loot_[lost_object.item.id] = lost_object;         
    } 
}

MoveInfo GameSession::CalculateNewPosition(app::Coordinates start, app::Speed v, double t, std::shared_ptr<Road> road) {
    MoveInfo move;
    move.start_coordinates = start;
    auto minX = std::min(road->GetStart().x, road->GetEnd().x) - 0.4;
    auto maxX = std::max(road->GetStart().x, road->GetEnd().x) + 0.4;
    auto minY = std::min(road->GetStart().y, road->GetEnd().y) - 0.4;
    auto maxY = std::max(road->GetStart().y, road->GetEnd().y) + 0.4;

    app::Coordinates actual_position;

    actual_position.x = std::clamp(start.x + v.x * t, minX, maxX);
    actual_position.y = std::clamp(start.y + v.y * t, minY, maxY);

    app::Coordinates destination_position = {start.x + v.x * t, start.y + v.y * t};

    move.road_end_met = (std::abs(actual_position.x - destination_position.x) > EPSILON) || 
                        (std::abs(actual_position.y - destination_position.y) > EPSILON);

    if (move.road_end_met) {
        double distance_x = actual_position.x - start.x;
        double distance_y = actual_position.y - start.y;
        double distance_traveled = std::sqrt(distance_x * distance_x + distance_y * distance_y);

        double speed = std::sqrt(v.x * v.x + v.y * v.y);
        move.movement_duration = speed > 0 ? distance_traveled / speed : 0.0;
    } else if (v.x == 0. && v.y == 0) {
        move.movement_duration = 0.;
    } else {
        move.movement_duration = t;
    }
    move.end_coordinates = actual_position;

    return move;
} 

void GameSession::UpdateRoadsDataForPlayer(std::shared_ptr<app::Player> player, bool is_excluded) {
    if (is_excluded) {
        player_to_roads_.erase(player);
    } else {
        PositionOnRoads roads = map_->GetRoadsByCoordinates(player->GetCoordinates());
        player_to_roads_[player] = {roads};
    }
}

double GameSession::GetIdleTimeLimit() const {
    return map_->GetIdleTimeLimit();
}

}  // namespace model
