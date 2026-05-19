/**
 * test_fracture.c — Tests for flux_fracture.h
 *
 * Tests the full fracture-coalesce pipeline across 4 dependency structures:
 *   A) Fully independent (8 blocks)  → perfect parallelism
 *   B) Block diagonal (2 blocks)     → 2-way parallel
 *   C) Chain (overlapping pairs)     → limited parallelism
 *   D) Fully connected (1 block)    → no parallelism
 *
 * Plus coalescence verification and adaptive re-fracture.
 */

#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

/* Helper: build identity adjacency (constraint i → dimension i) */
static frac_adjacency make_identity(int n) {
    frac_edge *edges = malloc((size_t)n * sizeof(frac_edge));
    for (int i = 0; i < n; i++) {
        edges[i].constraint_idx = i;
        edges[i].dim_idx = i;
    }
    frac_adjacency adj = frac_graph_build(edges, n, n, n);
    free(edges);
    return adj;
}

/* Helper: fully connected adjacency */
static frac_adjacency make_fully_connected(int nc, int nd) {
    frac_edge *edges = malloc((size_t)(nc * nd) * sizeof(frac_edge));
    int k = 0;
    for (int i = 0; i < nc; i++)
        for (int j = 0; j < nd; j++)
            edges[k++] = (frac_edge){i, j};
    frac_adjacency adj = frac_graph_build(edges, nc * nd, nc, nd);
    free(edges);
    return adj;
}

/* ------------------------------------------------------------------ */
/* Test A: Fully independent — 8 constraints, each on its own dim      */
/* ------------------------------------------------------------------ */
static void test_independent(void) {
    printf("Test A: Fully independent (8 blocks)\n");

    frac_adjacency adj = make_identity(8);
    frac_result result = frac_fracture(&adj, 8, 8);

    ASSERT(result.n_blocks == 8, "8 independent blocks");
    ASSERT(result.largest_block == 1, "largest block = 1");
    ASSERT(result.speedup_potential == 8.0, "speedup = 8x");

    /* Each block should have exactly 1 constraint and 1 dimension */
    for (int i = 0; i < result.n_blocks; i++) {
        ASSERT(result.blocks[i].n_constraints == 1, "block has 1 constraint");
        ASSERT(result.blocks[i].n_dims == 1, "block has 1 dimension");
    }

    /* Coalescence test: each block sees constraint 0 violated */
    uint64_t *masks = malloc((size_t)result.n_blocks * sizeof(uint64_t));
    for (int i = 0; i < result.n_blocks; i++) masks[i] = 1; /* bit 0 = violated */
    frac_coalesce_result cr = frac_coalesce(masks, result.blocks, result.n_blocks, 8);
    /* Each block's constraint 0 maps to a different absolute index */
    uint64_t expected = 0xFF; /* all 8 constraints violated */
    ASSERT(cr.error_mask == expected, "coalesced mask = 0xFF");
    ASSERT(frac_coalesce_verify(cr.error_mask, expected, 8), "verify passes");
    free(masks);

    frac_adjacency_free(&adj);
    frac_result_free(&result);
    printf("  done.\n");
}

