/*
 * pb_board.c - Board operations and BFS traversal
 *
 * Uses unified BFS traversal for both match detection and orphan finding.
 * This avoids recursion (stack-safe) and allows flexible visitor predicates.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_board.h"
#include "pb/pb_rng.h"


/*============================================================================
 * Board Initialization
 *============================================================================*/

void pb_board_init(pb_board* board)
{
    pb_board_init_custom(board, PB_DEFAULT_ROWS,
                         PB_DEFAULT_COLS_EVEN, PB_DEFAULT_COLS_ODD);
}

void pb_board_init_custom(pb_board* board, int rows, int cols_even, int cols_odd)
{
    memset(board, 0, sizeof(*board));
    board->rows = rows;
    board->cols_even = cols_even;
    board->cols_odd = cols_odd;
    board->ceiling_row = 0;
}

void pb_board_clear(pb_board* board)
{
    memset(board->cells, 0, sizeof(board->cells));
}

/*============================================================================
 * Cell Access
 *============================================================================*/

bool pb_board_in_bounds(const pb_board* board, pb_offset pos)
{
    return pb_offset_in_bounds(pos, board->rows, board->cols_even, board->cols_odd);
}

pb_bubble* pb_board_get(pb_board* board, pb_offset pos)
{
    if (!pb_board_in_bounds(board, pos)) {
        return NULL;
    }
    return &board->cells[pos.row][pos.col];
}

const pb_bubble* pb_board_get_const(const pb_board* board, pb_offset pos)
{
    if (!pb_board_in_bounds(board, pos)) {
        return NULL;
    }
    return &board->cells[pos.row][pos.col];
}

bool pb_board_is_empty(const pb_board* board, pb_offset pos)
{
    const pb_bubble* b = pb_board_get_const(board, pos);
    return b == NULL || b->kind == PB_KIND_NONE;
}

bool pb_board_set(pb_board* board, pb_offset pos, pb_bubble bubble)
{
    if (!pb_board_in_bounds(board, pos)) {
        return false;
    }
    board->cells[pos.row][pos.col] = bubble;
    return true;
}

void pb_board_remove(pb_board* board, pb_offset pos)
{
    if (pb_board_in_bounds(board, pos)) {
        board->cells[pos.row][pos.col].kind = PB_KIND_NONE;
    }
}

/*============================================================================
 * Unified BFS Traversal
 *============================================================================*/

/* Internal visited bitmap for BFS */
typedef struct {
    uint8_t bits[PB_MAX_ROWS][(PB_MAX_COLS + 7) / 8];
} pb_visited_map;

static inline void visited_clear(pb_visited_map* v)
{
    memset(v->bits, 0, sizeof(v->bits));
}

static inline bool visited_get(const pb_visited_map* v, pb_offset pos)
{
    return (v->bits[pos.row][pos.col / 8] & (1 << (pos.col % 8))) != 0;
}

static inline void visited_set(pb_visited_map* v, pb_offset pos)
{
    v->bits[pos.row][pos.col / 8] |= (1 << (pos.col % 8));
}

int pb_visit_connected(const pb_board* board, pb_offset origin,
                       pb_visitor_fn visitor, void* userdata,
                       pb_visit_result* result)
{
    result->count = 0;

    /* Early exit if origin is out of bounds or doesn't match predicate */
    if (!pb_board_in_bounds(board, origin)) {
        return 0;
    }
    if (!visitor(board, origin, origin, userdata)) {
        return 0;
    }

    /* BFS queue and visited map */
    pb_offset queue[PB_MAX_VISITED];
    int head = 0, tail = 0;

    pb_visited_map visited;
    visited_clear(&visited);

    /* Enqueue origin */
    queue[tail++] = origin;
    visited_set(&visited, origin);

    while (head < tail) {
        pb_offset current = queue[head++];

        /* Add to result */
        result->cells[result->count++] = current;

        /* Get neighbors */
        pb_offset neighbors[6];
        pb_hex_neighbors_offset(current, neighbors);

        for (int i = 0; i < 6; i++) {
            pb_offset neighbor = neighbors[i];

            /* Skip if out of bounds or already visited */
            if (!pb_board_in_bounds(board, neighbor)) {
                continue;
            }
            if (visited_get(&visited, neighbor)) {
                continue;
            }

            /* Check if neighbor matches predicate */
            if (visitor(board, neighbor, origin, userdata)) {
                visited_set(&visited, neighbor);
                queue[tail++] = neighbor;
            }
        }
    }

    return result->count;
}

