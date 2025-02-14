#define _USE_MATH_DEFINES

#include <catch2/catch_all.hpp> 
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include "../src/collision_detector.h"

using Catch::Matchers::Contains;
using Catch::Matchers::WithinRel;
using namespace std::literals;
using namespace collision_detector;
using namespace geom;

constexpr double EPSILON = 1e-9;

namespace collision_detector {
class ItemGathererProviderTester: public ItemGathererProvider {
public:
        
    size_t ItemsCount() const override {
        return items_.size();
    };

    Item GetItem(size_t idx) const override {
        return items_[idx];
    };

    void AddItem(Item item) {
        items_.push_back(std::move(item));
    };

    size_t GatherersCount() const override { 
        return gatherers_.size();
    };

    Gatherer GetGatherer(size_t idx) const override { 
        return gatherers_[idx];
    };

    void AddGatherer(Gatherer gatherer) {
        gatherers_.push_back(std::move(gatherer));
    };
private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

bool operator==(const GatheringEvent& lhs, const GatheringEvent& rhs) {
    return lhs.item_id == rhs.item_id &&
           lhs.gatherer_id == rhs.gatherer_id &&            
           Catch::Approx(lhs.sq_distance).epsilon(EPSILON) == rhs.sq_distance &&
           Catch::Approx(lhs.time).epsilon(EPSILON) == rhs.time;
}

bool operator<(const GatheringEvent& lhs, const GatheringEvent& rhs) {
    return lhs.time < rhs.time;  
}

bool operator<=(const GatheringEvent& lhs, const GatheringEvent& rhs) {
    return lhs.time < rhs.time || 
           Catch::Approx(lhs.time).epsilon(EPSILON) == rhs.time;  
}
}// namspace collision_detector 


namespace Catch {
template<>
struct StringMaker<collision_detector::GatheringEvent> {
  static std::string convert(collision_detector::GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

      return tmp.str();
  }
};
}  // namespace Catch 


template <typename Range>
struct IsPermutationMatcher : Catch::Matchers::MatcherGenericBase {
    IsPermutationMatcher(Range range)
        : range_{std::move(range)} {
        std::sort(std::begin(range_), std::end(range_));
    }
    IsPermutationMatcher(IsPermutationMatcher&&) = default;

    template <typename OtherRange>
    bool match(OtherRange other) const {
        using std::begin;
        using std::end;

        std::sort(begin(other), end(other));
        return std::equal(begin(range_), end(range_), begin(other), end(other));
    }

    std::string describe() const override {
        return "Is permutation of: "s + Catch::rangeToString(range_);
    }

private:
    Range range_;
}; 

TEST_CASE("Gatherer should collect an item that is on their way", "[collision_detector]") {
    //initialize provider 
    Point2D item0_pos = {2.0, 0.8};
    Point2D beg_pos = {0., 1.};
    Point2D end_pos = {3., 1.};  
    Item item0(item0_pos, 0.5);
    Gatherer gatherer1(beg_pos, end_pos, 0.5);
    ItemGathererProviderTester provider;
    provider.AddItem(item0);
    provider.AddGatherer(gatherer1);

    //call FindGatherEvents to get events to test
    std::vector<GatheringEvent> events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);

    //compute expected results
    CollectionResult result = TryCollectPoint(beg_pos, end_pos, item0_pos);

    GatheringEvent event = events[0];
    CHECK(event.gatherer_id == 0);
    CHECK(event.item_id == 0);
    CHECK_THAT(event.sq_distance, WithinRel(result.sq_distance, EPSILON));
    CHECK_THAT(event.time, WithinRel(result.proj_ratio, EPSILON));
} 

TEST_CASE("Gatherer should not collect an item that is not on their way", "[collision_detector]") {
    //initialize provider 
    Point2D item0_pos = {6.0, 6.0}; //The projection of the object does not lie on the movement vector of the gatherer.
    Point2D item1_pos = {1.0, 8.0}; //The projection of the item lies on the movement vector of the gatherer.
    Point2D beg_pos = {0., 1.};
    Point2D end_pos = {3., 1.}; 
    Item item0(item0_pos, 0.5);
    Item item1(item1_pos, 0.7);    
    Gatherer gatherer1(beg_pos, end_pos, 0.5);
    ItemGathererProviderTester provider;
    provider.AddItem(item0);
    provider.AddItem(item1);
    provider.AddGatherer(gatherer1);

    //call FindGatherEvents to get events to test
    std::vector<GatheringEvent> events = FindGatherEvents(provider);
    REQUIRE(events.size() == 0);
} 

TEST_CASE("Events should be sorted by time", "[collision_detector]") {
    //initialize provider 
    Point2D item0_pos = {2.0, 0.8}; // item on the way of gatherer 0
    Point2D item1_pos = {6.0, 6.0}; // item on the way of gatherer 1
    Point2D item2_pos = {1.0, 8.0}; // item out of the trajectory of any gatherers
    Point2D beg_pos_gatherer0 = {0., 1.};
    Point2D end_pos_gatherer0 = {3., 1.}; 
    Point2D beg_pos_gatherer1 = {0., 6.2};
    Point2D end_pos_gatherer1 = {7.0, 6.2};
    //coordinates are the same as for gatherer1 to check if item can be gathered multiple times
    Point2D beg_pos_gatherer2 = {0., 6.2}; 
    Point2D end_pos_gatherer2 = {7.0, 6.2};
    Item item0(item0_pos, 0.5); 
    Item item1(item1_pos, 0.7);    
    Item item2(item2_pos, 0.9);
    Gatherer gatherer0(beg_pos_gatherer0, end_pos_gatherer0, 0.5);
    Gatherer gatherer1(beg_pos_gatherer2, end_pos_gatherer2, 0.5);
    Gatherer gatherer2(beg_pos_gatherer2, end_pos_gatherer2, 0.5);

    ItemGathererProviderTester provider;
    provider.AddItem(item0); //id = 0
    provider.AddItem(item1); //id = 1
    provider.AddItem(item2); //id = 2
    provider.AddGatherer(gatherer0); //id = 0
    provider.AddGatherer(gatherer1); //id = 1
    provider.AddGatherer(gatherer2); //id = 2

    //call FindGatherEvents to get events to test
    std::vector<GatheringEvent> events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);

    //compute expected results
    CollectionResult result0 = TryCollectPoint(beg_pos_gatherer0, end_pos_gatherer0, item0_pos);
    CollectionResult result1 = TryCollectPoint(beg_pos_gatherer2, end_pos_gatherer2, item1_pos);
    CollectionResult result2 = TryCollectPoint(beg_pos_gatherer2, end_pos_gatherer2, item1_pos);
    GatheringEvent event0 = {0, 0, result0.sq_distance, result0.proj_ratio};
    GatheringEvent event1 = {1, 1, result1.sq_distance, result1.proj_ratio};
    GatheringEvent event2 = {1, 2, result2.sq_distance, result2.proj_ratio};
    std::vector<GatheringEvent> expected_events{event0, event1, event2};

    // Check if events match the expected permutation
    CHECK_THAT(events, IsPermutationMatcher<std::vector<GatheringEvent>>(expected_events));
    // Check if events are sorted in non-decreasing order by time
    for (size_t i = 1; i < events.size(); i++) {
        CHECK(events[i-1] <= events[i]);
    }
} 