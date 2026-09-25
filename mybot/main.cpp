#include "helper.hpp"
#include "strategy.hpp"

int main()
{
    auto [ct, game] = unswbc::init();

    while (unswbc::update(ct, game))
    {
        // Redundancy and size-management: Split into multiple dragons to cover more map.
        // Increase the split threshold so we don't lose the 'longest dragon' tiebreaker.
        if (ct.get_length() > 40 && ct.can_split(10)) {
            ct.do_split(10);
        } else {
            auto dir = unswbc::compute_move(ct);
            ct.make_move(dir);
        }
        unswbc::end_turn();
    }
}
