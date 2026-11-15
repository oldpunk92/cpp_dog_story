#pragma once

#include <string>
#include <unordered_set>
#include <set>
#include <limits>
#include <unordered_map>
#include <map>
#include <vector>
#include <cstdint>
#include <memory>
#include <random>

#include "tagged.h"
#include "loot_generator.h"
#include "collision_detector.h"

namespace detail {
struct TokenTag {};
}  // namespace detail

namespace model {

using Dimension = int;
using Coord = Dimension;
using DimensionDouble = double;

//-----------------------------
//        STRUCTs
//-----------------------------

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Point2D {
    DimensionDouble x, y;

    bool operator ==(const Point2D& other_point) const {
        return x == other_point.x && y == other_point.y;
    }
};

struct Speed2D {
    DimensionDouble ux = 0.0;
    DimensionDouble uy = 0.0;

    bool operator ==(const Speed2D& other_speed) const {
        return ux == other_speed.ux && uy == other_speed.uy;
    }
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct LootType {
    int value = 0;
};

struct LostObject {
    uint32_t id;
    uint32_t type;
    Point2D pos;
};

struct FoundObject {
    uint32_t id;
    uint32_t type;

    bool operator ==(const FoundObject& other_obj) const {
        return id == other_obj.id && type == other_obj.type;
    }
};

enum class TargetType {
    LOOT,
    OFFICE
};

struct CollisionTarget {
    TargetType type;
    uint32_t raw_id;
};

struct PlayerRecord {
    std::string name;
    int score;
    double play_time;
};

//----------------------------------------------------------
//        CLASSes
//----------------------------------------------------------

class IRecordRepository {
public:
    virtual ~IRecordRepository() = default;
    virtual void SaveRecord(const PlayerRecord& record) = 0;
    virtual std::vector<PlayerRecord> GetLeaderboard(int start, int max_items) = 0;
};

//------------ROAD------------------------------------------

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

//-----------BUILDING------------------------------------------

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};


//-----------OFFICE--------------------------------------------

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

//-----------MAP----------------------------------------------

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {}

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);
    const std::vector<Road>& GetHorizontalRoadsByY(int y) const;
    const std::vector<Road>& GetVerticalRoadsByX(int x) const;

    void AddBuilding(const Building& building);
    void AddOffice(Office office);

    void SetDogSpeed(DimensionDouble speed) noexcept;
    DimensionDouble GetDogSpeed() const noexcept;

    void SetBagCapacity(std::uint8_t capacity) noexcept;
    std::uint8_t GetBagCapacity() const noexcept;

    void AddLootType(LootType loot);
    const std::vector<LootType> GetLootTypes() const noexcept;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;

    Buildings buildings_;

    Roads roads_;
    std::unordered_map<int, std::vector<Road>> horizontal_roads_by_y_;
    std::unordered_map<int, std::vector<Road>> vertical_roads_by_x_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    DimensionDouble dog_speed_ = 1.0;
    std::uint8_t bag_capacity_ = 3;
    std::vector<LootType> loot_types_;
};

//-----------DOG-------------------------------------------------

class Dog {
public:
    explicit Dog(std::string name, Point2D position)
        : name_(std::move(name))
        , position_(position)
        , prev_position_(position)
        , speed_({0.0, 0.0})
        , direction_(Direction::NORTH) {}

private:
    void HorizontalMove(Point2D pos, double offset, const Map& map);
    void VerticalMove(Point2D pos, double offset, const Map& map);

public:
    const std::string& GetName() const noexcept;

    const Point2D& GetPosition() const noexcept;
    void SetPosition(Point2D pos) noexcept;

    const Speed2D& GetSpeed() const noexcept;
    void SetSpeed(Speed2D speed) noexcept;

    Direction GetDirection() const noexcept;
    void SetDirection(Direction dir) noexcept;

    std::uint8_t GetBagCapacity() const noexcept;
    void SetBagCapacity(std::uint8_t capacity) noexcept;

    const Point2D& GetPrevPosition() const noexcept;
    void UpdatePrevPosition() noexcept;

    const std::vector<FoundObject>& GetBag() const noexcept;
    bool IsBagFull() const noexcept;
    void AddToBag(FoundObject obj);
    std::vector<FoundObject> ClearBag();

