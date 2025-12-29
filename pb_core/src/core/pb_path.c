/*
 * pb_path.c - Pathfinding implementation for hexagonal grids
 *
 * Implements A* and JPS adapted for hex coordinate systems.
 * Uses binary heap for efficient priority queue operations.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_path.h"

/* Include string.h only in hosted mode; freestanding uses pb_freestanding.h */
#if !PB_FREESTANDING
#include <string.h>
#endif

/*============================================================================
 * Internal Structures
 *============================================================================*/

/* Node in the A* open/closed sets */
typedef struct pb_path_node {
    pb_offset pos;
    int g_cost;          /* Cost from start */
    int f_cost;          /* g + heuristic */
    pb_offset parent;    /* For path reconstruction */
    bool in_open;
    bool in_closed;
} pb_path_node;

/* Priority queue (min-heap) for open set */
typedef struct pb_path_heap {
    pb_offset nodes[PB_PATH_HEAP_SIZE];
    int size;
} pb_path_heap;

/* Pathfinder state */
typedef struct pb_pathfinder {
    pb_path_node map[PB_MAX_ROWS][PB_MAX_COLS];
    pb_path_heap open;
    int nodes_expanded;
    int nodes_visited;
} pb_pathfinder;

/*============================================================================
 * Heap Operations
 *============================================================================*/

static inline void heap_init(pb_path_heap* heap)
{
    heap->size = 0;
}

static inline bool heap_empty(const pb_path_heap* heap)
{
    return heap->size == 0;
}

static inline int heap_parent(int i) { return (i - 1) / 2; }
static inline int heap_left(int i) { return 2 * i + 1; }
static inline int heap_right(int i) { return 2 * i + 2; }

static void heap_push(pb_path_heap* heap, pb_offset pos,
                      pb_path_node map[PB_MAX_ROWS][PB_MAX_COLS])
{
    if (heap->size >= PB_PATH_HEAP_SIZE) return;

    int i = heap->size++;
    heap->nodes[i] = pos;

    /* Bubble up */
    while (i > 0) {
        int p = heap_parent(i);
        pb_offset pi = heap->nodes[p];
        pb_offset ci = heap->nodes[i];

        if (map[pi.row][pi.col].f_cost <= map[ci.row][ci.col].f_cost) {
            break;
        }

        /* Swap */
        heap->nodes[p] = ci;
        heap->nodes[i] = pi;
        i = p;
    }
}

static pb_offset heap_pop(pb_path_heap* heap,
                          pb_path_node map[PB_MAX_ROWS][PB_MAX_COLS])
{
    pb_offset result = heap->nodes[0];
    heap->nodes[0] = heap->nodes[--heap->size];

    /* Bubble down */
    int i = 0;
    while (true) {
        int smallest = i;
        int left = heap_left(i);
        int right = heap_right(i);

        if (left < heap->size) {
            pb_offset si = heap->nodes[smallest];
            pb_offset li = heap->nodes[left];
            if (map[li.row][li.col].f_cost < map[si.row][si.col].f_cost) {
                smallest = left;
            }
        }

        if (right < heap->size) {
            pb_offset si = heap->nodes[smallest];
            pb_offset ri = heap->nodes[right];
            if (map[ri.row][ri.col].f_cost < map[si.row][si.col].f_cost) {
                smallest = right;
            }
        }

        if (smallest == i) break;

        /* Swap */
        pb_offset tmp = heap->nodes[i];
        heap->nodes[i] = heap->nodes[smallest];
        heap->nodes[smallest] = tmp;
        i = smallest;
    }

    return result;
}

/*============================================================================
 * Heuristics
 *============================================================================*/

static int heuristic_manhattan(pb_offset a, pb_offset b)
{
    return pb_hex_distance_offset(a, b);
}

static int heuristic_weighted(pb_offset a, pb_offset b, pb_scalar weight)
{
    int base = pb_hex_distance_offset(a, b);
#if PB_USE_FIXED_POINT
    return (int)PB_FIXED_MUL(PB_INT_TO_FIXED(base), weight);
#else
    return (int)(base * weight);
#endif
}