/* ------------------------------------------------------------------ */
/* Test B: Block diagonal — 2 blocks of 4                              */
/* ------------------------------------------------------------------ */
static void test_block_diagonal(void) {
    printf("Test B: Block diagonal (2 blocks of 4)\n");

    /* Block 1: constraints 0-3 → dims 0-3
     * Block 2: constraints 4-7 → dims 4-7 */
    frac_edge edges[32];
    int k = 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            edges[k++] = (frac_edge){i, j};
    for (int i = 4; i < 8; i++)
        for (int j = 4; j < 8; j++)
            edges[k++] = (frac_edge){i, j};

    frac_adjacency adj = frac_graph_build(edges, k, 8, 8);
    frac_result result = frac_fracture(&adj, 8, 8);

    ASSERT(result.n_blocks == 2, "2 blocks");
    ASSERT(result.largest_block == 4, "largest block = 4");
    ASSERT(result.speedup_potential == 2.0, "speedup = 2x");

    /* Verify each block has 4 constraints and 4 dimensions */
    for (int i = 0; i < result.n_blocks; i++) {
        ASSERT(result.blocks[i].n_constraints == 4, "block has 4 constraints");
        ASSERT(result.blocks[i].n_dims == 4, "block has 4 dimensions");
    }

    /* Coalescence: violate all constraints in block 0, none in block 1 */
    uint64_t masks[2] = {0xF, 0x0}; /* block 0: bits 0-3 set */
    frac_coalesce_result cr = frac_coalesce(masks, result.blocks, 2, 8);
    /* Block 0 constraints are 0-3, so coalesced = 0xF */
    ASSERT(cr.error_mask == 0xF, "coalesced = 0xF (first 4 constraints)");
    ASSERT(frac_coalesce_verify(cr.error_mask, 0xF, 8), "verify passes");

    frac_adjacency_free(&adj);
    frac_result_free(&result);
    printf("  done.\n");
}

/* ------------------------------------------------------------------ */
/* Test C: Chain (cyclic overlapping pairs) — 1 block                  */
/* ------------------------------------------------------------------ */
static void test_chain(void) {
    printf("Test C: Chain (cyclic pairs → 1 block)\n");

    /* c0→d0,d1  c1→d1,d2  c2→d2,d3  ... c7→d7,d0 */
    frac_edge edges[16];
    int k = 0;
    for (int i = 0; i < 8; i++) {
        edges[k++] = (frac_edge){i, i};
        edges[k++] = (frac_edge){i, (i + 1) % 8};
    }

    frac_adjacency adj = frac_graph_build(edges, k, 8, 8);
    frac_result result = frac_fracture(&adj, 8, 8);

    ASSERT(result.n_blocks == 1, "1 block (fully connected via chain)");
    ASSERT(result.largest_block == 8, "largest block = 8");
    ASSERT(result.speedup_potential == 1.0, "speedup = 1x (no parallelism)");

    frac_adjacency_free(&adj);
    frac_result_free(&result);
    printf("  done.\n");
}

/* ------------------------------------------------------------------ */
/* Test D: Coalescence preserves correctness (statistical)             */
/* ------------------------------------------------------------------ */
static void test_coalescence_correctness(void) {
    printf("Test D: Coalescence correctness (statistical, 1000 trials)\n");

    /* Use independent structure for easy verification */
    frac_adjacency adj = make_identity(8);
    frac_result result = frac_fracture(&adj, 8, 8);

    int matches = 0, mismatches = 0;
    /* Simple LCG PRNG — deterministic */
    unsigned long rng = 42;

    for (int trial = 0; trial < 1000; trial++) {
        /* Generate monolithic mask: random violation pattern */
        uint64_t mono = 0;
        for (int i = 0; i < 8; i++) {
            rng = rng * 1103515245 + 12345;
            if ((rng >> 16) % 3 == 0) { /* ~1/3 chance of violation */
                mono |= (uint64_t)1 << i;
            }
        }

        /* Build per-block masks from monolithic */
        uint64_t *block_masks = malloc((size_t)result.n_blocks * sizeof(uint64_t));
        for (int i = 0; i < result.n_blocks; i++) {
            block_masks[i] = 0;
            for (int bit = 0; bit < result.blocks[i].n_constraints; bit++) {
                int abs_idx = result.blocks[i].constraint_indices[bit];
                if (mono & ((uint64_t)1 << abs_idx)) {
                    block_masks[i] |= (uint64_t)1 << bit;
                }
            }
        }

        frac_coalesce_result cr = frac_coalesce(block_masks, result.blocks,
                                                  result.n_blocks, 8);
        if (frac_coalesce_verify(cr.error_mask, mono, 8)) {
            matches++;
        } else {
            mismatches++;
        }
        free(block_masks);
    }

    ASSERT(matches == 1000, "all 1000 trials match");
    ASSERT(mismatches == 0, "zero mismatches");

    frac_adjacency_free(&adj);
    frac_result_free(&result);
    printf("  matches=%d mismatches=%d\n", matches, mismatches);
}

