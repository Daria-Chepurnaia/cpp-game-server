#include "application.h"

namespace app {

std::string Application::GetPlayersJSONInfo (std::shared_ptr<app::Player> player_ptr) {
    auto players = player_ptr->GetSession()->GetPlayers();
    json::object root;
    for (auto& player : players) {
        json::object player_name_info;
        player_name_info.insert({{"name", player->GetName()}});
        root.insert({{std::to_string(player->GetId()), player_name_info}});
    }
    return json::serialize(root);
}

std::string Application::GetStateJSONInfo(std::shared_ptr<app::Player> player_ptr) {
    auto players = player_ptr->GetSession()->GetPlayers();
    json::object root;
    json::object players_dict;
    for (auto& player : players) {
        json::object player_state_info;
        
        json::array position = {player->GetCoordinates().x,
                                player->GetCoordinates().y,};           
        json::array speed = {player->GetSpeed().x * 1000.,
                                player->GetSpeed().y * 1000.,};
        std::string direction = dir_to_letter.at(player->GetDirection());
        int score = player->GetScore();            
        json::array bag;            
        for (const auto& item : player->GetBag()) {
            json::object item_info;
            item_info.insert({{"id", item.id}});
            item_info.insert({{"type", item.type}});
            bag.emplace_back(item_info);
        }
    
        player_state_info.insert({{"pos", position}});
        player_state_info.insert({{"speed", speed}});
        player_state_info.insert({{"dir", direction}});
        player_state_info.insert({{"bag", bag}});
        player_state_info.insert({{"score", score}});
        players_dict.insert({{std::to_string(player->GetId()), player_state_info}});
    }
    root.insert({{"players", players_dict}});
    auto loot_objects = player_ptr->GetSession()->GetLostObjects();
    json::object loot_dict;
    for (const auto& [obj_id, object] : loot_objects) {
        json::object loot_info;
        int id = obj_id;
        int type = object.item.type;
        json::array position = {object.coordinates.x,
                                object.coordinates.y,};
        loot_info.insert({{"type", type}}); 
        loot_info.insert({{"pos", position}});
        
        loot_dict.insert({{std::to_string(id), loot_info}});            
    }
    root.insert({{"lostObjects", loot_dict}});
    return json::serialize(root);
}

std::string Application::GetRecordsJSONInfo(std::optional<int> start_element, std::optional<int> maxItems) {
    json::array root;
    std::vector<RetiredPlayerInfo> players_records = db_->GetPlayers(start_element, maxItems); 
    for (const auto& player : players_records) {
        json::object player_info;
        player_info.insert({{"name", player.name}});
        player_info.insert({{"score", player.score}});
        player_info.insert({{"playTime", player.total_time_in_game}});
        root.push_back(player_info);
    }
    return json::serialize(root);
}


boost::json::object Application::GetJSONforMap(const model::Map::Id &id) const {
    boost::json::object root;
    auto map = game_.FindMap(id);
    auto name = map->GetName();
    auto roads = map->GetRoads();
    auto buildings = map->GetBuildings();
    auto offices = map->GetOffices();
    auto loot_info = game_.GetLootInfo(*id);
    root.insert({{"id", *id}});
    root.insert({{"name", name}});
    boost::json::array roads_array;
    for (const auto& road : roads) {
        boost::json::object road_obj;
        road_obj.insert({{"x0", road.GetStart().x}});
        road_obj.insert({{"y0", road.GetStart().y}});
        if (road.IsHorizontal()) {
            road_obj.insert({{"x1", road.GetEnd().x}});
        } else {
            road_obj.insert({{"y1", road.GetEnd().y}});
        }
        roads_array.push_back(road_obj);
    }
    root.insert({{"roads", roads_array}});
    boost::json::array buildings_array;
    for (const auto& building : buildings) {
        boost::json::object building_obj;
        auto bounds = building.GetBounds();
        building_obj.insert({{"x", bounds.position.x}});
        building_obj.insert({{"y", bounds.position.y}});
        building_obj.insert({{"w", bounds.size.width}});
        building_obj.insert({{"h", bounds.size.height}});
        buildings_array.push_back(building_obj);
    }
    if (!buildings_array.empty()) {
        root.insert({{"buildings", buildings_array}});
    }
    boost::json::array offices_array;
    for (const auto& office : offices) {
        boost::json::object office_obj;
        office_obj.insert({{"id", *(office.GetId())}});
        office_obj.insert({{"x", office.GetPosition().x}});
        office_obj.insert({{"y", office.GetPosition().y}});
        office_obj.insert({{"offsetX", office.GetOffset().dx}});
        office_obj.insert({{"offsetY", office.GetOffset().dy}});
        offices_array.push_back(office_obj);
    }
    if (!offices_array.empty()) {
        root.insert({{"offices", offices_array}});
    }
    root.insert({{"lootTypes", loot_info.properties.as_array()}});
    return root;
}

boost::json::array Application::GetJSONforAllMaps() const {
    boost::json::array root;
    for (const auto& map : game_.GetMaps()) {
        std::string id = *(map.GetId());
        std::string name = map.GetName();
        boost::json::object map_to_add;
        map_to_add.insert({{"id", id}});
        map_to_add.insert({{"name", name}}); 
        root.push_back(map_to_add);
    }
    return root;
}

void Application::Move(std::shared_ptr<app::Player> player_ptr, std::string direction) {    
    player_ptr->Move(direction);
}

void Application::UpdateTime(double time_delta) {
    game_.UpdateTime(time_delta);
    tick_signal_(time_delta);
}

const model::Map* Application::FindMap(model::Map::Id(map_id)) {
    return game_.FindMap(map_id);
}

std::shared_ptr<app::Player> Application::JoinGame(const std::string& name, const model::Map* map) {       
    return game_.JoinGame(name, map);
}

std::shared_ptr<app::Player> Application::GetPlayerByToken(const std::string& token) const {
    return game_.GetPlayerByToken(token);
} 

sig::connection Application::DoOnTimeUpdate(const TickSignal::slot_type& handler) {
    return tick_signal_.connect(handler);
}

void Application::SaveState() const {
    if (state_save_file_path_) {
        serialization::SaveGameStateInFile(*state_save_file_path_, game_);
    }
}

}//namespace application
