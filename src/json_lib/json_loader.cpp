#include "json_loader.h"

#include <iostream>
#include <fstream>
#include <string>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

const std::string x_label = "x";
const std::string y_label = "y";
const std::string width_label = "w";
const std::string height_label = "h";

void AddRoadToGameMap(model::Map& game_map, const boost::json::value& road) {
    auto const& road_obj = road.as_object();

    auto x0 = json::value_to<model::Coord>(road_obj.at("x0"));
    auto y0 = json::value_to<model::Coord>(road_obj.at("y0"));

    if(road_obj.contains("x1")) {
        const model::Road new_road(model::Road::HORIZONTAL
                                 , model::Point(x0, y0)
                                 , json::value_to<model::Coord>(road_obj.at("x1")));
        game_map.AddRoad(new_road);
    }
    else {
        const model::Road new_road(model::Road::VERTICAL
                                 , model::Point(x0, y0)
                                 , json::value_to<model::Coord>(road_obj.at("y1")));
        game_map.AddRoad(new_road);
    }
}

void AddBuildToGameMap(model::Map& game_map, const boost::json::value& building) {
    auto const& building_obj = building.as_object();

    auto x = json::value_to<model::Coord>(building_obj.at(x_label));
    auto y = json::value_to<model::Coord>(building_obj.at(y_label));
    auto w = json::value_to<model::Dimension>(building_obj.at(width_label));
    auto h = json::value_to<model::Dimension>(building_obj.at(height_label));

    const model::Building new_building(model::Rectangle(model::Point(x, y)
                                     , model::Size(w, h)));
    game_map.AddBuilding(new_building);
}

void AddOfficeToGameMap(model::Map& game_map, const boost::json::value& office) {
    auto const& office_obj = office.as_object();

    auto id = model::Office::Id{json::value_to<std::string>(office_obj.at("id"))};
    auto x = json::value_to<model::Coord>(office_obj.at(x_label));
    auto y = json::value_to<model::Coord>(office_obj.at(y_label));
    auto dx = json::value_to<model::Coord>(office_obj.at("offsetX"));
    auto dy = json::value_to<model::Coord>(office_obj.at("offsetY"));

    model::Office new_office(id, model::Point(x, y), model::Offset(dx, dy));
    game_map.AddOffice(new_office);
}

void AddLootType(model::Map& game_map, const boost::json::value& loot_type) {
    game_map.AddLootType(model::LootType{});
}

model::Map CreateGameMap(const boost::json::value& map, model::DimensionDouble default_speed, std::uint8_t default_bag_capacity, infrastructure::FrontendLootRepository& repo) {
    auto const& map_obj = map.as_object();

    model::Map game_map(model::Map::Id{json::value_to<std::string>(map_obj.at("id"))}
                                     , json::value_to<std::string>(map_obj.at("name")));

    for(const auto& road : map_obj.at("roads").as_array()) {
        AddRoadToGameMap(game_map, road);
    }
    for(const auto& building : map_obj.at("buildings").as_array()) {
        AddBuildToGameMap(game_map, building);
    }
    for(const auto& office : map_obj.at("offices").as_array()) {
        AddOfficeToGameMap(game_map, office);
    }


    if (auto it = map_obj.find("dogSpeed"); it != map_obj.end()) {
        game_map.SetDogSpeed(json::value_to<model::DimensionDouble>(it->value()));
    } else {
        game_map.SetDogSpeed(default_speed);
    }

    if (auto it = map_obj.find("bagCapacity"); it != map_obj.end()) {
        game_map.SetBagCapacity(json::value_to<std::uint8_t>(it->value()));
    } else {
        game_map.SetBagCapacity(default_bag_capacity);
    }

    if (auto it = map_obj.find("lootTypes"); it != map_obj.end()) {
        auto loot_types_array = map_obj.at("lootTypes").as_array();
        for (size_t i = 0; i < loot_types_array.size(); ++i) {
            auto const& loot_obj = loot_types_array[i].as_object();

            int value = 0;
            if (loot_obj.contains("value")) {
                value = static_cast<int>(loot_obj.at("value").as_int64());
            }
            game_map.AddLootType(model::LootType{value});
        }
        repo.SaveLootTypes(*game_map.GetId(), std::move(loot_types_array));
    }

    return game_map;
}

model::Game LoadGame(const fs::path& json_path, infrastructure::FrontendLootRepository& repo, model::Players& players, model::IRecordRepository& db) {
    if (!fs::exists(json_path)) {
        std::runtime_error("Error: File not found!");
    }

    if (!fs::is_regular_file(json_path)) {
        std::runtime_error("Error: This is not file!");
    }

    if (fs::file_size(json_path) == 0) {
        std::runtime_error("Error: File is empty!");
    }

    std::ifstream file(json_path);

    if (!file.is_open()) {
        std::runtime_error("Error: Could not open the file!");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_line = buffer.str();

    json::error_code ec;
    auto value = json::parse(json_line, ec);
    if (ec) {
        throw std::runtime_error("Error: " + ec.message());
    }

    model::DimensionDouble dog_speed = json::value_to<model::DimensionDouble>(value.at("defaultDogSpeed"));

    std::uint8_t bag_capacity = 3;
    if (value.is_object()) {
        auto const& root_obj = value.as_object();

        if (root_obj.contains("defaultBagCapacity")) {
            bag_capacity = boost::json::value_to<std::uint8_t>(root_obj.at("defaultBagCapacity"));
        }
    }

    auto loot_config = value.at("lootGeneratorConfig").as_object();
    double period_seconds = loot_config.at("period").as_double();
    double probability = loot_config.at("probability").as_double();

    auto period_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(period_seconds));

    double retirement_time = 60.0;
    if (value.is_object() && value.as_object().contains("dogRetirementTime")) {
        retirement_time = value.as_object().at("dogRetirementTime").as_double();
    }

    model::Game game(period_ms, probability, players, db);

    game.SetDogRetirementTime(retirement_time);

    auto const& maps = value.as_object().at("maps").as_array();

    for(const auto& map : maps) {
        game.AddMap(CreateGameMap(map, dog_speed, bag_capacity, repo));
    }

    return game;
}
}  // namespace json_loader
