#include "helper.hpp"
#include "strategy.hpp"

int main()
{
    auto [ct, game] = unswbc::init();

    while (unswbc::update(ct, game))
    {
        // Redundancy and size-management: Split into multiple dragons to cover more map.
        // Increase the split threshold so we don't lose the 'longest dragon' tiebreaker.
        // However, if we are in an enclosed space (max_space < 1000) and we are about to outgrow it, split!
        int max_space = 0;
        for (auto const& s : unswbc::compute_safety(ct)) {
            if (unswbc::is_fully_safe(s) && s.reachableSpace > max_space) {
                max_space = s.reachableSpace;
            }
        }
        
        bool emergency_split = (max_space < 1000 && max_space > 0 && ct.get_length() > max_space - 4);

        if ((ct.get_length() > 40 || emergency_split) && ct.can_split(4)) {
            int split_size = emergency_split ? 4 : 10;
            ct.do_split(split_size);
        } else {
            auto dir = unswbc::compute_move(ct);
            ct.make_move(dir);
        }
        unswbc::end_turn();
    }
}
