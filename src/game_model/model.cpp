#include "model.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <limits>


namespace model {
using namespace std::literals;

//-----------MAP----------------------------------------------------

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

struct Interval {
    double min;
    double max;
};

void Map::AddRoad(const Road& road) {
    roads_.push_back(road);
    Point start = road.GetStart();

    if (road.IsHorizontal()) {
        horizontal_roads_by_y_[start.y].push_back(road);
    } else {
        vertical_roads_by_x_[start.x].push_back(road);
    }
}

const std::vector<Road>& Map::GetHorizontalRoadsByY(int y) const {
    static const std::vector<Road> empty;
    auto it = horizontal_roads_by_y_.find(y);
    return (it != horizontal_roads_by_y_.end()) ? it->second : empty;
}

const std::vector<Road>& Map::GetVerticalRoadsByX(int x) const {
    static const std::vector<Road> empty;
    auto it = vertical_roads_by_x_.find(x);
    return (it != vertical_roads_by_x_.end()) ? it->second : empty;
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::SetDogSpeed(DimensionDouble speed) noexcept {
    dog_speed_ = speed;
}
DimensionDouble Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}

void Map::AddLootType(LootType loot) {
    loot_types_.push_back(std::move(loot));
}
const std::vector<LootType> Map::GetLootTypes() const noexcept {
    return loot_types_;
}

void Map::SetBagCapacity(std::uint8_t capacity) noexcept {
    bag_capacity_ = capacity;
}
std::uint8_t Map::GetBagCapacity() const noexcept {
    return bag_capacity_;
}

//-----------DOG----------------------------------------------------

const std::string& Dog::GetName() const noexcept {
    return name_;
}
const Point2D& Dog::GetPosition() const noexcept {
    return position_;
}
const Speed2D& Dog::GetSpeed() const noexcept {
    return speed_;
}
Direction Dog::GetDirection() const noexcept {
    return direction_;
}
std::uint8_t Dog::GetBagCapacity() const noexcept {
    return bag_capacity_;
}

void Dog::SetPosition(Point2D pos) noexcept {
    position_ = pos;
}
void Dog::SetSpeed(Speed2D speed) noexcept {
    speed_ = speed;
}
void Dog::SetDirection(Direction dir) noexcept {
    direction_ = dir;
}
void Dog::SetBagCapacity(std::uint8_t capacity) noexcept {
    bag_capacity_ = capacity;
}

const Point2D& Dog::GetPrevPosition() const noexcept {
    return prev_position_;
}

void Dog::UpdatePrevPosition() noexcept {
    prev_position_ = position_;
}

const std::vector<FoundObject>& Dog::GetBag() const noexcept {
    return bag_;
}

bool Dog::IsBagFull() const noexcept {
    return bag_.size() >= bag_capacity_;
}

void Dog::AddToBag(FoundObject obj) {
    if (!IsBagFull()) {
        bag_.push_back(obj);
    }
}

std::vector<FoundObject> Dog::ClearBag() {
    std::vector<FoundObject> old_bag = std::move(bag_);
    bag_.clear();
    return old_bag;
}

void Dog::AddScore(int score) noexcept {
    score_ += score;
}

int Dog::GetScore() const noexcept {
    return score_;
}

void Dog::UpdateTime(double delta_seconds, double retirement_time) {
    play_time_ += delta_seconds;
    if (speed_.ux == 0.0 && speed_.uy == 0.0) {
        idle_time_ += delta_seconds;
    } else {
        idle_time_ = 0.0;
    }
}

bool Dog::IsRetired(double retirement_time) const noexcept {
    return idle_time_ >= retirement_time;
}

double Dog::GetPlayTime() const noexcept {
    return play_time_;
}

double Dog::GetIdleTime() const noexcept {
    return idle_time_;
}

void Dog::HorizontalMove(Point2D pos, double offset, const Map& map) {
    double allowed_min_x = position_.x;
    double allowed_max_x = position_.x;
    bool found_road = false;

    for (const auto& road : map.GetRoads()) {
        auto [r_min_x, r_max_x] = std::minmax(road.GetStart().x, road.GetEnd().x);
        double r_min = static_cast<double>(r_min_x);
        double r_max = static_cast<double>(r_max_x);

        if (road.IsHorizontal()) {
            double r_y = static_cast<double>(road.GetStart().y);
            if (position_.y >= (r_y - offset - 1e-6) && position_.y <= (r_y + offset + 1e-6)) {
                if (position_.x >= (r_min - offset - 1e-6) && position_.x <= (r_max + offset + 1e-6)) {
                    if (!found_road) {
                        allowed_min_x = r_min;
                        allowed_max_x = r_max;
                        found_road = true;
                    } else {
                        allowed_min_x = std::min(allowed_min_x, r_min);
                        allowed_max_x = std::max(allowed_max_x, r_max);
                    }
                }
            }
        } else {
            double r_x = static_cast<double>(road.GetStart().x);
            auto [r_min_y, r_max_y] = std::minmax(road.GetStart().y, road.GetEnd().y);

            if (position_.y >= (static_cast<double>(r_min_y) - offset - 1e-6) &&
                position_.y <= (static_cast<double>(r_max_y) + offset + 1e-6)) {
                if (position_.x >= (r_x - offset - 1e-6) && position_.x <= (r_x + offset + 1e-6)) {
                    if (!found_road) {
                        allowed_min_x = r_x;
                        allowed_max_x = r_x;
                        found_road = true;
                    }
                }
            }
        }
    }

    double total_min_x = allowed_min_x - offset;
    double total_max_x = allowed_max_x + offset;

    if (pos.x <= total_min_x) {
        position_.x = total_min_x;
        speed_.ux = 0.0;
    } else if (pos.x >= total_max_x) {
        position_.x = total_max_x;
        speed_.ux = 0.0;
    } else {
        position_.x = pos.x;
    }
}

void Dog::VerticalMove(Point2D pos, double offset, const Map& map) {
    double allowed_min_y = position_.y;
    double allowed_max_y = position_.y;
    bool found_road = false;

    for (const auto& road : map.GetRoads()) {
        auto [r_min_y, r_max_y] = std::minmax(road.GetStart().y, road.GetEnd().y);
        double r_min = static_cast<double>(r_min_y);
        double r_max = static_cast<double>(r_max_y);

        if (road.IsVertical()) {
            double r_x = static_cast<double>(road.GetStart().x);
            if (position_.x >= (r_x - offset - 1e-6) && position_.x <= (r_x + offset + 1e-6)) {
                if (position_.y >= (r_min - offset - 1e-6) && position_.y <= (r_max + offset + 1e-6)) {
                    if (!found_road) {
                        allowed_min_y = r_min;
                        allowed_max_y = r_max;
                        found_road = true;
                    } else {
                        allowed_min_y = std::min(allowed_min_y, r_min);
                        allowed_max_y = std::max(allowed_max_y, r_max);
                    }
                }
            }
        } else {
            double r_y = static_cast<double>(road.GetStart().y);
            auto [r_min_x, r_max_x] = std::minmax(road.GetStart().x, road.GetEnd().x);

            if (position_.x >= (static_cast<double>(r_min_x) - offset - 1e-6) &&
                position_.x <= (static_cast<double>(r_max_x) + offset + 1e-6)) {
                if (position_.y >= (r_y - offset - 1e-6) && position_.y <= (r_y + offset + 1e-6)) {
                    if (!found_road) {
                        allowed_min_y = r_y;
                        allowed_max_y = r_y;
                        found_road = true;
                    }
                }
            }
        }
    }

    double total_min_y = allowed_min_y - offset;
    double total_max_y = allowed_max_y + offset;

    if (pos.y <= total_min_y) {
        position_.y = total_min_y;
        speed_.uy = 0.0;
    } else if (pos.y >= total_max_y) {
        position_.y = total_max_y;
        speed_.uy = 0.0;
    } else {
        position_.y = pos.y;
    }
}


void Dog::Move(double time_delta, const Map& map) {
    UpdatePrevPosition();

    if (speed_.ux == 0.0 && speed_.uy == 0.0) {
        return;
    }

    const double offset = 0.4;

    if (speed_.ux != 0.0) {
        Point2D next_pos;
        next_pos.x = position_.x + speed_.ux * time_delta;
        next_pos.y = position_.y;
        HorizontalMove(next_pos, offset, map);
    }

    if (speed_.uy != 0.0) {
        Point2D next_pos;
        next_pos.x = position_.x;
        next_pos.y = position_.y + speed_.uy * time_delta;
        VerticalMove(next_pos, offset, map);
    }
}

//-----------GAME---------------------------------------------------------

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::FindSessionByMapId(const Map::Id& map_id) const noexcept {
    if (auto it = map_id_to_session_.find(map_id); it != map_id_to_session_.end()) {
        return it->second.get();
    }
    return nullptr;
}

GameSession* Game::CreateSession(const Map::Id& map_id) {
    const Map* map = FindMap(map_id);
    if (!map) return nullptr;

    auto session = std::make_unique<GameSession>(map, players_, db_, dog_retirement_time_);
    GameSession* raw_session_ptr = session.get();
    map_id_to_session_.emplace(map_id, std::move(session));
    return raw_session_ptr;
}

void Game::SetDogRetirementTime(double seconds) {
    dog_retirement_time_ = seconds;
}

void Game::Tick(double time_delta) {
    for (auto& [map_id, session] : map_id_to_session_) {
        session->Tick(time_delta);
        HandleTickLostObjects(map_id, *session, time_delta);
    }
}

void Game::HandleTickLostObjects (const Map::Id& map_id, GameSession& session, double time_delta) {
    const Map* map = FindMap(map_id);
    if (!map) return;

    size_t loot_types_count = map->GetLootTypes().size();
    if (loot_types_count == 0) return;

    size_t current_loot_count = session.GetLostObjects().size();
    unsigned looter_count = session.GetPlayersCount();

    auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(time_delta));

