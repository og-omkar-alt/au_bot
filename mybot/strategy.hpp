#pragma once

#include "helper.hpp"
#include "safety.hpp"
#include "pearl.hpp"

namespace unswbc {

// Compute the final move: pearl-seeking first, exploration-biased survival second.
inline Direction compute_move(Controller& ct) {
    // Record current position for anti-loop exploration bias.
    record_position(ct, ct.get_position());

    // Precompute safety for all directions.
    auto safety = compute_safety(ct);

    int max_reachable = 0;
    for (auto const& s : safety) {
        if (is_fully_safe(s) && s.reachableSpace > max_reachable) {
            max_reachable = s.reachableSpace;
        }
    }

    // 1. Pearls are the entire scoring mechanism — always seek one first.
    if (auto pearl_dir = bfs_to_nearest_pearl(ct)) {
        for (auto const& s : safety) {
            if (s.dir == *pearl_dir && is_fully_safe(s) && !s.headToHeadRisk) {
                // If there's an open path available (>1000), but the pearl path is closed (<1000)
                // AND doesn't have enough space to fit our dragon safely, it's a trap.
                bool is_trap = (max_reachable > 1000 && s.reachableSpace < 1000 && s.reachableSpace < ct.get_length() + 3);
                if (!is_trap) {
                    return *pearl_dir;
                }
            }
        }
        // Pearl direction is dangerous (head-to-head risk or dead-end trap).
        // Fall through to safe_move which handles the risk quantitatively.
    }

    // 2. No safe pearl path visible — use scored safe_move with exploration bias.
    return safe_move(ct);
}

} // namespace unswbc