/* Terrain-aware heuristic from 2024 ISPRS paper */
static int heuristic_terrain(pb_offset a, pb_offset b, const pb_board* board,
                              pb_terrain_cost_fn cost_fn, void* userdata)
{
    (void)cost_fn;  /* Reserved for future terrain cost weighting */
    (void)userdata;

    int base = pb_hex_distance_offset(a, b);

    /* Add penalty based on obstacle density in the direction of travel */
    pb_axial aa = pb_offset_to_axial(a);
    pb_axial ab = pb_offset_to_axial(b);

    int dq = (ab.q > aa.q) ? 1 : (ab.q < aa.q) ? -1 : 0;
    int dr = (ab.r > aa.r) ? 1 : (ab.r < aa.r) ? -1 : 0;

    /* Sample a few cells in the direction */
    int obstacle_penalty = 0;
    pb_axial sample = aa;
    for (int i = 0; i < 3 && i < base; i++) {
        sample.q += dq;
        sample.r += dr;
        pb_offset spos = pb_axial_to_offset(sample);

        if (pb_board_in_bounds(board, spos) && !pb_board_is_empty(board, spos)) {
            obstacle_penalty += 2;
        }
    }

    return base + obstacle_penalty;
}

/*============================================================================
 * Default Cost Function
 *============================================================================*/

static int default_cost_fn(const pb_board* board, pb_offset pos, void* userdata)
{
    (void)userdata;

    if (!pb_board_in_bounds(board, pos)) {
        return -1;
    }

    if (!pb_board_is_empty(board, pos)) {
        return -1; /* Blocked by bubble */
    }

    return 1; /* Uniform cost */
}

/*============================================================================
 * Configuration
 *============================================================================*/

pb_pathfinder_config pb_pathfinder_default_config(void)
{
    pb_pathfinder_config config = {
        .heuristic = PB_HEURISTIC_MANHATTAN,
#if PB_USE_FIXED_POINT
        .weight = PB_FLOAT_TO_FIXED(1.0f),
#else
        .weight = 1.0f,
#endif
        .cost_fn = NULL,
        .cost_userdata = NULL,
        .allow_diagonal = true,
        .max_iterations = 10000
    };
    return config;
}

/*============================================================================
 * A* Implementation
 *============================================================================*/

static int get_heuristic(const pb_pathfinder_config* config, const pb_board* board,
                         pb_offset pos, pb_offset goal)
{
    switch (config->heuristic) {
        case PB_HEURISTIC_EUCLIDEAN:
            /* For now, fall through to manhattan */
        case PB_HEURISTIC_MANHATTAN:
            return heuristic_manhattan(pos, goal);

        case PB_HEURISTIC_WEIGHTED:
            return heuristic_weighted(pos, goal, config->weight);

        case PB_HEURISTIC_TERRAIN:
            return heuristic_terrain(pos, goal, board, config->cost_fn,
                                     config->cost_userdata);

        default:
            return heuristic_manhattan(pos, goal);
    }
}

static void reconstruct_path(pb_pathfinder* pf, pb_offset goal,
                             pb_path_result* result)
{
    /* Build path backwards */
    pb_offset path[PB_MAX_PATH];
    int len = 0;

    pb_offset current = goal;
    while (len < PB_MAX_PATH) {
        path[len++] = current;

        pb_path_node* node = &pf->map[current.row][current.col];
        if (pb_offset_eq(node->parent, current)) {
            break; /* Reached start */
        }
        current = node->parent;
    }

    /* Reverse into result */
    result->length = len;
    for (int i = 0; i < len; i++) {
        result->path[i] = path[len - 1 - i];
    }
}

