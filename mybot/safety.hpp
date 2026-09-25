#pragma once

#include "helper.hpp"
#include <vector>
#include <queue>
#include <unordered_map>

// Structure to hold precomputed safety info for each direction
struct DirectionSafety {
    unswbc::Direction dir;
    bool wallCollision;      // true if move would hit a wall or kelp
    bool selfCollision;      // true if would collide with own body
    bool enemyCollision;     // true if would collide with any visible enemy body
    bool isReversal;         // true if this direction is the opposite of current heading
    bool headToHeadRisk;     // true if an enemy head could move into this tile next turn
    int reachableSpace;      // flood-fill reachable tiles within vision from this direction
};

// Vision-bounded flood-fill: counts reachable free tiles starting from `start`,
// only expanding within the 7x7 vision window. Returns a large number (e.g. 1000) 
// if the flood fill reaches the edge of the vision, meaning the space is open.
inline int vision_flood_fill(unswbc::Controller& ct, unswbc::Position start) {
    using namespace unswbc;
    static std::unordered_map<Position, bool, PositionHash> visited;
    visited.clear();
    std::queue<Position> q;

    // Check if start tile itself is blocked
    auto const* startTile = ct.get_tile(start);
    if (!startTile) return 0;
    if (startTile->get_dragon()) return 0;

    q.push(start);
    visited[start] = true;
    int count = 0;
    bool reached_edge = false;

    while (!q.empty()) {
        Position cur = q.front();
        q.pop();
        ++count;

        Tile const* cur_tile = ct.get_tile(cur);
        if (!cur_tile) continue;

        for (auto d : Direction::get_direction_list()) {
            if (!cur_tile->get_edge(d).is_passable()) continue;

            Position nxt = cur.add_dir(d);
            if (visited.count(nxt)) continue;

            Tile const* nxt_tile = ct.get_tile(nxt);
            if (!nxt_tile) {
                // Reached outside 7x7 vision! This is an open path.
                reached_edge = true;
                continue; 
            }
            if (nxt_tile->get_dragon()) continue; // occupied

            visited[nxt] = true;
            q.push(nxt);
        }
    }
    
    if (reached_edge) {
        return 1000 + count; // Bonus for open space
    }
    return count;
}

// Compute safety information for all four directions.
inline std::vector<DirectionSafety> compute_safety(unswbc::Controller& ct) {
    std::vector<DirectionSafety> result;
    result.reserve(4);
    auto const here = ct.get_position();
    auto const* hereTile = ct.get_tile(here);
    if (!hereTile) return result;

    unswbc::Direction current = ct.get_dir();
    unswbc::Direction reverse = current.get_opposite();

    // Collect enemy head positions and their predicted next positions.
    struct EnemyHead { unswbc::Position pos; unswbc::Position predicted_next; };
    std::vector<EnemyHead> enemy_heads;
    for (auto const& tile : ct.get_tiles()) {
        if (auto const* dragon = tile.get_dragon()) {
            if (dragon->get_id() != ct.get_id() && dragon->is_head()) {
                enemy_heads.push_back({
                    dragon->get_position(),
                    dragon->get_position().add_dir(dragon->get_dir())
                });
            }
        }
    }

    auto dirs = unswbc::Direction::get_direction_list();
    for (auto d : dirs) {
        DirectionSafety info{d, false, false, false, false, false, 0};

        // Reversals always cause self-collision (head runs into neck).
        if (d == reverse) {
            info.isReversal = true;
            info.selfCollision = true;
        }

        // Wall / kelp collision
        if (!hereTile->get_edge(d).is_passable()) {
            info.wallCollision = true;
        } else if (!info.isReversal) {
            auto target = here.add_dir(d);
            auto const* ahead = ct.get_tile(target);
            if (ahead && ahead->get_dragon()) {
                int other_id = ahead->get_dragon()->get_id();
                if (other_id == ct.get_id())
                    info.selfCollision = true;
                else
                    info.enemyCollision = true;
            }

            // Head-to-head risk: would an enemy head move into this tile next turn?
            for (auto const& eh : enemy_heads) {
                if (eh.predicted_next == target) {
                    info.headToHeadRisk = true;
                    break;
                }
            }

            // Flood-fill reachable space from this direction (within vision).
            if (!info.wallCollision && !info.selfCollision && !info.enemyCollision) {
                info.reachableSpace = vision_flood_fill(ct, target);
            }
        }

        result.push_back(info);
    }
    return result;
}

inline bool is_fully_safe(const DirectionSafety& s) {
    return !s.wallCollision && !s.selfCollision && !s.enemyCollision && !s.isReversal;
}

// Exploration memory: track recent positions per dragon to bias away from already-visited areas.
// This breaks the "wander in loops" pattern when no pearl is visible.
inline std::vector<unswbc::Position>& get_position_history(int dragon_id) {
    static std::unordered_map<int, std::vector<unswbc::Position>> histories;
    return histories[dragon_id];
}

inline void record_position(unswbc::Controller& ct, unswbc::Position pos) {
    auto& history = get_position_history(ct.get_id());
    history.push_back(pos);
    // Keep last 30 positions (enough to detect loops without excessive memory)
    if (history.size() > 30) {
        history.erase(history.begin());
    }
}

// Count how many times a position appears in recent history.
inline int recent_visit_count(unswbc::Controller& ct, unswbc::Position pos) {
    auto& history = get_position_history(ct.get_id());
    int count = 0;
    for (auto const& p : history) {
        if (p == pos) ++count;
    }
    return count;
}

// Choose the best safe move using a quantitative scoring system.
// Score = reachableSpace * 10 - headToHeadRisk * 100 - recentVisits * 30
inline unswbc::Direction safe_move(unswbc::Controller& ct) {
    auto safety = compute_safety(ct);

    // Score each direction. Higher = better.
    struct ScoredDir { unswbc::Direction dir; int score; };
    std::vector<ScoredDir> scored;

    for (auto const& s : safety) {
        // Hard-exclude lethal directions
        if (s.wallCollision || s.selfCollision || s.enemyCollision) continue;

        int score = s.reachableSpace * 10;
        if (s.headToHeadRisk) score -= 100;

        // Penalize directions that lead to recently visited tiles (anti-loop)
        unswbc::Position next_pos = ct.get_position().add_dir(s.dir);
        int visits = recent_visit_count(ct, next_pos);
        score -= visits * 30;

        scored.push_back({s.dir, score});
    }

    // Pick the highest-scoring direction.
    if (!scored.empty()) {
        auto best = scored.begin();
        for (auto it = scored.begin(); it != scored.end(); ++it) {
            if (it->score > best->score) best = it;
        }
        return best->dir;
    }

    // Emergency: all safe directions are blocked. Accept enemy collision as last resort.
    for (auto const& s : safety) {
        if (!s.wallCollision && !s.selfCollision) return s.dir;
    }

    // Ultra-emergency: accept anything that isn't a wall.
    for (auto const& s : safety) {
        if (!s.wallCollision) return s.dir;
    }

    return ct.get_dir();
}
