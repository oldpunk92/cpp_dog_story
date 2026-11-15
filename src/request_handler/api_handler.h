#pragma once

#include "game_model/model.h"
#include "game_model/infrastructure.h"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <variant>
#include <utility>

namespace http_handler {

using namespace std::literals;

namespace json = boost::json;
namespace beast = boost::beast;
namespace http = beast::http;

class ApiRequestHandler {
private:
    inline static const std::string_view CODE_BAD_REQUEST = "badRequest"sv;
    inline static const std::string_view CODE_NOT_FOUND = "mapNotFound"sv;
    inline static const std::string_view CODE_INVALID_ARGUMENT = "invalidArgument"sv;
    inline static const std::string_view CODE_INVALID_METHOD = "invalidMethod"sv;
    inline static const std::string_view CODE_SESSION_FULL = "sessionFull"sv;
    inline static const std::string_view CODE_INVALID_TOKEN = "invalidToken"sv;
    inline static const std::string_view CODE_UNKNOWN_TOKEN = "unknownToken"sv;

    std::string_view DirectionToStateString(model::Direction dir);
    boost::json::object MapToJson(const model::Map* map);
    http::response<http::string_body> ResponseError(http::status status, std::string_view code, std::string_view message, unsigned version) const;
    std::map<std::string, std::string> ParseQueryParameters(std::string_view target) const;

    template <typename JsonObject>
    http::response<http::string_body> ResponseOk(unsigned version, JsonObject obj) const {
        http::response<http::string_body> response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = boost::json::serialize(obj);
        response.prepare_payload();
        return response;
    }

    //---------------Authorize-------------------------------------------------------------------------------------------

    using AuthorizeResult = std::variant<model::Player*, http::response<http::string_body>>;

    template <typename Body, typename Allocator>
    AuthorizeResult Authorize(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        auto version = req.version();

        auto auth_header_it = req.find(http::field::authorization);
        if (auth_header_it == req.end()) {
            return ResponseError(http::status::unauthorized, CODE_INVALID_TOKEN, "Authorization header is missing", version);
        }

        std::string_view auth_value = auth_header_it->value();
        std::string_view bearer_prefix = "Bearer ";

        if (!auth_value.starts_with(bearer_prefix)) {
            return ResponseError(http::status::unauthorized, CODE_INVALID_TOKEN, "Authorization header is invalid", version);
        }

        auth_value.remove_prefix(bearer_prefix.size());

        if (auth_value.size() != 32) {
            return ResponseError(http::status::unauthorized, CODE_INVALID_TOKEN, "Token length is invalid", version);
        }

        model::PlayerTokens::Token token{std::string(auth_value)};
        model::Player* target_player = players_.FindByToken(token);

        if (!target_player) {
            return ResponseError(http::status::unauthorized, CODE_UNKNOWN_TOKEN, "Player token has not been found", version);
        }

        return target_player;
    }

    //------------------------------------------------------------------------------------------------------------------

    //---------------HandleJoinGame-------------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleJoinGame(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto method = req.method();
        auto target = req.target();
        auto version = req.version();

        if (method != http::verb::post) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Only POST method is allowed", version);
            response.set(http::field::allow, "POST");
            response.prepare_payload();

            return response;
        }
        if (req[http::field::content_type] != "application/json") {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Invalid content type"sv, version);
        }

        std::string user_name;
        std::string map_id_str;

        try {
            auto value = json::parse(req.body());
            if (!value.is_object()) {
                throw std::exception();
            }

            const auto& obj = value.as_object();

            if (!obj.contains("userName") || !obj.contains("mapId")) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Join game request parsing error"sv, version);
            }

            user_name = json::value_to<std::string>(obj.at("userName"));
            map_id_str = json::value_to<std::string>(obj.at("mapId"));

            auto is_blank = [](std::string_view str) {
                return std::all_of(str.begin(), str.end(), [](unsigned char ch) {
                    return std::isspace(ch);
                });
            };