    unsigned loot_to_generate = loot_generator_.Generate(delta_ms, current_loot_count, looter_count);

    if (loot_to_generate > 0) {
        std::mt19937 random_engine{std::random_device{}()};

        std::uniform_int_distribution<uint32_t> type_dist(0, static_cast<uint32_t>(loot_types_count - 1));

        for (unsigned i = 0; i < loot_to_generate; ++i) {
            uint32_t random_type = type_dist(random_engine);
            Point2D random_pos = session.GetRandomPointOnRoads();

            session.AddLostObject(random_type, random_pos);
        }
    }
}

//-----------GAME SESSION----------------------------------------------------

std::pair<Dog*, GameSession::DogId> GameSession::AddDog(std::string name) {
    if (dogs_.size() >= MAX_COUNT_DOGS) {
        return {nullptr, 0};
    }

    DogId assigned_id;
    if (!free_ids_.empty()) {
        auto it = free_ids_.begin();
        assigned_id = *it;
        free_ids_.erase(it);
    } else {
        assigned_id = next_id_++;
    }

    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return {nullptr, 0};
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const Road& chosen_road = roads[road_dist(gen)];

    Point2D spawn_point;
    Point start = chosen_road.GetStart();
    Point end = chosen_road.GetEnd();

    if (chosen_road.IsHorizontal()) {
        auto [min_x, max_x] = std::minmax(start.x, end.x);
        std::uniform_real_distribution<double> x_dist(static_cast<double>(min_x), static_cast<double>(max_x));

        spawn_point.x = x_dist(gen);
        spawn_point.y = static_cast<double>(start.y);
    } else {
        auto [min_y, max_y] = std::minmax(start.y, end.y);
        std::uniform_real_distribution<double> y_dist(static_cast<double>(min_y), static_cast<double>(max_y));

        spawn_point.x = static_cast<double>(start.x);
        spawn_point.y = y_dist(gen);
    }

    auto [it, inserted] = dogs_.emplace(assigned_id, Dog(std::move(name), spawn_point));
    it->second.SetBagCapacity(map_->GetBagCapacity());

    return {&(it->second), assigned_id};
}

