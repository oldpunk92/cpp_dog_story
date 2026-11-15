#pragma once

#include <filesystem>
#include <boost/json.hpp>

#include "game_model/model.h"
#include "game_model/infrastructure.h"


namespace json_loader {

namespace fs = std::filesystem;

model::Game LoadGame(const fs::path& json_path, infrastructure::FrontendLootRepository& repo, model::Players& players, model::IRecordRepository& db);

model::Map CreateGameMap(const boost::json::value& map, model::DimensionDouble default_speed, std::uint8_t default_bag_capacity, infrastructure::FrontendLootRepository& repo);
void AddRoadToGameMap(model::Map& game_map, const boost::json::value& road);
void AddBuildToGameMap(model::Map& game_map, const boost::json::value& building);
void AddOfficeToGameMap(model::Map& game_map, const boost::json::value& office);

}  // namespace json_loader