/*============================================================================
 * Match Detection Visitors
 *============================================================================*/

/* Visitor context for color matching */
typedef struct {
    uint8_t target_color;
    bool match_wildcards;
} color_match_ctx;

static bool color_match_visitor(const pb_board* board, pb_offset pos,
                                 pb_offset origin, void* userdata)
{
    (void)origin;
    color_match_ctx* ctx = (color_match_ctx*)userdata;

    const pb_bubble* b = pb_board_get_const(board, pos);
    if (b == NULL) {
        return false;
    }

    /* Match colored bubbles of same color */
    if (b->kind == PB_KIND_COLORED && b->color_id == ctx->target_color) {
        return true;
    }

    /* Wildcards match any color if enabled */
    if (ctx->match_wildcards && b->kind == PB_KIND_WILDCARD) {
        return true;
    }

    /* Rainbow special also matches any color */
    if (b->kind == PB_KIND_SPECIAL && b->special == PB_SPECIAL_RAINBOW) {
        return true;
    }

    return false;
}

int pb_find_matches(const pb_board* board, pb_offset origin,
                    pb_visit_result* result)
{
    const pb_bubble* b = pb_board_get_const(board, origin);
    if (b == NULL || (b->kind != PB_KIND_COLORED && b->kind != PB_KIND_WILDCARD)) {
        result->count = 0;
        return 0;
    }

    color_match_ctx ctx = {
        .target_color = b->color_id,
        .match_wildcards = true
    };

    return pb_visit_connected(board, origin, color_match_visitor, &ctx, result);
}

bool pb_has_match(const pb_board* board, pb_offset pos, int threshold)
{
    pb_visit_result result;
    int count = pb_find_matches(board, pos, &result);
    return count >= threshold;
}

/*============================================================================
 * Orphan Detection
 *============================================================================*/

/* Visitor for "any non-empty cell" */
static bool any_bubble_visitor(const pb_board* board, pb_offset pos,
                                pb_offset origin, void* userdata)
{
    (void)origin;
    (void)userdata;

    const pb_bubble* b = pb_board_get_const(board, pos);
    if (b == NULL || b->kind == PB_KIND_NONE) {
        return false;
    }

    /* Ghost bubbles don't anchor or get anchored */
    if (b->flags & PB_FLAG_GHOST) {
        return false;
    }

    return true;
}

int pb_find_anchored(const pb_board* board, pb_visit_result* result)
{
    result->count = 0;

    /* Track all visited cells across all ceiling seeds */
    pb_visited_map global_visited;
    visited_clear(&global_visited);

    /* BFS from each ceiling cell */
    int ceiling_row = board->ceiling_row;
    int ceiling_cols = pb_row_cols(ceiling_row, board->cols_even, board->cols_odd);

    for (int col = 0; col < ceiling_cols; col++) {
        pb_offset start = {ceiling_row, col};

        /* Skip empty cells and already visited */
        if (pb_board_is_empty(board, start)) {
            continue;
        }
        if (visited_get(&global_visited, start)) {
            continue;
        }

        /* Also check for explicit anchor flag */
        const pb_bubble* b = pb_board_get_const(board, start);
        bool is_anchor = (b->flags & PB_FLAG_ANCHOR) ||
                         (start.row == ceiling_row);

        if (!is_anchor) {
            continue;
        }

        /* Run BFS from this anchor point */
        pb_visit_result local;
        pb_visit_connected(board, start, any_bubble_visitor, NULL, &local);

        /* Merge into global result, skipping duplicates */
        for (int i = 0; i < local.count; i++) {
            pb_offset pos = local.cells[i];
            if (!visited_get(&global_visited, pos)) {
                visited_set(&global_visited, pos);
                result->cells[result->count++] = pos;
            }
        }
    }

    return result->count;
}

