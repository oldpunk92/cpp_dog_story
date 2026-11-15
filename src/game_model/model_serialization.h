#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/set.hpp>

#include "model.h"

namespace boost::serialization {

template <typename Archive>
void serialize(Archive& ar, geom::Point2D& point, [[maybe_unused]] const unsigned int version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, model::Point2D& point, [[maybe_unused]] const unsigned int version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, model::Speed2D& speed, [[maybe_unused]] const unsigned int version) {
    ar & speed.ux;
    ar & speed.uy;
}

template <typename Archive>
void serialize(Archive& ar, model::FoundObject& obj, [[maybe_unused]] const unsigned int version) {
    ar & obj.id;
    ar & obj.type;
}

template <typename Archive>
void serialize(Archive& ar, model::LostObject& obj, [[maybe_unused]] const unsigned int version) {
    ar & obj.id;
    ar & obj.type;
    ar & obj.pos;
}

template <typename Archive>
void serialize(Archive& ar, model::Direction& direction, [[maybe_unused]] const unsigned int version) {
    int val = static_cast<int>(direction);
    ar & val;
    if (Archive::is_loading::value) {
        direction = static_cast<model::Direction>(val);
    }
}

} // namespace boost::serialization

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : name_(dog.GetName())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , bag_capacity_(dog.GetBagCapacity())
        , direction_(dog.GetDirection())
        , bag_(dog.GetBag())
        , score_(dog.GetScore()) {}

    model::Dog Restore() const {
        model::Dog dog{name_, pos_};
        dog.SetBagCapacity(bag_capacity_);
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_) {
            dog.AddToBag(item);
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & name_;
        ar & pos_;
        ar & speed_;
        ar & bag_capacity_;
        ar & direction_;
        ar & bag_;
        ar & score_;
    }

private:
    std::string name_;
    model::Point2D pos_;
    model::Speed2D speed_;
    std::uint8_t bag_capacity_ = 0;
    model::Direction direction_ = model::Direction::NORTH;
    std::vector<model::FoundObject> bag_;
    int score_ = 0;
};

struct GameStateRepr {
    struct SessionSnapshot {
        uint32_t next_dog_id;
        std::set<uint32_t> free_dog_ids;
        std::map<uint32_t, DogRepr> dogs;
        std::unordered_map<uint32_t, model::LostObject> lost_objects;
        uint32_t next_loot_id;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
            ar & next_dog_id;
            ar & free_dog_ids;
            ar & dogs;
            ar & lost_objects;
            ar & next_loot_id;
        }
    };

    struct PlayerSnapshot {
        std::string map_id;
        uint32_t dog_id;
        std::string token;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
            ar & map_id;
            ar & dog_id;
            ar & token;
        }
    };

    std::unordered_map<std::string, SessionSnapshot> sessions;
    std::vector<PlayerSnapshot> players;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & sessions;
        ar & players;
    }
};

} // namespace serialization