            if (user_name.empty() || is_blank(user_name)) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Invalid name"sv, version);
            }
        } catch (...) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Join game request parsing error"sv, version);
        }

        model::Map::Id map_id{map_id_str};
        const model::Map* map = game_.FindMap(map_id);
        if (!map) {
            return ResponseError(http::status::not_found, CODE_NOT_FOUND, "Map not found"sv, version);
        }

        model::GameSession* session = game_.FindSessionByMapId(map_id);
        if (!session) {
            session = game_.CreateSession(map_id);
        }
        if (session == nullptr) {
            return ResponseError(http::status::not_found, CODE_NOT_FOUND, "Map not found"sv, version);
        }

        auto [player_ptr, token] = players_.Add(user_name, *session);
        if (!player_ptr) {
            return ResponseError(http::status::internal_server_error, CODE_SESSION_FULL, "Cannot add player to session"sv, version);
        }

        json::object res;
        res["authToken"] = *token;
        res["playerId"] = player_ptr->GetDogId();

        return ResponseOk(version, res);
    }

    //------------------------------------------------------------------------------------------------------------------

    //---------------HandleGetMaps--------------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGetMaps(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto method = req.method();
        auto target = req.target();
        auto version = req.version();

        if (target == "/api/v1/maps" || target == "/api/v1/maps/") {
            if (method == http::verb::get || method == http::verb::head) {
                boost::json::array json_maps;
                for (const auto& map : game_.GetMaps()) {
                    boost::json::object map_obj;
                    map_obj["id"] = *map.GetId();
                    map_obj["name"] = map.GetName();
                    json_maps.push_back(map_obj);
                }
                return ResponseOk(version, json_maps);
            }
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Invalid method"sv, version);
            response.set(http::field::allow, "GET, HEAD");
            response.prepare_payload();
            return response;
        } else {
            if (method == http::verb::get || method == http::verb::head) {
                auto map_id = target.substr(13);
                auto map = game_.FindMap(model::Map::Id{std::string{map_id}});

                if (map == nullptr) {
                    return ResponseError(http::status::not_found, CODE_NOT_FOUND, "Map not found"sv, version);
                }

                auto json_map = MapToJson(map);
                return ResponseOk(version, json_map);
            }
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Invalid method"sv, version);
            response.set(http::field::allow, "GET, HEAD");
            response.prepare_payload();
            return response;
        }
        return ResponseError(http::status::bad_request, CODE_BAD_REQUEST, "Bad request"sv, version);
    }

    //------------------------------------------------------------------------------------------------------------------

    //---------------HandleGetPlayers-----------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGetPlayers(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto version = req.version();
        auto method = req.method();

        if (method != http::verb::get && method != http::verb::head) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Invalid method"sv, version);
            response.set(http::field::allow, "GET, HEAD");
            response.prepare_payload();
            return response;
        }

        auto auth_result = Authorize(req);
        if (auto* error_response = std::get_if<http::response<http::string_body>>(&auth_result)) {
            return std::move(*error_response);
        }

        model::Player* target_player = std::get<model::Player*>(auth_result);
        model::GameSession* current_session = target_player->GetSession();

        boost::json::object players_list_obj;

        for (const auto& [player_ptr, player_token] : players_.GetPlayerToTokenMap()) {
            if (player_ptr->GetSession() == current_session) {
                boost::json::object player_info;
                player_info["name"] = player_ptr->GetDog()->GetName();

                std::string dog_id_str = std::to_string(player_ptr->GetDogId());
                players_list_obj[dog_id_str] = player_info;
            }
        }
        return ResponseOk(version, players_list_obj);
    }

    //------------------------------------------------------------------------------------------------------------------

    //---------------HandleGetGameState---------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGetGameState(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto version = req.version();
        auto method = req.method();

        if (method != http::verb::get && method != http::verb::head) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Invalid method"sv, version);
            response.set(http::field::allow, "GET, HEAD");
            return response;
        }

        auto auth_result = Authorize(req);
        if (auto* error_response = std::get_if<http::response<http::string_body>>(&auth_result)) {
            return std::move(*error_response);
        }

        model::Player* target_player = std::get<model::Player*>(auth_result);
        model::GameSession* current_session = target_player->GetSession();

        boost::json::object players_state_map;

        for (const auto& [player_ptr, player_token] : players_.GetPlayerToTokenMap()) {
            if (player_ptr->GetSession() == current_session) {
                boost::json::object single_player_state;
                const auto* dog = player_ptr->GetDog();

                boost::json::array pos_array;
                pos_array.push_back(dog->GetPosition().x);
                pos_array.push_back(dog->GetPosition().y);
                single_player_state["pos"] = pos_array;

                boost::json::array speed_array;
                speed_array.push_back(dog->GetSpeed().ux);
                speed_array.push_back(dog->GetSpeed().uy);
                single_player_state["speed"] = speed_array;

                single_player_state["dir"] = std::string(DirectionToStateString(dog->GetDirection()));

                single_player_state["score"] = dog->GetScore();

                boost::json::array bag_json;
                for (const auto& obj : dog->GetBag()) {
                    boost::json::object obj_json;
                    obj_json["id"] = obj.id;
                    obj_json["type"] = obj.type;
                    bag_json.push_back(obj_json);
                }
                single_player_state["bag"] = bag_json;

                std::string dog_id_str = std::to_string(player_ptr->GetDogId());
                players_state_map[dog_id_str] = single_player_state;
            }
        }

        boost::json::object root_response_obj;
        root_response_obj["players"] = players_state_map;

        boost::json::object lost_objects_json;
        for (const auto& [id, lost_object] : current_session->GetLostObjects()) {
            boost::json::object obj_json;
            obj_json["type"] = lost_object.type;
            obj_json["pos"] = boost::json::array{lost_object.pos.x, lost_object.pos.y};

            lost_objects_json[std::to_string(id)] = obj_json;
        }
        root_response_obj["lostObjects"] = lost_objects_json;

        return ResponseOk(version, root_response_obj);
    }

    //------------------------------------------------------------------------------------------------------------------

    //---------------HandlePlayerAction---------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandlePlayerAction(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto version = req.version();
        auto method = req.method();

        if (method != http::verb::post) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Only POST method is allowed"sv, version);
            response.set(http::field::allow, "POST");
            response.prepare_payload();

            return response;
        }
        if (req[http::field::content_type] != "application/json") {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Invalid content type"sv, version);
        }

        auto auth_result = Authorize(req);
        if (auto* error_response = std::get_if<http::response<http::string_body>>(&auth_result)) {
            return std::move(*error_response);
        }

        model::Player* player = std::get<model::Player*>(auth_result);
        model::Dog* dog = player->GetDog();
        model::GameSession* session = player->GetSession();

        model::DimensionDouble s = session->GetMap()->GetDogSpeed();

        std::string move_direction;
        try {
            auto value = boost::json::parse(req.body());
            if (!value.is_object()) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse action"sv, version);
            }

            const auto& obj = value.as_object();
            if (!obj.contains("move")) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse action"sv, version);
            }

            move_direction = boost::json::value_to<std::string>(obj.at("move"));
        } catch (...) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse action"sv, version);
        }

        model::Speed2D new_speed{0.0, 0.0};

        if (move_direction == "L") {
            new_speed = {-s, 0.0};
            dog->SetDirection(model::Direction::WEST);
        } else if (move_direction == "R") {
            new_speed = {s, 0.0};
            dog->SetDirection(model::Direction::EAST);
        } else if (move_direction == "U") {
            new_speed = {0.0, -s};
            dog->SetDirection(model::Direction::NORTH);
        } else if (move_direction == "D") {
            new_speed = {0.0, s};
            dog->SetDirection(model::Direction::SOUTH);
        } else if (move_direction.empty()) {
            new_speed = {0.0, 0.0};
        } else {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse action"sv, version);
        }

        dog->SetSpeed(new_speed);
        dog->ResetIdleTime();

        boost::json::object empty_res;
        return ResponseOk(version, empty_res);
    }

    //------------------------------------------------------------------------------------------------------------------

    //-------------------HandleGameTick---------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGameTick(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto version = req.version();

        if (req.method() != http::verb::post) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Invalid method"sv, version);
            response.set(http::field::allow, "POST");
            return response;
        }

        int64_t time_delta_ms = 0;
        try {
            auto value = boost::json::parse(req.body());
            if (!value.is_object()) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse tick request"sv, version);
            }

            const auto& obj = value.as_object();
            if (!obj.contains("timeDelta")) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse tick request"sv, version);
            }

            time_delta_ms = obj.at("timeDelta").as_int64();
            if (time_delta_ms < 0) {
                return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Invalid time delta"sv, version);
            }
        } catch (...) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse tick request"sv, version);
        }

        double time_delta_sec = static_cast<double>(time_delta_ms) / 1000.0;
        game_.Tick(time_delta_sec);

        if (state_manager_ && state_manager_->HasStateFile()) {
            std::chrono::milliseconds delta(time_delta_ms);
            state_manager_->OnTick(delta, game_, players_);
        }

        boost::json::object empty_res;
        return ResponseOk(version, empty_res);
    }

    //------------------------------------------------------------------------------------------------------------------

    //-------------------HandleGameRecord---------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGameRecord(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto method = req.method();
        auto version = req.version();

        if (method != http::verb::get && method != http::verb::head) {
            auto response = ResponseError(http::status::method_not_allowed, CODE_INVALID_METHOD, "Only GET and HEAD methods are allowed"sv, version);
            response.set(http::field::allow, "GET, HEAD");
            return response;
        }

        auto query_params = ParseQueryParameters(req.target());
        int start = 0;
        int max_items = 100;

        try {
            if (query_params.contains("start")) {
                start = std::stoi(query_params["start"]);
            }
            if (query_params.contains("maxItems")) {
                max_items = std::stoi(query_params["maxItems"]);
            }
        } catch (...) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Failed to parse query parameters"sv, version);
        }

        if (max_items > 100) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "maxItems cannot be greater than 100"sv, version);
        }
        if (start < 0 || max_items < 0) {
            return ResponseError(http::status::bad_request, CODE_INVALID_ARGUMENT, "Parameters cannot be negative"sv, version);
        }

        auto leaderboard = db_.GetLeaderboard(start, max_items);

        boost::json::array json_records;
        for (const auto& record : leaderboard) {
            boost::json::object rec_obj;
            rec_obj["name"] = record.name;
            rec_obj["score"] = record.score;
            rec_obj["playTime"] = record.play_time;
            json_records.push_back(rec_obj);
        }

        return ResponseOk(version, json_records);
    }

    //------------------------------------------------------------------------------------------------------------------