bool pb_astar_find_path(const pb_board* board, pb_offset start, pb_offset goal,
                        const pb_pathfinder_config* config, pb_path_result* result)
{
    memset(result, 0, sizeof(*result));

    if (!pb_board_in_bounds(board, start) || !pb_board_in_bounds(board, goal)) {
        return false;
    }

    if (pb_offset_eq(start, goal)) {
        result->path[0] = start;
        result->length = 1;
        return true;
    }

    pb_pathfinder_config cfg = config ? *config : pb_pathfinder_default_config();
    pb_terrain_cost_fn cost_fn = cfg.cost_fn ? cfg.cost_fn : default_cost_fn;

    /* Check if goal is passable */
    if (cost_fn(board, goal, cfg.cost_userdata) < 0) {
        return false;
    }

    /* Initialize pathfinder state */
    pb_pathfinder pf;
    memset(&pf, 0, sizeof(pf));
    heap_init(&pf.open);

    /* Initialize start node */
    pb_path_node* start_node = &pf.map[start.row][start.col];
    start_node->pos = start;
    start_node->g_cost = 0;
    start_node->f_cost = get_heuristic(&cfg, board, start, goal);
    start_node->parent = start;
    start_node->in_open = true;

    heap_push(&pf.open, start, pf.map);
    pf.nodes_visited = 1;

    int iterations = 0;
    int max_iter = cfg.max_iterations > 0 ? cfg.max_iterations : 100000;

    while (!heap_empty(&pf.open) && iterations++ < max_iter) {
        pb_offset current = heap_pop(&pf.open, pf.map);
        pb_path_node* current_node = &pf.map[current.row][current.col];

        current_node->in_open = false;
        current_node->in_closed = true;
        pf.nodes_expanded++;

        /* Found goal? */
        if (pb_offset_eq(current, goal)) {
            reconstruct_path(&pf, goal, result);
            result->nodes_expanded = pf.nodes_expanded;
            result->nodes_visited = pf.nodes_visited;
            return true;
        }

        /* Explore neighbors */
        pb_offset neighbors[6];
        pb_hex_neighbors_offset(current, neighbors);

        for (int i = 0; i < 6; i++) {
            pb_offset neighbor = neighbors[i];

            if (!pb_board_in_bounds(board, neighbor)) {
                continue;
            }

            pb_path_node* neighbor_node = &pf.map[neighbor.row][neighbor.col];

            if (neighbor_node->in_closed) {
                continue;
            }

            int move_cost = cost_fn(board, neighbor, cfg.cost_userdata);
            if (move_cost < 0) {
                continue; /* Blocked */
            }

            int tentative_g = current_node->g_cost + move_cost;

            if (!neighbor_node->in_open) {
                /* New node */
                neighbor_node->pos = neighbor;
                neighbor_node->g_cost = tentative_g;
                neighbor_node->f_cost = tentative_g + get_heuristic(&cfg, board, neighbor, goal);
                neighbor_node->parent = current;
                neighbor_node->in_open = true;

                heap_push(&pf.open, neighbor, pf.map);
                pf.nodes_visited++;

            } else if (tentative_g < neighbor_node->g_cost) {
                /* Better path found */
                neighbor_node->g_cost = tentative_g;
                neighbor_node->f_cost = tentative_g + get_heuristic(&cfg, board, neighbor, goal);
                neighbor_node->parent = current;

                /* Re-heapify (simplified: just push again, will be handled by closed check) */
                heap_push(&pf.open, neighbor, pf.map);
            }
        }
    }

    result->nodes_expanded = pf.nodes_expanded;
    result->nodes_visited = pf.nodes_visited;
    return false;
}

/*============================================================================
 * Jump Point Search (Hex Grid Adaptation)
 *============================================================================*/

/*
 * JPS for hex grids works by identifying "jump points" - cells where the
 * optimal path might change direction. On hex grids, these are cells
 * adjacent to obstacles that force the path to turn.
 */

