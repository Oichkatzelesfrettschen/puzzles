/*
 * pb_validate.c - Level validation and analysis tool
 *
 * Usage: pb_validate <level.json> [options]
 *
 * Options:
 *   -v, --verbose    Show detailed analysis
 *   -s, --solve      Attempt to find solution
 *   -d, --difficulty Show difficulty estimate
 *   -q, --quiet      Only show errors
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Command Line Parsing
 *============================================================================*/

typedef struct options {
    const char* level_path;
    bool verbose;
    bool solve;
    bool difficulty;
    bool quiet;
} options;

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <level.json> [options]\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v, --verbose    Show detailed analysis\n");
    fprintf(stderr, "  -s, --solve      Attempt to find solution\n");
    fprintf(stderr, "  -d, --difficulty Show difficulty estimate\n");
    fprintf(stderr, "  -q, --quiet      Only show errors\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s levels/easy/level01.json\n", prog);
    fprintf(stderr, "  %s levels/hard/level05.json -v -s\n", prog);
}

static bool parse_args(int argc, char** argv, options* opts)
{
    memset(opts, 0, sizeof(*opts));

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                opts->verbose = true;
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--solve") == 0) {
                opts->solve = true;
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--difficulty") == 0) {
                opts->difficulty = true;
            } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
                opts->quiet = true;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                return false;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                return false;
            }
        } else {
            if (opts->level_path) {
                fprintf(stderr, "Multiple level files not supported\n");
                return false;
            }
            opts->level_path = argv[i];
        }
    }

    if (!opts->level_path) {
        fprintf(stderr, "Error: No level file specified\n");
        return false;
    }

    return true;
}

/*============================================================================
 * Analysis Output
 *============================================================================*/

static void print_board_stats(const pb_board_stats* stats)
{
    printf("Board Statistics:\n");
    printf("  Total bubbles: %d\n", stats->total_bubbles);
    printf("  Height: %d rows\n", stats->height);
    printf("  Density: %.1f%%\n", stats->density * 100.0f);
    printf("  Orphans: %d\n", stats->orphan_count);
    printf("  Blockers: %d\n", stats->blocker_count);

    printf("  Colors: ");
    bool first = true;
    for (int c = 0; c < PB_MAX_COLORS; c++) {
        if (stats->color_counts[c] > 0) {
            if (!first) printf(", ");
            printf("%d:%d", c, stats->color_counts[c]);
            first = false;
        }
    }
    printf("\n");

    if (stats->max_group_size > 1) {
        printf("  Largest group: %d bubbles\n", stats->max_group_size);
    }
}

static void print_difficulty(const pb_difficulty_info* info)
{
    const char* ratings[] = {
        "TRIVIAL", "EASY", "MEDIUM", "HARD", "EXPERT", "UNKNOWN"
    };

    printf("\nDifficulty Analysis:\n");
    printf("  Rating: %s\n", ratings[info->rating]);
    printf("  Estimated moves: %d\n", info->estimated_moves);
    printf("  Precision required: %.0f%%\n", info->precision_required * 100.0f);

    if (info->time_pressure > 0) {
        printf("  Time pressure: %.0f%%\n", info->time_pressure * 100.0f);
    }
    if (info->chokepoints > 0) {
        printf("  Chokepoints: %d\n", info->chokepoints);
    }
    printf("  Notes: %s\n", info->notes);
}

static void print_solution(const pb_solvability* result)
{
    printf("\nSolvability Analysis:\n");

    if (result->solvable) {
        printf("  Status: SOLVABLE\n");
        printf("  Moves needed: %d\n", result->min_moves);
        printf("  Confidence: %.0f%%\n", result->confidence * 100.0f);

        if (result->solution.count > 0) {
            printf("\n  Suggested moves:\n");
            for (int i = 0; i < result->solution.count && i < 5; i++) {
                const pb_move* m = &result->solution.moves[i];
                printf("    %d. Color %d -> (%d,%d) [score: %.1f]\n",
                       i + 1, m->color_id, m->target.row, m->target.col,
                       m->score);
            }
        }
    } else if (result->possibly_solvable) {
        printf("  Status: POSSIBLY SOLVABLE\n");
        printf("  Min moves estimated: %d\n", result->min_moves);
        printf("  Confidence: %.0f%%\n", result->confidence * 100.0f);
    } else {
        printf("  Status: UNSOLVABLE (within analysis depth)\n");
    }
}

/*============================================================================
 * Main
 *============================================================================*/

int main(int argc, char** argv)
{
    options opts;
    if (!parse_args(argc, argv, &opts)) {
        print_usage(argv[0]);
        return 1;
    }

    /* Load level */
    pb_level_data level;
    pb_data_result result;
    if (!pb_level_load_file(opts.level_path, &level, &result)) {
        fprintf(stderr, "Error loading level: %s\n", result.error);
        return 1;
    }

    if (!opts.quiet) {
        printf("Level: %s\n", level.name[0] ? level.name : opts.level_path);
        if (level.author[0]) {
            printf("Author: %s\n", level.author);
        }
        printf("\n");
    }

    /* Convert level to board */
    pb_board board;
    pb_level_to_board(&level, &board);

    /* Analyze board */
    pb_board_stats stats;
    pb_board_analyze(&board, &stats);

    if (opts.verbose) {
        print_board_stats(&stats);
    }

    /* Validate level */
    uint8_t available_colors = 0xFF;  /* All colors available by default */
    pb_validation_info validation;
    pb_validation_result valid = pb_validate_level(&board, NULL, available_colors,
                                                    &validation);

    if (valid != PB_VALID) {
        printf("VALIDATION FAILED: %s\n", validation.message);
        pb_level_data_free(&level);
        return 2;
    }

    if (!opts.quiet) {
        printf("Validation: PASSED (%s)\n", validation.message);
    }

    /* Difficulty analysis */
    if (opts.difficulty || opts.verbose) {
        pb_difficulty_info diff;
        pb_estimate_difficulty(&board, NULL, &diff);
        print_difficulty(&diff);
    }

    /* Solvability analysis */
    if (opts.solve) {
        printf("\nAnalyzing solvability (this may take a moment)...\n");

        pb_solvability solve;
        pb_analyze_solvability(&board, NULL, NULL, 0, 1000, &solve);
        print_solution(&solve);
    }

    pb_level_data_free(&level);

    if (!opts.quiet) {
        printf("\nDone.\n");
    }

    return 0;
}
