/*
 * test_rng.c - Tests for pb_rng module
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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

/*============================================================================
 * Seeding Tests
 *============================================================================*/

TEST(rng_seed_deterministic)
{
    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 12345);
    pb_rng_seed(&rng2, 12345);

    /* Same seed should produce same sequence */
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(pb_rng_next(&rng1), pb_rng_next(&rng2));
    }
}

TEST(rng_seed_different)
{
    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 12345);
    pb_rng_seed(&rng2, 54321);

    /* Different seeds should produce different values */
    int same_count = 0;
    for (int i = 0; i < 100; i++) {
        if (pb_rng_next(&rng1) == pb_rng_next(&rng2)) {
            same_count++;
        }
    }
    /* Extremely unlikely to have more than a few matches */
    ASSERT_TRUE(same_count < 5);
}

TEST(rng_seed_zero)
{
    pb_rng rng;
    pb_rng_seed(&rng, 0);

    /* Should still produce values (not stuck) */
    uint32_t first = pb_rng_next(&rng);
    uint32_t second = pb_rng_next(&rng);
    ASSERT_NE(first, second);
}

/*============================================================================
 * State Save/Restore Tests
 *============================================================================*/

TEST(rng_state_save_restore)
{
    pb_rng rng;
    pb_rng_seed(&rng, 99999);

    /* Advance a bit */
    for (int i = 0; i < 50; i++) {
        pb_rng_next(&rng);
    }

    /* Save state */
    uint32_t saved_state[4];
    pb_rng_get_state(&rng, saved_state);

    /* Get next values */
    uint32_t expected[10];
    for (int i = 0; i < 10; i++) {
        expected[i] = pb_rng_next(&rng);
    }

    /* Restore state and verify same sequence */
    pb_rng_set_state(&rng, saved_state);
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(pb_rng_next(&rng), expected[i]);
    }
}

/*============================================================================
 * Range Tests
 *============================================================================*/

TEST(rng_range_max)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        uint32_t val = pb_rng_range(&rng, 10);
        ASSERT_TRUE(val < 10);
    }
}

TEST(rng_range_int)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        int val = pb_rng_range_int(&rng, 5, 15);
        ASSERT_TRUE(val >= 5 && val <= 15);
    }
}

TEST(rng_range_int_negative)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        int val = pb_rng_range_int(&rng, -10, 10);
        ASSERT_TRUE(val >= -10 && val <= 10);
    }
}

TEST(rng_range_distribution)
{
    pb_rng rng;
    pb_rng_seed(&rng, 123);

    /* Check rough uniformity */
    int buckets[10] = {0};
    int samples = 10000;

    for (int i = 0; i < samples; i++) {
        uint32_t val = pb_rng_range(&rng, 10);
        buckets[val]++;
    }

    /* Each bucket should have roughly 1000 samples (10%) */
    /* Allow 20% deviation */
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(buckets[i] > 800 && buckets[i] < 1200);
    }
}

/*============================================================================
 * Float Tests
 *============================================================================*/

TEST(rng_float_range_01)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        float val = pb_rng_float(&rng);
        ASSERT_TRUE(val >= 0.0f && val < 1.0f);
    }
}

TEST(rng_float_range_custom)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        float val = pb_rng_float_range(&rng, -5.0f, 5.0f);
        ASSERT_TRUE(val >= -5.0f && val <= 5.0f);
    }
}

TEST(rng_float_distribution)
{
    pb_rng rng;
    pb_rng_seed(&rng, 456);

    float sum = 0;
    int samples = 10000;

    for (int i = 0; i < samples; i++) {
        sum += pb_rng_float(&rng);
    }

    /* Mean should be close to 0.5 */
    float mean = sum / samples;
    ASSERT_TRUE(mean > 0.48f && mean < 0.52f);
}

/*============================================================================
 * Shuffle Tests
 *============================================================================*/

TEST(rng_shuffle_permutation)
{
    pb_rng rng;
    pb_rng_seed(&rng, 789);

    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    pb_rng_shuffle(&rng, arr, 10, sizeof(int));

    /* Check all elements still present */
    int present[10] = {0};
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(arr[i] >= 0 && arr[i] <= 9);
        present[arr[i]] = 1;
    }
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(present[i]);
    }
}

