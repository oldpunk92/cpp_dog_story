#define _USE_MATH_DEFINES

#include "../src/game_model/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

using namespace collision_detector;
using namespace geom;

class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    void AddItem(const Item& item) {
        items_.push_back(item);
    }

    void AddGatherer(const Gatherer& gatherer) {
        gatherers_.push_back(gatherer);
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

TEST_CASE("Collision Detector: Empty provider") {
    VectorItemGathererProvider provider;
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Collision Detector: Multiple items and gatherers detection") {
    VectorItemGathererProvider provider;

    provider.AddItem(Item{{5.0, 0.0}, 0.5});
    provider.AddItem(Item{{5.0, 5.0}, 0.5});

    provider.AddGatherer(Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);

    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-5));
}

TEST_CASE("Collision Detector: Sorting events by time") {
    VectorItemGathererProvider provider;

    provider.AddItem(Item{{8.0, 0.0}, 0.1});
    provider.AddItem(Item{{2.0, 0.0}, 0.1});

    provider.AddGatherer(Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);

    CHECK(events[0].item_id == 1);
    CHECK(events[1].item_id == 0);
    CHECK(events[0].time < events[1].time);
}

TEST_CASE("Collision Detector: Touch on the edge of radius") {
    VectorItemGathererProvider provider;

    provider.AddItem(Item{{5.0, 1.0}, 0.4});
    provider.AddGatherer(Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.6});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
}

TEST_CASE("Collision Detector: Gatherer misses items outside its path segment") {
    VectorItemGathererProvider provider;

    provider.AddItem(Item{{-2.0, 0.0}, 0.5});
    provider.AddItem(Item{{12.0, 0.0}, 0.5});

    provider.AddGatherer(Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.5});

    auto events = FindGatherEvents(provider);

    CHECK(events.empty());
}

TEST_CASE("Collision Detector: Multi-gatherer and multi-item complex interaction") {
    VectorItemGathererProvider provider;

    provider.AddItem(Item{{2.0, 0.0}, 0.2});
    provider.AddItem(Item{{3.0, 3.0}, 0.2});

    provider.AddGatherer(Gatherer{{0.0, 0.0}, {5.0, 0.0}, 0.5});
    provider.AddGatherer(Gatherer{{0.0, 3.0}, {5.0, 3.0}, 0.5});

    auto events = FindGatherEvents(provider);

    REQUIRE(events.size() == 2);

    for (const auto& event : events) {
        if (event.gatherer_id == 0) {
            CHECK(event.item_id == 0);
        } else if (event.gatherer_id == 1) {
            CHECK(event.item_id == 1);
        }
    }
}
