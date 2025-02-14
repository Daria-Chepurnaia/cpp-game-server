#include "collision_detector.h"
#include <cassert>
#include <algorithm>

using namespace std;

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;
    int gatherers_count = provider.GatherersCount();
    int items_count = provider.ItemsCount();
    for (uint i = 0; i < gatherers_count; i++) {
        Gatherer gatherer = provider.GetGatherer(i);
        if (gatherer.start_pos.x == gatherer.end_pos.x &&
            gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }
        for (uint j = 0; j < items_count; j++) {
            Item item = provider.GetItem(j);
            auto result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
            if (result.IsCollected(item.width + gatherer.width)) {
                GatheringEvent event; 
                event.gatherer_id = i;
                event.item_id = j;
                event.sq_distance = result.sq_distance;
                event.time = result.proj_ratio;
                events.push_back(event);
            }
        }
    }
    std::sort(events.begin(), events.end(), [](GatheringEvent lhs, GatheringEvent rhs) {
        return lhs.time < rhs.time;
    });
    return events;
}
}  // namespace collision_detector