#pragma once

#include "helper.hpp"
#include <queue>
#include <optional>
#include <unordered_map>

// BFS within the current 7x7 vision window to the nearest tile with a pearl.
// Returns the first-step direction to take this turn, or nullopt if no
// pearl is reachable within vision.
inline std::optional<unswbc::Direction> bfs_to_nearest_pearl(unswbc::Controller& ct) {
    using namespace unswbc;

    Position const start = ct.get_position();

    // Store first-step direction as char (Direction has no default ctor).
    static std::unordered_map<Position, char, PositionHash> first_step;
    static std::unordered_map<Position, bool, PositionHash> visited;
    first_step.clear();
    visited.clear();

    std::queue<Position> q;
    q.push(start);
    visited[start] = true;

    while (!q.empty()) {
        Position cur = q.front();
        q.pop();

        Tile const* cur_tile = ct.get_tile(cur);
        if (!cur_tile) continue;

        for (auto d : Direction::get_direction_list()) {
            if (!cur_tile->get_edge(d).is_passable()) continue; // wall/kelp

            Position nxt = cur.add_dir(d);
            if (visited.count(nxt)) continue;

            Tile const* nxt_tile = ct.get_tile(nxt);
            if (!nxt_tile) continue; // outside 7x7 vision window
            if (nxt_tile->get_dragon()) continue; // occupied — don't path through bodies

            visited[nxt] = true;
            first_step[nxt] = (cur == start) ? d.value : first_step[cur];

            if (nxt_tile->has_pearl()) {
                return Direction(first_step[nxt]);
            }
            q.push(nxt);
        }
    }
    return std::nullopt; // no reachable pearl currently visible
}

// BFS within the current 7x7 vision window to the nearest portal.
inline std::optional<unswbc::Direction> bfs_to_nearest_portal(unswbc::Controller& ct) {
    using namespace unswbc;

    Position const start = ct.get_position();

    static std::unordered_map<Position, char, PositionHash> first_step;
    static std::unordered_map<Position, bool, PositionHash> visited;
    first_step.clear();
    visited.clear();

    std::queue<Position> q;
    q.push(start);
    visited[start] = true;

    while (!q.empty()) {
        Position cur = q.front();
        q.pop();

        Tile const* cur_tile = ct.get_tile(cur);
        if (!cur_tile) continue;

        for (auto d : Direction::get_direction_list()) {
            if (!cur_tile->get_edge(d).is_passable()) continue;

            if (cur_tile->get_edge(d).is_portal()) {
                return (cur == start) ? std::make_optional(d) : std::make_optional(Direction(first_step[cur]));
            }

            Position nxt = cur.add_dir(d);
            if (visited.count(nxt)) continue;

            Tile const* nxt_tile = ct.get_tile(nxt);
            if (!nxt_tile) continue; 
            if (nxt_tile->get_dragon()) continue; 

            visited[nxt] = true;
            first_step[nxt] = (cur == start) ? d.value : first_step[cur];
            q.push(nxt);
        }
    }
    return std::nullopt; 
}
