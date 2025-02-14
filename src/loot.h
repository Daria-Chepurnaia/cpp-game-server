#pragma once

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <boost/json.hpp>

struct LootProperties {
    boost::json::value properties;
};

class LootObjectsInfo {
public:    
    void AddLootforMap(const std::string& id, const LootProperties& loot_info) {
        map_id_to_loot_info_[id] = loot_info;
    }

    LootProperties GetLootInfo(const std::string& map_id) {
        return map_id_to_loot_info_.at(map_id);
    }

private:
    std::map<std::string, LootProperties> map_id_to_loot_info_;
}; 