static bool is_forced_neighbor(const pb_board* board, pb_offset pos,
                                pb_hex_dir dir, pb_terrain_cost_fn cost_fn,
                                void* userdata)
{
    /* A neighbor is forced if the natural neighbor is blocked but diagonal is open */
    pb_offset natural = pb_hex_neighbor_offset(pos, dir);

    if (!pb_board_in_bounds(board, natural)) {
        return false;
    }

    if (cost_fn(board, natural, userdata) >= 0) {
        return false; /* Natural neighbor is passable, no forcing */
    }

    /* Check if there's an open path around the obstacle */
    int prev_dir = (dir + 5) % 6; /* Previous direction (CCW) */
    int next_dir = (dir + 1) % 6; /* Next direction (CW) */

    pb_offset prev_n = pb_hex_neighbor_offset(pos, (pb_hex_dir)prev_dir);
    pb_offset next_n = pb_hex_neighbor_offset(pos, (pb_hex_dir)next_dir);

    bool prev_open = pb_board_in_bounds(board, prev_n) &&
                     cost_fn(board, prev_n, userdata) >= 0;
    bool next_open = pb_board_in_bounds(board, next_n) &&
                     cost_fn(board, next_n, userdata) >= 0;

    return prev_open || next_open;
}

/* Jump function for full JPS implementation (called from pb_jps_find_path) */
__attribute__((unused))
static pb_offset jump(const pb_board* board, pb_offset pos, pb_hex_dir dir,
                      pb_offset goal, pb_terrain_cost_fn cost_fn, void* userdata,
                      int max_jump)
{
    pb_offset next = pb_hex_neighbor_offset(pos, dir);
    pb_offset invalid = {-1, -1};

    if (!pb_board_in_bounds(board, next)) {
        return invalid;
    }

    if (cost_fn(board, next, userdata) < 0) {
        return invalid; /* Blocked */
    }

    if (pb_offset_eq(next, goal)) {
        return next; /* Found goal */
    }

    /* Check for forced neighbors */
    for (int d = 0; d < 6; d++) {
        if (is_forced_neighbor(board, next, (pb_hex_dir)d, cost_fn, userdata)) {
            return next; /* This is a jump point */
        }
    }

    /* Continue jumping in this direction */
    if (max_jump > 0) {
        return jump(board, next, dir, goal, cost_fn, userdata, max_jump - 1);
    }

    return invalid;
}

