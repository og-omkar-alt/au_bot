#include "helper.hpp"
#include "strategy.hpp"

int main()
{
    auto [ct, game] = unswbc::init();

    while (unswbc::update(ct, game))
    {
        // Redundancy and size-management: Split into multiple dragons to cover more map.
        // If a dragon gets too long (e.g. > 10), it becomes harder to maneuver.
        if (ct.get_length() > 10 && ct.can_split(4)) {
            ct.do_split(4);
        } else {
            auto dir = unswbc::compute_move(ct);
            ct.make_move(dir);
        }
        unswbc::end_turn();
    }
}
