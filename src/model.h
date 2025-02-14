#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include "player.h"
#include <random>
#include <ctime>
#include <cmath>
#include <chrono>
#include <boost/signals2.hpp>

#include "loot.h"
#include "loot_generator.h"
#include "tagged.h"
#include "collision_detector.h"
namespace sig = boost::signals2;

namespace collision_detector {
class GameItemGathererProvider: public ItemGathererProvider {
public:
        
    size_t ItemsCount() const override {
        return items_.size();
    };

    Item GetItem(size_t idx) const override {
        return items_[idx];
    };

    void AddItem(Item item) {
        items_.push_back(std::move(item));
    };

    size_t GatherersCount() const override { 
        return gatherers_.size();
    };

    Gatherer GetGatherer(size_t idx) const override { 
        return gatherers_[idx];
    };

    void AddGatherer(Gatherer gatherer) {
        gatherers_.push_back(std::move(gatherer));
    };
private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};
}

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct MoveInfo {
    bool road_end_met;
    double movement_duration;
    app::Coordinates start_coordinates;
    app::Coordinates end_coordinates;
    bool HasMoved() const {return start_coordinates != end_coordinates;}
};

bool isFractionInRange(double num);

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

struct PositionOnRoads {
    std::shared_ptr<Road> vertical = nullptr;
    std::shared_ptr<Road> horizontal = nullptr;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;
    int GetBagCapacity() const;
    double GetMapSpeed() const;
    int GetRandomLootType() const;
    int GetLootValue(int id) const;
    PositionOnRoads GetRoadsByCoordinates(app::Coordinates coordinates) const;
    double GetIdleTimeLimit() const { return idle_time_limit_;}

    void SetLootNumber(int loot_number);  
    void SetLootValues(const std::vector<int>& values);
    void SetDefaultSpeed(double speed);
    void SetDefaultBagCapacity(int capacity);
    void SetIdleTimeLimit(double retirement_time) {
        idle_time_limit_ = retirement_time;
    }


    void AddRoad(const Road& road);
    void AddOffice(Office office);
    void AddBuilding(const Building& building);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    std::vector<std::shared_ptr<Road>> road_ptrs_;

    Buildings buildings_;    
    double speed_ = 0.001;
    int capacity_ = 3;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::map<int, std::vector<std::shared_ptr<Road>>> vertical_roads_;
    std::map<int, std::vector<std::shared_ptr<Road>>> horizontal_roads_;
    int loot_number_;
    std::vector<int> loot_type_id_to_value_;
    double idle_time_limit_;
};

struct Item {
    int id;
    int type;
    int value;
};

struct LostObject {
    Item item;
    app::Coordinates coordinates;
};

class GameSession {
public:
    GameSession(const Map* map, bool spawn_points_randomized, std::shared_ptr<loot_gen::LootGenerator> loot_generator) 
        : map_(map)
        , session_id_(++session_id_counter_)
        , spawn_points_randomized_(spawn_points_randomized)
        , generator_(static_cast<unsigned>(time(0)))
        , loot_generator_(loot_generator)
    {        
    }

    int GetId() const;
    void AddPlayer(std::shared_ptr<app::Player>& player);
    std::vector<std::shared_ptr<app::Player>> GetPlayers() const;
    app::Coordinates GetRandomCoordinates() const;
    app::Coordinates GetDefaultCoordinates() const;
    app::Coordinates GetSpawnCoordinates() const;
    double GetMapSpeed() const;
    void UpdateTime(double time_delta);
    MoveInfo CalculateNewPosition(app::Coordinates start, app::Speed v, double t, std::shared_ptr<Road> road);
    std::map<int, LostObject> GetLostObjects() const;
    void RestoreLostObjects(std::map<int, LostObject> loot);
    std::string GetMapID() const;
    double GetIdleTimeLimit() const;
    void DeletePlayerFromSession(std::string token);

private:  
    std::vector<std::shared_ptr<app::Player>> players_;
    const Map* map_;
    int session_id_;
    bool spawn_points_randomized_;
    static int session_id_counter_;
    static int loot_counter_;
    mutable std::mt19937 generator_; 
    std::shared_ptr<loot_gen::LootGenerator> loot_generator_;

    std::map<std::shared_ptr<app::Player>, PositionOnRoads> player_to_roads_;
    std::map<int, LostObject> loot_;
    void UpdateRoadsDataForPlayer(std::shared_ptr<app::Player> player, bool is_excluded = false);  
    void GenerateLoot(std::chrono::milliseconds time_delta);
    void ExcludePlayers(const std::vector<std::shared_ptr<app::Player>>& players_to_exclude);
};

struct LootGeneratorConfig {
    std::chrono::milliseconds period;
    double probability;
};

class Game {
public:
    using DBSignal = sig::signal<void(std::string, int, int)>;

    using Maps = std::vector<Map>;
    using Sessions = std::map<int, std::shared_ptr<GameSession>>;
    using LostObjects = std::map<int, LostObject>;

    void SetLootGenerator(LootGeneratorConfig config);
    void SetLootObjectsInfo(LootObjectsInfo loot_objects_info);
    void AddMap(Map map);
    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;
    std::shared_ptr<GameSession> AddSession(const Map* map);
    std::shared_ptr<app::Player> JoinGame(const std::string& name, const Map* map);
    RetiredPlayerInfo LeaveGame(std::shared_ptr<app::Player> player);

    std::shared_ptr<app::Player> InitializePlayerForRestore(std::string name, std::string token, int id, const Map* map);
    std::shared_ptr<app::Player> GetPlayerByToken(const std::string& token) const;
    void UpdateTime(double time_delta);
    void SetPlayersStartPointRandomizing(bool randomize_spawn_points);
    LootProperties GetLootInfo(std::string map_id);
    std::map<int, std::shared_ptr<app::Player>> GetPlayers();
    void RestoreLootForAllSessions(std::map<int, LostObjects> session_id_to_loot);
    std::map<int, LostObjects>  RetrieveLootForBackup() const;
    void SetOnLeaveHandler(const DBSignal::slot_type& handler) {
        players_.SetOnLeaveHandler(handler);
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;    

    Sessions id_to_sessions_;
    //map ptr to collection of sessions' id
    std::map<const Map*, std::vector<int>> map_to_sessions_;
    app::Players players_;
    bool spawn_points_randomized_;    
    std::shared_ptr<loot_gen::LootGenerator> loot_generator_;
    LootObjectsInfo loot_objects_info_;
};

}  // namespace model

