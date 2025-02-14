#pragma once

#include <random>
#include <string>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <map>
#include <optional>
#include <compare>
#include <boost/asio/signal_set.hpp>
#include <boost/signals2.hpp>
#include "data_structures.h"


namespace model {
    class GameSession;
    struct Item;
    struct MoveInfo;
};

namespace app {

namespace sig = boost::signals2;
using Token = std::string;
using DBSignal = sig::signal<void(std::string, int, int)>;

struct Coordinates {
    double x = 0.;
    double y = 0.;

    auto operator<=>(const Coordinates&) const = default;
};

struct Speed {
    double x = 0.;
    double y = 0.;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST,
};

class Player {
public:
    using LeaveSignal = sig::signal<void()>;

    Player() = delete;
    Player(std::string name, int id, Token token) 
        : name_(name)
        , id_(id)
        , token_(token){}

    Token GetToken() const;
    int GetId() const;
    std::shared_ptr<model::GameSession> GetSession() const;
    std::string GetName() const;
    Coordinates GetCoordinates() const;    
    Speed GetSpeed() const;
    Direction GetDirection() const;
    double GetIdleTime() const {
        return idle_time_;
    }
    double GetTotalTime() const {
        return total_play_time_;
    }
    std::vector<model::Item> GetBag() const;
    int GetScore() const;

    void SetSession(std::shared_ptr<model::GameSession> game_session);
    void SetPosition(app::Coordinates new_position);
    bool UpdatePosition(double time_delta, const model::MoveInfo& move_info);
    void LeaveGame();
    sig::connection DoOnLeave(const LeaveSignal::slot_type& handler);
    sig::connection BindDBHandler(const DBSignal::slot_type& handler);

    void ClearBag();
    void CollectItem(model::Item item);
    void Move(std::string direction);
    void StopPlayer();
    std::string GetMapID();
    void RestorePlayerState(int score, double idle_time, double total_time, Coordinates coordinates, Speed speed, Direction direction, std::vector<model::Item> bag);

private:
    std::weak_ptr<model::GameSession> game_session_;
    std::string name_;
    int id_;
    Token token_;
    int score_ = 0;
    double idle_time_ = 0;          
    double total_play_time_ = 0;

    Coordinates coordinates_;
    Speed speed_;
    Direction direction_ = Direction::NORTH;  
    std::vector<model::Item> bag_;
    LeaveSignal leave_signal_;
    DBSignal db_signal_;
};

class TokenGenerator {
public: 
    std::string GetToken();

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};   
};

class Players {
public:

    Players(){}
    std::shared_ptr<Player> AddPlayer(std::string name, std::shared_ptr<model::GameSession> session);
    std::shared_ptr<Player> AddPlayer(std::string name, std::string token, int id, std::shared_ptr<model::GameSession> session);
    std::shared_ptr<Player> GetPlayerByToken(const Token& token) const;
    std::map<int, std::shared_ptr<Player>> GetPlayers () {
        return player_id_to_player_;
    }
    void SetOnLeaveHandler(const DBSignal::slot_type& handler) {
        on_leave_sig_handler_ = handler;
    }
    void DeletePlayer(std::string token) {
        int player_id = token_to_player_.at(token)->GetId();
        token_to_player_.erase(token);
        player_id_to_player_.erase(player_id);
    }
    
private:
    static int player_id_counter_;
    static TokenGenerator token_generator_;
    std::unordered_map<Token, std::shared_ptr<Player>> token_to_player_;
    std::map<int, std::shared_ptr<Player>> player_id_to_player_;
    std::optional<DBSignal::slot_type> on_leave_sig_handler_;
};

} // namespace app