public:
    explicit ApiRequestHandler(model::Game& game,
                               model::Players& players,
                               infrastructure::FrontendLootRepository& repo,
                               std::shared_ptr<infrastructure::StateManager> state_manager,
                               model::IRecordRepository& db)
        : game_(game)
        , players_(players)
        , repo_(repo)
        , state_manager_(state_manager)
        , db_(db) {}

    static bool IsApiRequest(std::string_view target) noexcept {
        return target.starts_with("/api/v1/");
    }

    //-------------------HandleApiRequest------------------------------------------------------------------------------

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleApiRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto target = req.target();

        if (!IsApiRequest(target)) {
            return ResponseError(http::status::bad_request, CODE_BAD_REQUEST, "Bad request REST API"sv, req.version());
        }

        if (target == "/api/v1/game/join") {
            return HandleJoinGame(req);
        }
        if (target.starts_with("/api/v1/maps")) {
            return HandleGetMaps(req);
        }
        if (target == "/api/v1/game/players") {
            return HandleGetPlayers(req);
        }
        if (target == "/api/v1/game/state" || target == "/api/v1/game/state/") {
            return HandleGetGameState(req);
        }
        if (target == "/api/v1/game/player/action") {
            return HandlePlayerAction(req);
        }
        if (target == "/api/v1/game/tick") {
            return HandleGameTick(req);
        }
        if (target.starts_with("/api/v1/game/records")) {
            return HandleGameRecord(req);
        }
        return ResponseError(http::status::bad_request, CODE_BAD_REQUEST, "Bad request"sv, req.version());
    }

    //-------------------------------------------------------------------------------------------------------------------

private:
    model::Game& game_;
    model::Players& players_;
    infrastructure::FrontendLootRepository& repo_;
    std::shared_ptr<infrastructure::StateManager> state_manager_;
    model::IRecordRepository& db_;
};
}  // namespace http_handler