void GameSession::DeleteDog(DogId id) {
    if (dogs_.erase(id) > 0) {
        if (id < next_id_) {
            free_ids_.insert(id);
        }
    }
}

const std::map<GameSession::DogId, Dog>& GameSession::GetDogs() const noexcept {
    return dogs_;
}

GameSession::DogId GameSession::GetNextDogId() const noexcept {
    return next_id_;
}

 const std::set<GameSession::DogId>& GameSession::GetFreeIds() const noexcept {
    return free_ids_;
}

const Map* GameSession::GetMap() const noexcept {
    return map_;
}
const Map::Id& GameSession::GetMapId() const noexcept {
    return map_->GetId();
}

void GameSession::Tick(double time_delta) {   
    std::vector<DogId> retired_dog_ids;
    std::vector<Dog*> active_dogs;

    for (auto& [id, dog] : dogs_) {
        dog.Move(time_delta, *map_);
        active_dogs.push_back(&dog);
        dog.UpdateTime(time_delta, retirement_time_);

        if (dog.IsRetired(retirement_time_)) {
            retired_dog_ids.push_back(id);
        }
    }

    std::vector<LostObject> loot_vector;
    std::vector<GameSession::LostObjId> loot_ids;
    for (const auto& [id, obj] : lost_objects_) {
        loot_vector.push_back(obj);
        loot_ids.push_back(id);
    }

    GameSessionProvider provider(active_dogs, loot_vector, map_->GetOffices());
    auto events = collision_detector::FindGatherEvents(provider);

    std::unordered_set<GameSession::LostObjId> gathered_loot_ids;

    const auto bag_capacity = map_->GetBagCapacity();

    for (const auto& event : events) {
        Dog* dog = active_dogs[event.gatherer_id];
        const auto& target = provider.GetTargetInfo(event.item_id);

        if (target.type == TargetType::LOOT) {
            GameSession::LostObjId real_loot_id = loot_ids[target.raw_id];

            if (gathered_loot_ids.contains(real_loot_id)) {
                continue;
            }

            if (dog->GetBag().size() < bag_capacity) {
                const auto& loot_obj = loot_vector[target.raw_id];
                dog->AddToBag(FoundObject{loot_obj.id, loot_obj.type});

                gathered_loot_ids.insert(real_loot_id);
            }

        } else if (target.type == TargetType::OFFICE) {
            if (!dog->GetBag().empty()) {
                auto delivered_objects = dog->ClearBag();

                int total_score = 0;
                for (const auto& obj : delivered_objects) {
                    total_score += 10;
                }
                dog->AddScore(total_score);
            }
        }
    }

    for (auto id : gathered_loot_ids) {
        lost_objects_.erase(id);
    }

    for (DogId dog_id : retired_dog_ids) {
        const auto& dog = dogs_.at(dog_id);

        db_.SaveRecord({dog.GetName(), dog.GetScore(), dog.GetPlayTime()});
        players_.RemovePlayer(dog_id, map_->GetId());
        DeleteDog(dog_id);
    }
}