    int GetScore() const noexcept;
    void AddScore(int score) noexcept;

    void UpdateTime(double delta_seconds, double retirement_time);
    bool IsRetired(double retirement_time) const noexcept;
    double GetPlayTime() const noexcept;
    double GetIdleTime() const noexcept;

    void ResetIdleTime() noexcept {
        idle_time_ = 0.0;
    }

    void Move(double time_delta, const Map& map);

private:
    std::string name_;

    Point2D position_;
    Point2D prev_position_;

    Speed2D speed_;
    std::uint8_t bag_capacity_ = 3;
    Direction direction_;

    std::vector<FoundObject> bag_;
    int score_ = 0;

    double play_time_ = 0.0;
    double idle_time_ = 0.0;
};

//-----------GAME SESSION------------------------------------------

class Players;

class GameSession {
    static constexpr std::uint32_t MAX_COUNT_DOGS = std::numeric_limits<std::uint32_t>::max();

public:
    using DogId = std::uint32_t;
    using LostObjId = std::uint32_t;

    explicit GameSession(const Map* map, Players& players, IRecordRepository& db, double retirement_time)
        : map_(map)
        , players_(players)
        , db_(db)
        , retirement_time_(retirement_time)
        , next_id_(0) {}

    std::pair<Dog*, DogId> AddDog(std::string name);
    void DeleteDog(DogId id);
    const std::map<DogId, Dog>& GetDogs() const noexcept;
    DogId GetNextDogId() const noexcept;
    const std::set<DogId>& GetFreeIds() const noexcept;

    const Map* GetMap() const noexcept;
    const Map::Id& GetMapId() const noexcept;

    Point2D GetRandomPointOnRoads() const;

    void AddLostObject(uint32_t type, Point2D pos);
    const std::unordered_map<LostObjId, LostObject>& GetLostObjects() const noexcept;
    LostObjId GetNextLootId() const noexcept;


    size_t GetLootCount() const noexcept;
    size_t GetPlayersCount() const noexcept;

    void Tick(double time_delta);

    void RestoreDynamicState(DogId next_id,
                             std::set<DogId> free_ids,
                             std::map<DogId, Dog> dogs,
                             std::unordered_map<LostObjId,
                             LostObject> lost_objects,
                             LostObjId next_loot_id);

private:
    const Map* map_;
    DogId next_id_;
    std::set<DogId> free_ids_;
    std::map<DogId, Dog> dogs_;

    std::unordered_map<LostObjId, LostObject> lost_objects_;
    LostObjId lost_object_id_counter_ = 0;

    Players& players_;
    IRecordRepository& db_;
    double retirement_time_;
};

//-----------GAME SESSION PROVIDER---------------------------------

class GameSessionProvider : public collision_detector::ItemGathererProvider {
    const double LOOT_WIDTH = 0.0;
    const double OFFICE_WIDTH = 0.5;
    const double GATHERER_WIDTH = 0.6;

public:
    GameSessionProvider(const std::vector<Dog*>& dogs,
                        const std::vector<LostObject>& loot,
                        const Map::Offices& offices)
        : dogs_(dogs)
        , loot_(loot)
        , offices_(offices) {
        InitTargets();
    }

    size_t ItemsCount() const override {
        return targets_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        const auto& target = targets_[idx];
        if (target.type == TargetType::LOOT) {
            return { {loot_[target.raw_id].pos.x, loot_[target.raw_id].pos.y}
                     , LOOT_WIDTH };
        } else {
            auto office_pos = offices_[target.raw_id].GetPosition();
            return { {static_cast<double>(office_pos.x), static_cast<double>(office_pos.y)}, OFFICE_WIDTH };
        }
    }

    size_t GatherersCount() const override {
        return dogs_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return { { dogs_[idx]->GetPrevPosition().x, dogs_[idx]->GetPrevPosition().y },
                 { dogs_[idx]->GetPosition().x, dogs_[idx]->GetPosition().y }
                 , GATHERER_WIDTH };
    }

    const CollisionTarget& GetTargetInfo(size_t idx) const {
        return targets_[idx];
    }

private:
    void InitTargets() {
        for (size_t i = 0; i < loot_.size(); ++i) {
            targets_.push_back({TargetType::LOOT, static_cast<uint32_t>(i)});
        }
        for (size_t i = 0; i < offices_.size(); ++i) {
            targets_.push_back({TargetType::OFFICE, static_cast<uint32_t>(i)});
        }
    }