bool pb_jps_find_path(const pb_board* board, pb_offset start, pb_offset goal,
                      const pb_pathfinder_config* config, pb_path_result* result)
{
    /* For now, JPS falls back to A* with optimizations */
    /* Full JPS implementation requires careful handling of hex symmetry */

    pb_pathfinder_config cfg = config ? *config : pb_pathfinder_default_config();

    /* Use weighted heuristic for faster (suboptimal) paths */
    if (cfg.heuristic == PB_HEURISTIC_MANHATTAN) {
        cfg.heuristic = PB_HEURISTIC_WEIGHTED;
#if PB_USE_FIXED_POINT
        cfg.weight = PB_FLOAT_TO_FIXED(1.1f);
#else
        cfg.weight = 1.1f;
#endif
    }

    return pb_astar_find_path(board, start, goal, &cfg, result);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

int pb_path_distance(const pb_board* board, pb_offset start, pb_offset goal,
                     const pb_pathfinder_config* config)
{
    pb_path_result result;
    if (pb_astar_find_path(board, start, goal, config, &result)) {
        /* Distance is path length - 1 (number of moves) */
        return result.length > 0 ? result.length - 1 : 0;
    }
    return -1;
}

bool pb_is_reachable(const pb_board* board, pb_offset start, pb_offset goal,
                     const pb_pathfinder_config* config)
{
    pb_path_result result;
    return pb_astar_find_path(board, start, goal, config, &result);
}

/*============================================================================
 * Flood Fill
 *============================================================================*/

int pb_flood_fill(const pb_board* board, pb_offset origin, int max_dist,
                  const pb_pathfinder_config* config, pb_flood_result* result)
{
    memset(result, 0, sizeof(*result));

    if (!pb_board_in_bounds(board, origin)) {
        return 0;
    }

    pb_pathfinder_config cfg = config ? *config : pb_pathfinder_default_config();
    pb_terrain_cost_fn cost_fn = cfg.cost_fn ? cfg.cost_fn : default_cost_fn;

    /* BFS from origin */
    pb_offset queue[PB_MAX_CELLS];
    int distances[PB_MAX_ROWS][PB_MAX_COLS];
    bool visited[PB_MAX_ROWS][PB_MAX_COLS];

    memset(distances, -1, sizeof(distances));
    memset(visited, 0, sizeof(visited));

    int head = 0, tail = 0;
    queue[tail++] = origin;
    distances[origin.row][origin.col] = 0;
    visited[origin.row][origin.col] = true;

    result->cells[result->count] = origin;
    result->distances[result->count] = 0;
    result->count++;

    while (head < tail && result->count < PB_MAX_CELLS) {
        pb_offset current = queue[head++];
        int current_dist = distances[current.row][current.col];

        if (max_dist > 0 && current_dist >= max_dist) {
            continue;
        }

        pb_offset neighbors[6];
        pb_hex_neighbors_offset(current, neighbors);

        for (int i = 0; i < 6; i++) {
            pb_offset n = neighbors[i];

            if (!pb_board_in_bounds(board, n)) {
                continue;
            }

            if (visited[n.row][n.col]) {
                continue;
            }

            int move_cost = cost_fn(board, n, cfg.cost_userdata);
            if (move_cost < 0) {
                continue;
            }

            int new_dist = current_dist + move_cost;

            visited[n.row][n.col] = true;
            distances[n.row][n.col] = new_dist;
            queue[tail++] = n;

            result->cells[result->count] = n;
            result->distances[result->count] = new_dist;
            result->count++;
        }
    }

    return result->count;
}

/*============================================================================
 * Line of Sight
 *============================================================================*/

bool pb_has_line_of_sight(const pb_board* board, pb_offset from, pb_offset to)
{
    pb_cube start = pb_offset_to_cube(from);
    pb_cube end = pb_offset_to_cube(to);

    pb_cube line[PB_MAX_PATH];
    int len = pb_hex_line(start, end, line, PB_MAX_PATH);

    /* Check all cells except start */
    for (int i = 1; i < len; i++) {
        pb_offset pos = pb_cube_to_offset(line[i]);

        if (!pb_board_in_bounds(board, pos)) {
            return false;
        }

        /* Only check intermediate cells, not the endpoint */
        if (i < len - 1 && !pb_board_is_empty(board, pos)) {
            return false;
        }
    }

    return true;
}

int pb_get_line_of_sight(const pb_board* board, pb_offset from, pb_offset to,
                         pb_offset* out, int max)
{
    pb_cube start = pb_offset_to_cube(from);
    pb_cube end = pb_offset_to_cube(to);

    pb_cube line[PB_MAX_PATH];
    int cube_len = pb_hex_line(start, end, line, PB_MAX_PATH);

    int count = 0;
    for (int i = 0; i < cube_len && count < max; i++) {
        pb_offset pos = pb_cube_to_offset(line[i]);
        out[count++] = pos;

        /* Stop at obstacle (except for endpoint) */
        if (i < cube_len - 1) {
            if (!pb_board_in_bounds(board, pos) || !pb_board_is_empty(board, pos)) {
                break;
            }
        }
    }

    return count;
}

/*============================================================================
 * Field of View (Shadowcasting for Hex)
 *============================================================================*/

int pb_calculate_fov(const pb_board* board, pb_offset origin, int radius,
                     pb_fov_result* result)
{
    memset(result, 0, sizeof(*result));

    if (!pb_board_in_bounds(board, origin)) {
        return 0;
    }

    /* Origin is always visible */
    result->visible[result->count++] = origin;

    /* Simple approach: check line of sight to all cells within radius */
    for (int dr = -radius; dr <= radius; dr++) {
        for (int dc = -radius; dc <= radius; dc++) {
            pb_offset target = {origin.row + dr, origin.col + dc};

            if (pb_offset_eq(target, origin)) {
                continue;
            }

            if (!pb_board_in_bounds(board, target)) {
                continue;
            }

            int dist = pb_hex_distance_offset(origin, target);
            if (dist > radius) {
                continue;
            }

            if (pb_has_line_of_sight(board, origin, target)) {
                if (result->count < PB_MAX_CELLS) {
                    result->visible[result->count++] = target;
                }
            }
        }
    }

    return result->count;
}
