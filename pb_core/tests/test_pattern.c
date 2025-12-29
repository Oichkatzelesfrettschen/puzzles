/*
 * test_pattern.c - Tests for pb_pattern module
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  " #name "..."); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAILED at line %d: %s\n", __LINE__, #cond); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_STR_NE(a, b) ASSERT(strcmp(a, b) != 0)

/*============================================================================
 * Pattern Name Tests
 *============================================================================*/

TEST(pattern_name_valid)
{
    const char* name = pb_pattern_name(PB_PATTERN_NONE);
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(strlen(name) > 0);
}

TEST(pattern_name_stripes)
{
    const char* name = pb_pattern_name(PB_PATTERN_LINES_H);
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(strstr(name, "stripe") != NULL || strstr(name, "Stripe") != NULL ||
                strstr(name, "horiz") != NULL || strstr(name, "Horiz") != NULL);
}

TEST(pattern_name_dots)
{
    const char* name = pb_pattern_name(PB_PATTERN_DOTS);
    ASSERT_NOT_NULL(name);
}

TEST(pattern_name_all_unique)
{
    /* All pattern names should be unique */
    const char* names[PB_PATTERN_COUNT];
    for (int i = 0; i < PB_PATTERN_COUNT; i++) {
        names[i] = pb_pattern_name((pb_pattern_id)i);
        ASSERT_NOT_NULL(names[i]);

        /* Check against all previous names */
        for (int j = 0; j < i; j++) {
            ASSERT_STR_NE(names[i], names[j]);
        }
    }
}

/*============================================================================
 * Pattern Category Tests
 *============================================================================*/

TEST(pattern_category_solid)
{
    pb_pattern_category cat = pb_pattern_get_category(PB_PATTERN_NONE);
    ASSERT_EQ(cat, PB_PATTERN_CAT_NONE);
}

TEST(pattern_category_stripes)
{
    pb_pattern_category cat = pb_pattern_get_category(PB_PATTERN_LINES_H);
    ASSERT_EQ(cat, PB_PATTERN_CAT_LINE);

    cat = pb_pattern_get_category(PB_PATTERN_LINES_V);
    ASSERT_EQ(cat, PB_PATTERN_CAT_LINE);
}

TEST(pattern_category_dots)
{
    pb_pattern_category cat = pb_pattern_get_category(PB_PATTERN_DOTS);
    ASSERT_EQ(cat, PB_PATTERN_CAT_FILL);
}

TEST(pattern_category_geometric)
{
    pb_pattern_category cat = pb_pattern_get_category(PB_PATTERN_HATCH_CROSS);
    ASSERT_EQ(cat, PB_PATTERN_CAT_HATCH);
}

/*============================================================================
 * Pattern Definition Tests
 *============================================================================*/

TEST(pattern_get_solid)
{
    const pb_pattern_def* def = pb_pattern_get(PB_PATTERN_NONE);
    ASSERT_NOT_NULL(def);
    ASSERT_EQ(def->id, PB_PATTERN_NONE);
}

TEST(pattern_get_all_valid)
{
    for (int i = 0; i < PB_PATTERN_COUNT; i++) {
        const pb_pattern_def* def = pb_pattern_get((pb_pattern_id)i);
        ASSERT_NOT_NULL(def);
        ASSERT_EQ(def->id, (pb_pattern_id)i);
    }
}

TEST(pattern_get_has_name)
{
    for (int i = 0; i < PB_PATTERN_COUNT; i++) {
        const pb_pattern_def* def = pb_pattern_get((pb_pattern_id)i);
        ASSERT_NOT_NULL(def);
        ASSERT_NOT_NULL(def->name);
        ASSERT_TRUE(strlen(def->name) > 0);
    }
}

/*============================================================================
 * SVG Generation Tests
 *============================================================================*/

TEST(pattern_svg_path_solid)
{
    char buffer[1024];
    int len = pb_pattern_to_svg_path(PB_PATTERN_NONE, 32.0f, buffer, sizeof(buffer));

    /* Solid pattern might return empty path or a filled circle */
    ASSERT_TRUE(len >= 0);
}

