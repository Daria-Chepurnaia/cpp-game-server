#include "json_loader.h"
#include "loot.h"

#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

void ParseRoads(const boost::json::value& map, model::Map& map_model) {
    if (map.as_object().contains("roads")) {
        if (map.at("roads").as_array().empty()) {
            //incorrect json
            throw InvalidConfigureFile("there are no roads in configure file");
        }
        for (auto road : map.at("roads").as_array()) {
            if (!road.as_object().contains(x0) ||
                !road.as_object().contains(y0) ||
                (!road.as_object().contains(x1) && !road.as_object().contains(y1))) {
                //incorrect road format in json file
                throw InvalidConfigureFile("incorrect road information format");
            }
            if (road.as_object().contains(x1)) {
                model::Road road_to_add(model::Road::HORIZONTAL,
                                        model::Point{static_cast<int>(road.at(x0).as_int64()),
                                                     static_cast<int>(road.at(y0).as_int64())},
                                        static_cast<int>(road.at(x1).as_int64()));
                map_model.AddRoad(road_to_add);

            } else if (road.as_object().contains(y1)) {
                model::Road road_to_add(model::Road::VERTICAL,
                                        model::Point{static_cast<int>(road.at(x0).as_int64()),
                                                     static_cast<int>(road.at(y0).as_int64())},
                                        static_cast<int>(road.at(y1).as_int64()));
                map_model.AddRoad(road_to_add);
            }
        }
    }    
}

void ParseBuildings(const boost::json::value& map, model::Map& map_model) {
    if (map.as_object().contains("buildings")) {
        for (auto building : map.at("buildings").as_array()) {
            if (building.as_object().contains(x) &&
                building.as_object().contains(y) &&
                building.as_object().contains(width) &&
                building.as_object().contains(height)) {
                model::Building building_to_add(model::Rectangle{model::Point{static_cast<int>(building.at(x).as_int64()),
                                                                              static_cast<int>(building.at(y).as_int64())},
                                                                 model::Size{static_cast<int>(building.at(width).as_int64()),
                                                                             static_cast<int>(building.at(height).as_int64())}});

                map_model.AddBuilding(building_to_add);

            } else {
                //logic of incorrect json
                throw InvalidConfigureFile("incorrect building information format");
            }
        }
    }
}

void ParseOffices(const boost::json::value& map, model::Map& map_model) {
    if (map.as_object().contains("offices")) {
        for (auto office : map.at("offices").as_array()) {
            if (office.as_object().contains(id_key) &&
                office.as_object().contains(x) &&
                office.as_object().contains(y) &&
                office.as_object().contains(offsetX) &&
                office.as_object().contains(offsetY)) {
                model::Office office_to_add(model::Office{model::Office::Id(office.at(id_key).as_string().data()),
                                                          model::Point{static_cast<int>(office.at(x).as_int64()),
                                                                       static_cast<int>(office.at(y).as_int64())},
                                                          model::Offset{static_cast<int>(office.at(offsetX).as_int64()),
                                                                        static_cast<int>(office.at(offsetY).as_int64())}});

                map_model.AddOffice(office_to_add);

            } else {
                //logic of incorrect json
                throw InvalidConfigureFile("incorrect office information format");
            }
        }
    }
}

void ParseLootInfo(const boost::json::value& map, LootObjectsInfo& loot_info) { 
    auto id = map.at(id_key).as_string().data();
    if (map.as_object().contains("lootTypes")) {                     
        loot_info.AddLootforMap(id, LootProperties(map.as_object().at("lootTypes")));
    } else {
        throw InvalidConfigureFile("incorrect loot information format");
    }    
}

void ParseSettings(const boost::json::value& map, model::Map& map_model, const DefaultSettings& settings) {    
        if (map.as_object().contains("dogSpeed")) {            
            map_model.SetDefaultSpeed(map.as_object().at("dogSpeed").as_double()/1000.);           
        } else {
            map_model.SetDefaultSpeed(settings.global_default_speed);
        }
        if (map.as_object().contains("bagCapacity")) {            
            map_model.SetDefaultBagCapacity(map.as_object().at("bagCapacity").as_int64());           
        } else {
            map_model.SetDefaultBagCapacity(settings.global_bag_capacity);
        }
        map_model.SetIdleTimeLimit(settings.retirement_time);
}

void ParseLoot(const boost::json::value& map, model::Map& map_model) {    
    int loot_number = map.at("lootTypes").as_array().size();     
    map_model.SetLootNumber(loot_number);

    std::vector<int> loot_values;
    for (const auto& loot : map.at("lootTypes").as_array()) {
        if (loot.as_object().contains("value")) {
            loot_values.push_back(loot.as_object().at("value").as_int64());
        } else {
            throw InvalidConfigureFile("value of each loot should be specified in configure file");
        }
    }
    map_model.SetLootValues(loot_values);
}

model::LootGeneratorConfig ParseLootGeneratorConfig(const boost::json::value& value) {
    std::chrono::milliseconds period;
    double probability;
    if (value.as_object().contains("lootGeneratorConfig") &&
        value.as_object().at("lootGeneratorConfig").as_object().contains("period") &&
        value.as_object().at("lootGeneratorConfig").as_object().contains("probability")) {
            period = std::chrono::milliseconds(static_cast<int>(value.as_object().at("lootGeneratorConfig").at("period").as_double() * 1000));
            probability = value.as_object().at("lootGeneratorConfig").at("probability").as_double();            
    } else {
        throw InvalidConfigureFile("incorrect lootGeneratorConfig field");
    }
    return {period, probability};
}

DefaultSettings ParseDefaultSettings(const boost::json::value& value) {
    DefaultSettings settings;
    if (value.as_object().contains("defaultDogSpeed")) {
        settings.global_default_speed = value.as_object().at("defaultDogSpeed").as_double() / 1000.;
    }   
    if (value.as_object().contains("defaultBagCapacity")) {
        settings.global_bag_capacity = value.as_object().at("defaultBagCapacity").as_int64();
    }  
    if (value.as_object().contains("dogRetirementTime")) {
        settings.retirement_time = value.as_object().at("dogRetirementTime").as_double() * 1000.;
    } 
    return settings;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::fstream File;
    File.open(json_path.string());
    std::string line;
    std::string result_line;
    while (getline(File, line)) {
        result_line += line;
    }
    auto value = json::parse(result_line);

    model::Game game;    
    DefaultSettings default_settings = ParseDefaultSettings(value);
    LootObjectsInfo loot_info;

    for (auto map : value.as_object().at("maps").as_array()) {
        auto id = map.at(id_key).as_string().data();
        auto name = map.at(name_key).as_string().data();

        model::Map map_model(model::Map::Id(id), name);        

        ParseRoads(map, map_model);        
        ParseBuildings(map, map_model);
        ParseOffices(map, map_model);        
        ParseSettings(map, map_model, default_settings);
        ParseLoot(map, map_model);
        ParseLootInfo(map, loot_info);

        game.AddMap(map_model);
    }   
    game.SetLootObjectsInfo(loot_info);    
    game.SetLootGenerator(ParseLootGeneratorConfig(value));

    return game;
}

}  // namespace json_loader
