#include "model_serialization.h"

namespace serialization {
void RestoreGameState(std::string path, model::Game& game) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    try {
        std::ifstream ifs(path);

        boost::archive::text_iarchive ia{ifs}; 
        std::map<int, model::PlayerRepr> players_info;
        std::map<int, model::Game::LostObjects> loot_info;
        
        ia >> players_info;
        ia >> loot_info;

        for (const auto& [id, player] : players_info) {
            auto player_ptr = game.InitializePlayerForRestore(player.name, player.token, id, game.FindMap(model::Map::Id(player.map_id)));
            player_ptr->RestorePlayerState( player.score, 
                                            player.idle_time,
                                            player.total_time,
                                            player.coordinates, 
                                            player.speed,
                                            player.direction,
                                            player.bag);
        } 

        game.RestoreLootForAllSessions(loot_info);
    }
    catch(const std::exception& e) {
        throw std::runtime_error("Error restoring game data: corrupted file");
    }    
}

void SaveGameStateInFile(std::string path,  model::Game& game) {
    namespace fs = std::filesystem;
    
    std::string temp_path = path + ".tmp";

    std::ofstream ofs(temp_path);

    boost::archive::text_oarchive oa{ofs};

    //serialize players info
    std::map<int, model::PlayerRepr> players_info;
    std::map<int, std::shared_ptr<app::Player>> players = game.GetPlayers();
    for (const auto[id, player] : players) {
        players_info[id] = {player->GetName(),
                            player->GetToken(), 
                            player->GetMapID(), 
                            player->GetScore(),                            
                            player->GetIdleTime(),                            
                            player->GetTotalTime(),                            
                            player->GetCoordinates(),
                            player->GetSpeed(),
                            player->GetDirection(),
                            player->GetBag()};
    }

    //serialize loot info
    std::map<int, model::Game::LostObjects> loot_info = game.RetrieveLootForBackup();
 
    oa << players_info;
    oa << loot_info;

    ofs.close();
    if (fs::exists(path)) {
        fs::remove(path);
    }
    fs::rename(temp_path, path);
}
}// namespace serialization