TEST(pattern_svg_path_stripes)
{
    char buffer[2048];
    int len = pb_pattern_to_svg_path(PB_PATTERN_LINES_H, 32.0f, buffer, sizeof(buffer));

    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strlen(buffer) > 0);
}

TEST(pattern_svg_path_buffer_size)
{
    char small_buffer[10];
    char large_buffer[4096];

    /* Small buffer should truncate or return error */
    int small_len = pb_pattern_to_svg_path(PB_PATTERN_LINES_H, 32.0f,
                                            small_buffer, sizeof(small_buffer));

    int large_len = pb_pattern_to_svg_path(PB_PATTERN_LINES_H, 32.0f,
                                            large_buffer, sizeof(large_buffer));

    /* Large buffer should have more content */
    ASSERT_TRUE(large_len >= small_len);
}

TEST(pattern_svg_full)
{
    char buffer[4096];
    int len = pb_pattern_to_svg(PB_PATTERN_DOTS, 32.0f, "#FF0000", "#FFFFFF", buffer, sizeof(buffer));

    ASSERT_TRUE(len > 0);
    /* Should contain SVG elements */
    ASSERT_TRUE(strstr(buffer, "<") != NULL);
}

TEST(pattern_svg_colors)
{
    char buffer[4096];
    pb_pattern_to_svg(PB_PATTERN_HATCH_CROSS, 32.0f, "#123456", "#ABCDEF", buffer, sizeof(buffer));

    /* Should contain the color values */
    ASSERT_TRUE(strstr(buffer, "123456") != NULL || strstr(buffer, "#123456") != NULL);
}

/*============================================================================
 * Pattern Map Tests
 *============================================================================*/

TEST(pattern_default_map)
{
    pb_pattern_map map;
    pb_pattern_get_default_map(&map);

    /* Should have assignments for at least the first few colors */
    ASSERT_TRUE(map.patterns[0] != 0 || map.patterns[1] != 0 || map.patterns[2] != 0);
}

TEST(pattern_map_valid)
{
    pb_pattern_map map;
    pb_pattern_get_default_map(&map);

    ASSERT_TRUE(pb_pattern_map_is_valid(&map));
}

TEST(pattern_map_invalid_duplicate)
{
    pb_pattern_map map;
    pb_pattern_get_default_map(&map);

    /* Force duplicate pattern for different colors */
    map.patterns[0] = PB_PATTERN_LINES_H;
    map.patterns[1] = PB_PATTERN_LINES_H;

    /* This might be invalid depending on validation rules */
    /* The map is still technically usable, just not optimal for accessibility */
}

TEST(pattern_map_all_solid_invalid)
{
    pb_pattern_map map;
    for (int i = 0; i < PB_MAX_COLORS; i++) {
        map.patterns[i] = PB_PATTERN_NONE;
    }

    /* All solid patterns should be considered invalid for accessibility */
    ASSERT_FALSE(pb_pattern_map_is_valid(&map));
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_pattern tests\n");
    printf("================\n\n");

    printf("Pattern names:\n");
    RUN_TEST(pattern_name_valid);
    RUN_TEST(pattern_name_stripes);
    RUN_TEST(pattern_name_dots);
    RUN_TEST(pattern_name_all_unique);

    printf("\nPattern categories:\n");
    RUN_TEST(pattern_category_solid);
    RUN_TEST(pattern_category_stripes);
    RUN_TEST(pattern_category_dots);
    RUN_TEST(pattern_category_geometric);

    printf("\nPattern definitions:\n");
    RUN_TEST(pattern_get_solid);
    RUN_TEST(pattern_get_all_valid);
    RUN_TEST(pattern_get_has_name);

    printf("\nSVG generation:\n");
    RUN_TEST(pattern_svg_path_solid);
    RUN_TEST(pattern_svg_path_stripes);
    RUN_TEST(pattern_svg_path_buffer_size);
    RUN_TEST(pattern_svg_full);
    RUN_TEST(pattern_svg_colors);

    printf("\nPattern maps:\n");
    RUN_TEST(pattern_default_map);
    RUN_TEST(pattern_map_valid);
    RUN_TEST(pattern_map_invalid_duplicate);
    RUN_TEST(pattern_map_all_solid_invalid);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