Point2D GameSession::GetRandomPointOnRoads() const {
    if (!map_ || map_->GetRoads().empty()) {
        return {0.0, 0.0};
    }

    const auto& roads = map_->GetRoads();

    thread_local std::mt19937 random_engine{std::random_device{}()};

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const auto& road = roads[road_dist(random_engine)];

    Point2D point;

    if (road.IsHorizontal()) {
        point.y = static_cast<double>(road.GetStart().y);

        auto [min_x, max_x] = std::minmax(road.GetStart().x, road.GetEnd().x);
        std::uniform_int_distribution<int> x_dist(min_x, max_x);

        point.x = static_cast<double>(x_dist(random_engine));
    } else {
        point.x = static_cast<double>(road.GetStart().x);

        auto [min_y, max_y] = std::minmax(road.GetStart().y, road.GetEnd().y);
        std::uniform_int_distribution<int> y_dist(min_y, max_y);

        point.y = static_cast<double>(y_dist(random_engine));
    }

    return point;
}

void GameSession::AddLostObject(uint32_t type, Point2D pos) {
    LostObjId obj_id = lost_object_id_counter_++;
    lost_objects_[obj_id] = LostObject{obj_id, type, pos};
}

const std::unordered_map<GameSession::LostObjId, LostObject>& GameSession::GetLostObjects() const noexcept {
    return lost_objects_;
}

GameSession::LostObjId GameSession::GetNextLootId() const noexcept {
    return lost_object_id_counter_;
}