/* ------------------------------------------------------------------ */
/* Test E: Adaptive re-fracture                                        */
/* ------------------------------------------------------------------ */
static void test_adaptive_refracture(void) {
    printf("Test E: Adaptive re-fracture\n");

    /* Start with 4 independent constraints */
    frac_edge edges[4] = {
        {0, 0}, {1, 1}, {2, 2}, {3, 3}
    };
    frac_result r1 = frac_fracture_from_edges(edges, 4, 4, 4);
    ASSERT(r1.n_blocks == 4, "initial: 4 independent blocks");
    ASSERT(r1.largest_block == 1, "initial: largest = 1");

    /* Add edge connecting constraint 0 and 1 via shared dim 0 */
    /* Actually, add edge (1, 0) — constraint 1 now touches dim 0 too */
    frac_edge new_edges[1] = {{1, 0}};
    frac_delta delta;
    frac_result r2 = frac_adaptive_refracture(&r1, new_edges, 1, 4, 4, &delta);

    ASSERT(delta.structure_changed == true, "structure changed");
    ASSERT(r2.n_blocks == 3, "after merge: 3 blocks (c0+c1 merged)");
    ASSERT(r2.largest_block == 2, "largest block now 2");

    /* Add another edge to connect constraint 2 to dim 0 */
    frac_edge more_edges[1] = {{2, 0}};
    frac_delta delta2;
    frac_result r3 = frac_adaptive_refracture(&r2, more_edges, 1, 4, 4, &delta2);
    ASSERT(delta2.structure_changed == true, "structure changed again");
    ASSERT(r3.n_blocks == 2, "after 2nd merge: 2 blocks");
    ASSERT(r3.largest_block == 3, "largest block now 3");

    frac_result_free(&r1);
    frac_result_free(&r2);
    frac_result_free(&r3);
    printf("  done.\n");
}

/* ------------------------------------------------------------------ */
/* Test F: Edge cases                                                  */
/* ------------------------------------------------------------------ */
static void test_edge_cases(void) {
    printf("Test F: Edge cases\n");

    /* Empty system */
    frac_result r0 = frac_fracture_from_edges(NULL, 0, 0, 0);
    ASSERT(r0.n_blocks == 0, "empty system: 0 blocks");
    frac_result_free(&r0);

    /* Single constraint, single dimension */
    frac_edge e1 = {0, 0};
    frac_result r1 = frac_fracture_from_edges(&e1, 1, 1, 1);
    ASSERT(r1.n_blocks == 1, "single: 1 block");
    ASSERT(r1.largest_block == 1, "single: largest = 1");
    frac_result_free(&r1);

    /* Dimensions with no constraints */
    frac_edge e2 = {0, 0};
    frac_result r2 = frac_fracture_from_edges(&e2, 1, 1, 3);
    ASSERT(r2.n_blocks == 3, "1 constraint + 2 orphan dims = 3 blocks");
    frac_result_free(&r2);

    printf("  done.\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    printf("============================================================\n");
    printf("FLUX FRACTURE — C Test Suite\n");
    printf("============================================================\n\n");

    test_independent();
    test_block_diagonal();
    test_chain();
    test_coalescence_correctness();
    test_adaptive_refracture();
    test_edge_cases();

    printf("\n============================================================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED ✓\n");
    } else {
        printf("SOME TESTS FAILED ✗\n");
    }
    printf("============================================================\n");

    return g_fail > 0 ? 1 : 0;
}