TEST(rng_shuffle_deterministic)
{
    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 999);
    pb_rng_seed(&rng2, 999);

    int arr1[5] = {0, 1, 2, 3, 4};
    int arr2[5] = {0, 1, 2, 3, 4};

    pb_rng_shuffle(&rng1, arr1, 5, sizeof(int));
    pb_rng_shuffle(&rng2, arr2, 5, sizeof(int));

    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(arr1[i], arr2[i]);
    }
}

TEST(rng_shuffle_different_order)
{
    pb_rng rng;
    pb_rng_seed(&rng, 111);

    int original[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int shuffled[10];
    memcpy(shuffled, original, sizeof(original));

    pb_rng_shuffle(&rng, shuffled, 10, sizeof(int));

    /* Very unlikely to stay in same order */
    int same_position = 0;
    for (int i = 0; i < 10; i++) {
        if (shuffled[i] == original[i]) {
            same_position++;
        }
    }
    ASSERT_TRUE(same_position < 5);
}

/*============================================================================
 * Pick Color Tests
 *============================================================================*/

TEST(rng_pick_color_from_mask)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    /* Only allow colors 1, 3, 5 (mask = 0b00101010 = 42) */
    uint8_t mask = (1 << 1) | (1 << 3) | (1 << 5);

    for (int i = 0; i < 100; i++) {
        int color = pb_rng_pick_color(&rng, mask);
        ASSERT_TRUE(color == 1 || color == 3 || color == 5);
    }
}

TEST(rng_pick_color_single)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    /* Only color 4 allowed */
    uint8_t mask = (1 << 4);

    for (int i = 0; i < 10; i++) {
        int color = pb_rng_pick_color(&rng, mask);
        ASSERT_EQ(color, 4);
    }
}

TEST(rng_pick_color_distribution)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    /* Colors 0, 1, 2, 3 allowed */
    uint8_t mask = 0x0F;
    int counts[4] = {0};
    int samples = 4000;

    for (int i = 0; i < samples; i++) {
        int color = pb_rng_pick_color(&rng, mask);
        ASSERT_TRUE(color >= 0 && color < 4);
        counts[color]++;
    }

    /* Each color should appear roughly 1000 times */
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(counts[i] > 800 && counts[i] < 1200);
    }
}

/*============================================================================
 * Checksum Tests
 *============================================================================*/

TEST(rng_checksum_deterministic)
{
    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 12345);
    pb_rng_seed(&rng2, 12345);

    ASSERT_EQ(pb_rng_checksum(&rng1), pb_rng_checksum(&rng2));
}

TEST(rng_checksum_changes)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    uint32_t cs1 = pb_rng_checksum(&rng);
    pb_rng_next(&rng);
    uint32_t cs2 = pb_rng_checksum(&rng);

    ASSERT_NE(cs1, cs2);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_rng tests\n");
    printf("============\n\n");

    printf("Seeding:\n");
    RUN_TEST(rng_seed_deterministic);
    RUN_TEST(rng_seed_different);
    RUN_TEST(rng_seed_zero);

    printf("\nState save/restore:\n");
    RUN_TEST(rng_state_save_restore);

    printf("\nRange generation:\n");
    RUN_TEST(rng_range_max);
    RUN_TEST(rng_range_int);
    RUN_TEST(rng_range_int_negative);
    RUN_TEST(rng_range_distribution);

    printf("\nFloat generation:\n");
    RUN_TEST(rng_float_range_01);
    RUN_TEST(rng_float_range_custom);
    RUN_TEST(rng_float_distribution);

    printf("\nShuffle:\n");
    RUN_TEST(rng_shuffle_permutation);
    RUN_TEST(rng_shuffle_deterministic);
    RUN_TEST(rng_shuffle_different_order);

    printf("\nColor picking:\n");
    RUN_TEST(rng_pick_color_from_mask);
    RUN_TEST(rng_pick_color_single);
    RUN_TEST(rng_pick_color_distribution);

    printf("\nChecksum:\n");
    RUN_TEST(rng_checksum_deterministic);
    RUN_TEST(rng_checksum_changes);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
