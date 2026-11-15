#include "api_handler.h"

#include <boost/json.hpp>

namespace http_handler {

http::response<http::string_body> ApiRequestHandler::ResponseError(http::status status,
                                                                   std::string_view code,
                                                                   std::string_view message,
                                                                   unsigned version) const {
    boost::json::object json_map;
    json_map["code"] = std::string(code);
    json_map["message"] = std::string(message);

    http::response<http::string_body> response(status, version);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(json_map);
    response.prepare_payload();

    return response;
}

std::string_view ApiRequestHandler::DirectionToStateString(model::Direction dir) {
    switch (dir) {
    case model::Direction::NORTH: return "U";
    case model::Direction::SOUTH: return "D";
    case model::Direction::WEST:  return "L";
    case model::Direction::EAST:  return "R";
    }
    return "U";
}

boost::json::object ApiRequestHandler::MapToJson(const model::Map* map) {
    boost::json::object json_map;
    json_map["id"] = *map->GetId();
    json_map["name"] = map->GetName();

    boost::json::array json_roads;
    for (const auto& road : map->GetRoads()) {
        boost::json::object r;
        r["x0"] = road.GetStart().x;
        r["y0"] = road.GetStart().y;
        if (road.IsHorizontal()) r["x1"] = road.GetEnd().x;
        else r["y1"] = road.GetEnd().y;
        json_roads.push_back(r);
    }
    json_map["roads"] = std::move(json_roads);

    boost::json::array json_buildings;
    for (const auto& building : map->GetBuildings()) {
        auto bounds = building.GetBounds();
        json_buildings.push_back(boost::json::object{
            {"x", bounds.position.x}, {"y", bounds.position.y},
            {"w", bounds.size.width}, {"h", bounds.size.height}
        });
    }
    json_map["buildings"] = std::move(json_buildings);

    boost::json::array json_offices;
    for (const auto& office : map->GetOffices()) {
        json_offices.push_back(boost::json::object{
            {"id", *office.GetId()},
            {"x", office.GetPosition().x}, {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx}, {"offsetY", office.GetOffset().dy}
        });
    }
    json_map["offices"] = std::move(json_offices);

    if (const auto* loot_types_json = repo_.FindLootTypes(*map->GetId())) {
        json_map["lootTypes"] = *loot_types_json;
    } else {
        json_map["lootTypes"] = boost::json::array{};
    }

    return json_map;
}

std::map<std::string, std::string> ApiRequestHandler::ParseQueryParameters(std::string_view target) const {
    std::map<std::string, std::string> params;
    size_t query_pos = target.find('?');
    if (query_pos == std::string_view::npos) return params;

    std::string_view query = target.substr(query_pos + 1);
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp_pos = query.find('&', pos);
        std::string_view kv = (amp_pos == std::string_view::npos) ? query.substr(pos) : query.substr(pos, amp_pos - pos);

        size_t eq_pos = kv.find('=');
        if (eq_pos != std::string_view::npos) {
            params[std::string(kv.substr(0, eq_pos))] = std::string(kv.substr(eq_pos + 1));
        }
        if (amp_pos == std::string_view::npos) break;
        pos = amp_pos + 1;
    }
    return params;
}
}  // namespace http_handler