size_t GameSession::GetLootCount() const noexcept {
    return lost_objects_.size();
}

size_t GameSession::GetPlayersCount() const noexcept {
    return dogs_.size();
}

void GameSession::RestoreDynamicState(DogId next_id, std::set<DogId> free_ids, std::map<DogId, Dog> dogs,
                         std::unordered_map<LostObjId, LostObject> lost_objects, LostObjId next_loot_id) {
    next_id_ = next_id;
    free_ids_ = std::move(free_ids);
    dogs_ = std::move(dogs);
    lost_objects_ = std::move(lost_objects);
    lost_object_id_counter_ = next_loot_id;
}

//-----------PLAYER TOKENS----------------------------------------------------

Player* PlayerTokens::FindPlayerByToken(const Token& token) const {
    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::pair<Player*, PlayerTokens::Token> PlayerTokens::AddPlayer(std::unique_ptr<Player> player) {
    Token token = GenerateToken();
    Player* raw_player_ptr = player.get();
    token_to_player_.emplace(token, std::move(player));
    return {raw_player_ptr, token};
}

void PlayerTokens::EraseToken(const PlayerTokens::Token& token) {
    token_to_player_.erase(token);
}

PlayerTokens::Token PlayerTokens::GenerateToken() {
    uint64_t part1 = generator1_();
    uint64_t part2 = generator2_();

    std::stringstream sstr;
    sstr << std::hex << std::setw(16) << std::setfill('0') << part1;
    sstr << std::setw(16) << part2;

    return Token{sstr.str()};
}

void PlayerTokens::RestoreTokenMapping(const Token& token, std::unique_ptr<Player> player) {
    token_to_player_[token] = std::move(player);
}

//-----------PLAYERS---------------------------------------------------------

std::pair<Player*, PlayerTokens::Token> Players::Add(std::string dog_name, GameSession& session) {
    auto [dog_ptr, dog_id] = session.AddDog(std::move(dog_name));
    if (!dog_ptr) {
        return {nullptr, PlayerTokens::Token{""}};
    }

    auto player = std::make_unique<Player>(&session, dog_ptr, dog_id);
    auto [player_ptr, token] = player_tokens_.AddPlayer(std::move(player));

    PlayerKey key{dog_id, session.GetMap()->GetId()};
    key_to_player_[key] = player_ptr;
    player_to_token_.emplace(player_ptr, token);

    return {player_ptr, token};
}

void Players::RemovePlayer(GameSession::DogId dog_id, const Map::Id& map_id) {
    PlayerKey key{dog_id, map_id};
    auto it = key_to_player_.find(key);
    if (it != key_to_player_.end()) {
        const Player* player_ptr = it->second;

        auto token_it = player_to_token_.find(player_ptr);
        if (token_it != player_to_token_.end()) {
            PlayerTokens::Token token = token_it->second;

            player_tokens_.EraseToken(token);
            player_to_token_.erase(token_it);
        }
        key_to_player_.erase(it);
    }
}

Player* Players::FindByDogAndMapId(GameSession::DogId dog_id, const Map::Id& map_id) const noexcept {
    if (auto it = key_to_player_.find({dog_id, map_id}); it != key_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

Player* Players::FindByToken(const PlayerTokens::Token& token) const noexcept {
    return player_tokens_.FindPlayerByToken(token);
}

const PlayerTokens::Token* Players::FindToken(const Player& player) const noexcept {
    if (auto it = player_to_token_.find(&player); it != player_to_token_.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::unordered_map<const Player*, PlayerTokens::Token>& Players::GetPlayerToTokenMap() const noexcept {
    return player_to_token_;
}

void Players::RestorePlayer(PlayerTokens::Token token, std::unique_ptr<Player> player) {
    Player* raw_player_ptr = player.get();
    GameSession::DogId dog_id = player->GetDogId();
    Map::Id map_id = player->GetSession()->GetMap()->GetId();

    PlayerKey key{dog_id, map_id};

    key_to_player_[key] = raw_player_ptr;

    auto [it, inserted] = player_to_token_.emplace(raw_player_ptr, std::move(token));

    player_tokens_.RestoreTokenMapping(it->second, std::move(player));
}
}  // namespace model
