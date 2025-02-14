#include "player.h"
#include "model.h"
#include <map>

int app::Players::player_id_counter_ = 0;
app::TokenGenerator app::Players::token_generator_;

namespace app{

    std::string TokenGenerator::GetToken() {
        uint64_t a = generator1_();
        uint64_t b = generator2_();
        std::stringstream stream;      
        stream << std::hex << a << b;
        std::string token;
        getline(stream, token); 
        for (uint i = token.size() - 1; i < 31; i++) {
            token.append("1");
        }
        return token;
    }

    std::map<std::string, Direction> symbol_to_direction = {{"U", Direction::NORTH},     
                                                            {"R", Direction::EAST},
                                                            {"L", Direction::WEST},
                                                            {"D", Direction::SOUTH},
                                                            {"", Direction::NORTH}};

    std::shared_ptr<Player> Players::AddPlayer(std::string name, std::shared_ptr<model::GameSession> session) {
        Token token = token_generator_.GetToken();               
        return Players::AddPlayer(name, token, ++player_id_counter_, session);
    }

    std::shared_ptr<Player> Players::AddPlayer(std::string name, std::string token, int id, std::shared_ptr<model::GameSession> session) {        
        std::shared_ptr<Player> player = std::make_shared<Player>(name, id, token);
        player_id_counter_ = id;
        player->SetSession(session);
        session->AddPlayer(player);
        token_to_player_.insert({token, player}); 
        player_id_to_player_[player->GetId()] = player;  
        player->DoOnLeave([this, token] { 
            this->DeletePlayer(token);
        });     
        if (on_leave_sig_handler_) {
            player->BindDBHandler(*on_leave_sig_handler_);
        }
        return player;
    }

    void Player::SetSession(std::shared_ptr<model::GameSession> game_session) {
        game_session_ =  game_session;    
        if (auto session = game_session_.lock()) {
            coordinates_ = session->GetSpawnCoordinates();
        }
    }

    void Player::RestorePlayerState(int score, double idle_time, double total_time, Coordinates coordinates, Speed speed, Direction direction, std::vector<model::Item> bag)  {
        score_ = score;
        idle_time_ = idle_time;
        total_play_time_ = total_time;
        coordinates_ = coordinates;
        speed_ = speed;
        direction_ = direction;
        bag_ = bag;
    }


    std::string Player::GetMapID() {
        if (auto session = game_session_.lock()) {
            return session->GetMapID();
        } 
        return "";
    }

    void Player::Move(std::string direction) {
        direction_ = symbol_to_direction.at(direction);
        if (auto session = game_session_.lock()) {
            double speed = session->GetMapSpeed();

            if (direction == "") {
                speed_ = {0, 0};
            } else if (direction == "U") {
                speed_ = {0, -1. * speed};
                idle_time_ = 0.;
            } else if (direction == "D") {
                speed_ = {0, speed};
                idle_time_ = 0.;
            } else if (direction == "L") {
                speed_ = {-1. * speed, 0};
                idle_time_ = 0.;
            } else if (direction == "R") {
                speed_ = {speed, 0};
                idle_time_ = 0.;
            }
        }
    }

    void Player::LeaveGame() {
        // send signal to DB about leaving game
        db_signal_(name_, total_play_time_, score_);
        leave_signal_();
    }

    bool Player::UpdatePosition(double time_delta, const model::MoveInfo& move_info) {

        double idle_time_limit;
        if (auto session = game_session_.lock()) {
            idle_time_limit = session->GetIdleTimeLimit();
        }
        double time_until_exclusion = idle_time_limit - idle_time_;

        //update idle_time_  
        idle_time_ += time_delta - move_info.movement_duration;

        // update speed if player stopped
        if (move_info.road_end_met) {
            StopPlayer();
        } 
        SetPosition(move_info.end_coordinates);

        //check if idle_time exceeded idle_time_limit
        if (idle_time_ >= idle_time_limit) {
            total_play_time_ += time_until_exclusion;
            return false;
        }

        total_play_time_ += time_delta;
        return true;
    }

    Token Player::GetToken() const {
        return token_;
    }

    int Player::GetId() const {
        return id_;
    }

    std::shared_ptr<model::GameSession> Player::GetSession() const {
        return  game_session_ .lock();
    }

    std::string Player::GetName() const {
        return name_;
    }

    Coordinates Player::GetCoordinates() const {
        return coordinates_;
    }

    void Player::SetPosition(app::Coordinates new_position) {
        coordinates_ = new_position;
    }

    Speed Player::GetSpeed() const {
        return speed_;
    }

    void Player::StopPlayer() {
        speed_ = {};
    }

    void Player::ClearBag() {
        score_ += std::accumulate(bag_.begin(), bag_.end(), 0, 
            [](int sum, const model::Item& item) {
                return sum + item.value;
            });
        bag_.clear();
    }

    Direction Player::GetDirection() const {
        return direction_;
    }

    std::vector<model::Item> Player::GetBag() const {
        return bag_;
    }

    int Player::GetScore() const {
        return score_;
    }

    void Player::CollectItem(model::Item item) {
        bag_.push_back(item);
    }

    sig::connection Player::DoOnLeave(const LeaveSignal::slot_type& handler) {
        return leave_signal_.connect(handler);
    }

    sig::connection Player::BindDBHandler(const DBSignal::slot_type& handler) {
        return db_signal_.connect(handler);
    }

    std::shared_ptr<Player> Players::GetPlayerByToken(const Token& token) const {
        if(token_to_player_.contains(token)) {
            return token_to_player_.at(token);
        }
        return nullptr;
    } 

}