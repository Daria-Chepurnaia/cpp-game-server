#pragma once 

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

#include "model.h"

namespace serialization {

void RestoreGameState(std::string path, model::Game& game);
void SaveGameStateInFile(std::string path,  model::Game& game);

}// namespace serialization


namespace model {

template <typename Archive>
void serialize(Archive& ar, Item& item, [[maybe_unused]] const unsigned version) {
    ar&(item.id);
    ar&(item.type);
    ar&(item.value);
}

template <typename Archive>
void serialize(Archive& ar, LostObject& lost_object, [[maybe_unused]] const unsigned version) {
    ar&(lost_object.item);
    ar&(lost_object.coordinates);
}

//player representation for serialization
struct PlayerRepr {
    std::string name;
    std::string token;
    std::string map_id;
    int score;
    double idle_time;
    double total_time;
    app::Coordinates coordinates;
    app::Speed speed;
    app::Direction direction;
    std::vector<model::Item> bag;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & name;
        ar & token;
        ar & map_id;
        ar & score;
        ar & idle_time;
        ar & total_time;
        ar & coordinates;
        ar & speed;
        ar & direction;
        ar & bag;
    }
};
}// namespace model

namespace app {

template <typename Archive>
void serialize(Archive& ar, Coordinates& coordinates, [[maybe_unused]] const unsigned version) {
    ar & coordinates.x;
    ar & coordinates.y;
}

template <typename Archive>
void serialize(Archive& ar, Speed& speed, [[maybe_unused]] const unsigned version) {
    ar & speed.x;
    ar & speed.y;
}

template <typename Archive>
void serialize(Archive& ar, Direction& direction, [[maybe_unused]] const unsigned version) {
    // Serialize the enum as its underlying integer value
    int value = static_cast<int>(direction); // Convert the enum to an int
    ar & value;                              // Serialize the int
    if constexpr (Archive::is_loading::value) {
        // On deserialization, convert back to the enum
        direction = static_cast<Direction>(value);
    }
}
} //namespace app