    const std::vector<Dog*>& dogs_;
    const std::vector<LostObject>& loot_;
    const Map::Offices& offices_;
    std::vector<CollisionTarget> targets_;
};


//-----------GAME---------------------------------------------------

class Game {
public:
    using Maps = std::vector<Map>;

    Game(loot_gen::LootGenerator::TimeInterval period, double probability, Players& players, IRecordRepository& db)
        : loot_generator_(period, probability)
        , players_(players)
        , db_(db) {}

private :
    void HandleTickLostObjects (const Map::Id& map_id, GameSession& session, double time_delta);

public:
    void AddMap(Map map);
    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;

    GameSession* FindSessionByMapId(const Map::Id& map_id) const noexcept;
    GameSession* CreateSession(const Map::Id& map_id);
    void SetDogRetirementTime(double seconds);

    void Tick(double time_delta);

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
    std::unordered_map<Map::Id, std::unique_ptr<GameSession>, MapIdHasher> map_id_to_session_;
    loot_gen::LootGenerator loot_generator_;

    Players& players_;
    IRecordRepository& db_;
    double dog_retirement_time_ = 60.0;
};

//-----------PLAYER---------------------------------------------------

class Player {
public:
    explicit Player(GameSession* session, Dog* dog, GameSession::DogId dog_id)
        : session_(session)
        , dog_(dog)
        , id_(dog_id) {}

    GameSession* GetSession() const noexcept {
        return session_;
    }

    const Dog* GetDog() const noexcept {
        return dog_;
    }

    Dog* GetDog() noexcept {
        return dog_;
    }

    GameSession::DogId GetDogId() const noexcept {
        return id_;
    }

private:
    GameSession* session_;
    Dog* dog_;
    GameSession::DogId id_;
};

//-----------PLAYER TOKENS--------------------------------------------

class PlayerTokens {
public:
    using Token = util::Tagged<std::string, detail::TokenTag>;

    Player* FindPlayerByToken(const Token& token) const;
    std::pair<Player*, Token> AddPlayer(std::unique_ptr<Player> player);
    void EraseToken(const Token& token);

    void RestoreTokenMapping(const Token& token, std::unique_ptr<Player> player);

private:
    Token GenerateToken();

private:
    using TokenHasher = util::TaggedHasher<Token>;

    std::unordered_map<Token, std::unique_ptr<Player>, TokenHasher> token_to_player_;

    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

//-----------PLAYERS---------------------------------------------------

class Players {
public:
    explicit Players(PlayerTokens& player_tokens)
        : player_tokens_(player_tokens) {}

    std::pair<Player*, PlayerTokens::Token> Add(std::string dog_name, GameSession& session);
    void RemovePlayer(GameSession::DogId dog_id, const Map::Id& map_id);
    Player* FindByDogAndMapId(GameSession::DogId dog_id, const Map::Id& map_id) const noexcept;
    Player* FindByToken(const PlayerTokens::Token& token) const noexcept;
    const PlayerTokens::Token* FindToken(const Player& player) const noexcept;
    const std::unordered_map<const Player*, PlayerTokens::Token>& GetPlayerToTokenMap() const noexcept;

    void RestorePlayer(PlayerTokens::Token token, std::unique_ptr<Player> player);

private:
    PlayerTokens& player_tokens_;

    struct PlayerKey {
        GameSession::DogId dog_id;
        Map::Id map_id;

        PlayerKey(GameSession::DogId id, Map::Id map)
            : dog_id(id)
            , map_id(std::move(map)) {}

        bool operator==(const PlayerKey& other) const {
            return dog_id == other.dog_id && map_id == other.map_id;
        }
    };

    struct PlayerKeyHasher {
        std::size_t operator()(const PlayerKey& key) const {
            std::size_t h1 = std::hash<GameSession::DogId>{}(key.dog_id);
            std::size_t h2 = util::TaggedHasher<Map::Id>{}(key.map_id);
            return h1 ^ (h2 << 1);
        }
    };

    std::unordered_map<PlayerKey, Player*, PlayerKeyHasher> key_to_player_;
    std::unordered_map<const Player*, PlayerTokens::Token> player_to_token_;
};

}  // namespace model
