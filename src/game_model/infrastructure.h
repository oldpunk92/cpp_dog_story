#pragma once

#include "model_serialization.h"
#include "model.h"

#include <unordered_map>
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/log/trivial.hpp>

namespace infrastructure {

class FrontendLootRepository {
public:
    void SaveLootTypes(const std::string& map_id, boost::json::array loot_types) {
        map_to_loot_types_[map_id] = std::move(loot_types);
    }

    const boost::json::array* FindLootTypes(const std::string& map_id) const {
        auto it = map_to_loot_types_.find(map_id);
        if (it != map_to_loot_types_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, boost::json::array> map_to_loot_types_;
};

class StateManager {
public:
    StateManager(std::optional<std::string> state_file, std::optional<uint32_t> save_period)
        : state_file_(std::move(state_file))
        , save_period_ms_(save_period) {}

    bool HasStateFile() const noexcept { return state_file_.has_value(); }

    void OnTick(std::chrono::milliseconds delta, model::Game& game, model::Players& players) {
        if (!state_file_ || !save_period_ms_) {
            return;
        }
        accumulated_time_ms_ += delta.count();
        if (accumulated_time_ms_ >= *save_period_ms_) {
            SaveState(game, players);
            accumulated_time_ms_ = 0;
        }
    }

    void SaveState(model::Game& game, model::Players& players) {
        if (!state_file_) return;

        try {
            serialization::GameStateRepr repr;

            for (const auto& map : game.GetMaps()) {
                if (auto* session = game.FindSessionByMapId(map.GetId())) {
                    serialization::GameStateRepr::SessionSnapshot session_snap;
                    session_snap.next_dog_id = session->GetNextDogId();
                    session_snap.free_dog_ids = session->GetFreeIds();
                    session_snap.next_loot_id = session->GetNextLootId();
                    session_snap.lost_objects = session->GetLostObjects();

                    for (const auto& [dog_id, dog_obj] : session->GetDogs()) {
                        session_snap.dogs[dog_id] = serialization::DogRepr(dog_obj);
                    }
                    repr.sessions[*map.GetId()] = std::move(session_snap);
                }
            }

            for (const auto& [player_ptr, token] : players.GetPlayerToTokenMap()) {
                repr.players.push_back({
                    *player_ptr->GetSession()->GetMapId(),
                    player_ptr->GetDogId(),
                    *token
                });
            }

            std::filesystem::path target_path(*state_file_);
            std::filesystem::path tmp_path = target_path.string() + ".tmp";
            {
                std::ofstream ofs(tmp_path);
                boost::archive::text_oarchive oa(ofs);
                oa << repr;
            }
            std::filesystem::rename(tmp_path, target_path);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to auto-save state: " << e.what();
        }
    }

    bool RestoreState(model::Game& game, model::Players& players) {
        if (!state_file_ || !std::filesystem::exists(*state_file_)) {
            return true;
        }

        try {
            std::ifstream ifs(*state_file_);
            boost::archive::text_iarchive ia(ifs);
            serialization::GameStateRepr repr;
            ia >> repr;

            for (const auto& [map_id_str, snap] : repr.sessions) {
                model::Map::Id map_id{map_id_str};
                auto* session = game.CreateSession(map_id);
                if (!session) continue;

                std::map<uint32_t, model::Dog> restored_dogs;
                for (const auto& [dog_id, dog_repr] : snap.dogs) {
                    restored_dogs.emplace(dog_id, dog_repr.Restore());
                }


                session->RestoreDynamicState(
                    snap.next_dog_id,
                    snap.free_dog_ids,
                    std::move(restored_dogs),
                    snap.lost_objects,
                    snap.next_loot_id
                );
            }

            for (const auto& p_snap : repr.players) {
                model::Map::Id map_id{p_snap.map_id};
                auto* session = game.FindSessionByMapId(map_id);
                if (!session) continue;

                const auto& dogs = session->GetDogs();
                auto it = dogs.find(p_snap.dog_id);
                if (it == dogs.end()) continue;

                model::Dog* dog_ptr = const_cast<model::Dog*>(&it->second);
                auto player = std::make_unique<model::Player>(session, dog_ptr, p_snap.dog_id);

                players.RestorePlayer(model::PlayerTokens::Token{p_snap.token}, std::move(player));
            }
            return true;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error restoring state: " << e.what();
            return false;
        }
    }

private:
    std::optional<std::string> state_file_;
    std::optional<uint32_t> save_period_ms_;
    uint64_t accumulated_time_ms_ = 0;
};

} // namespace infrastructure