int pb_find_orphans(const pb_board* board, pb_visit_result* result)
{
    /* First find all anchored cells */
    pb_visit_result anchored;
    pb_find_anchored(board, &anchored);

    /* Build visited map from anchored cells */
    pb_visited_map anchored_map;
    visited_clear(&anchored_map);
    for (int i = 0; i < anchored.count; i++) {
        visited_set(&anchored_map, anchored.cells[i]);
    }

    /* Scan all cells - orphans are non-empty cells not in anchored set */
    result->count = 0;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};

            if (pb_board_is_empty(board, pos)) {
                continue;
            }

            /* Check frozen flag - frozen bubbles don't fall */
            const pb_bubble* b = pb_board_get_const(board, pos);
            if (b->flags & PB_FLAG_FROZEN) {
                continue;
            }

            if (!visited_get(&anchored_map, pos)) {
                result->cells[result->count++] = pos;
            }
        }
    }

    return result->count;
}

/*============================================================================
 * Batch Operations
 *============================================================================*/

int pb_board_remove_cells(pb_board* board, const pb_visit_result* cells)
{
    int removed = 0;
    for (int i = 0; i < cells->count; i++) {
        pb_offset pos = cells->cells[i];
        if (!pb_board_is_empty(board, pos)) {
            pb_board_remove(board, pos);
            removed++;
        }
    }
    return removed;
}

void pb_board_count_colors(const pb_board* board, int counts[PB_MAX_COLORS])
{
    memset(counts, 0, sizeof(int) * PB_MAX_COLORS);

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            const pb_bubble* b = &board->cells[row][col];
            if (b->kind == PB_KIND_COLORED && b->color_id < PB_MAX_COLORS) {
                counts[b->color_id]++;
            }
        }
    }
}

uint8_t pb_board_color_mask(const pb_board* board)
{
    uint8_t mask = 0;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            const pb_bubble* b = &board->cells[row][col];
            if (b->kind == PB_KIND_COLORED && b->color_id < PB_MAX_COLORS) {
                mask |= (1 << b->color_id);
            }
        }
    }

    return mask;
}

bool pb_board_is_clear(const pb_board* board)
{
    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            if (board->cells[row][col].kind != PB_KIND_NONE) {
                return false;
            }
        }
    }
    return true;
}

uint32_t pb_board_checksum(const pb_board* board)
{
    /* FNV-1a hash of board state */
    uint32_t hash = 2166136261u;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            const pb_bubble* b = &board->cells[row][col];
            const uint8_t* bytes = (const uint8_t*)b;
            for (size_t i = 0; i < sizeof(*b); i++) {
                hash ^= bytes[i];
                hash *= 16777619u;
            }
        }
    }

    /* Include board dimensions */
    hash ^= (uint32_t)board->rows;
    hash *= 16777619u;
    hash ^= (uint32_t)board->cols_even;
    hash *= 16777619u;
    hash ^= (uint32_t)board->cols_odd;
    hash *= 16777619u;
    hash ^= (uint32_t)board->ceiling_row;
    hash *= 16777619u;

    return hash;
}

/*============================================================================
 * Row Insertion (Survival Mode)
 *============================================================================*/

bool pb_board_insert_row(pb_board* board, pb_rng* rng, uint8_t allowed_colors)
{
    /* Check if bottom row has any bubbles (would cause overflow) */
    int bottom_row = board->rows - 1;
    int bottom_cols = pb_row_cols(bottom_row, board->cols_even, board->cols_odd);
    bool will_overflow = false;

    for (int col = 0; col < bottom_cols; col++) {
        if (board->cells[bottom_row][col].kind != PB_KIND_NONE) {
            will_overflow = true;
            break;
        }
    }

    /* Shift all rows down by one */
    for (int row = board->rows - 1; row > 0; row--) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            /* Copy from row above - handle column offset for hex grid */
            pb_offset from = {row - 1, col};
            if (pb_board_in_bounds(board, from)) {
                board->cells[row][col] = board->cells[row - 1][col];
            } else {
                board->cells[row][col] = (pb_bubble){0};
            }
        }
    }

    /* Fill row 0 with new bubbles */
    int row0_cols = pb_row_cols(0, board->cols_even, board->cols_odd);
    for (int col = 0; col < row0_cols; col++) {
        /* Pick random color from allowed colors */
        int color = pb_rng_pick_color(rng, allowed_colors);

        board->cells[0][col] = (pb_bubble){
            .kind = PB_KIND_COLORED,
            .color_id = (uint8_t)color,
            .flags = 0,
            .special = PB_SPECIAL_NONE
        };
    }

    return !will_overflow;
}
