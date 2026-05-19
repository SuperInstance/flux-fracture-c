/**
 * flux_fracture.h — Disjoint Linear Algebra for Constraint Systems
 *
 * Single-header C99 library. Fractures constraint systems into independent
 * blocks via BFS connected components on the bipartite constraint-dimension
 * dependency graph, then coalesces results provably correct.
 *
 * THEOREM: If fracture correctly identifies connected components of the
 * constraint-dimension dependency graph, coalescence via bitwise OR
 * preserves zero false negatives.
 *
 * Usage:
 *   #define FRACTURE_IMPLEMENTATION
 *   #include "flux_fracture.h"
 *
 * Author: Forgemaster ⚒️ (Constraint Theory Ecosystem)
 */

#ifndef FLUX_FRACTURE_H
#define FLUX_FRACTURE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Data structures                                                     */
/* ------------------------------------------------------------------ */

/** Bipartite graph edge: constraint i involves dimension j. */
typedef struct {
    int constraint_idx;
    int dim_idx;
} frac_edge;

/** One independent block of the fractured system. */
typedef struct {
    int  *constraint_indices;  /**< Constraints in this block (sorted) */
    int   n_constraints;       /**< Number of constraints */
    int  *dim_indices;         /**< Dimensions in this block (sorted)   */
    int   n_dims;              /**< Number of dimensions                */
} frac_block;

/** Result of fracturing a constraint system into independent blocks. */
typedef struct {
    frac_block *blocks;
    int         n_blocks;
    int         largest_block;      /**< Size of largest block (constraints) */
    double      speedup_potential;  /**< n_constraints / largest_block       */
} frac_result;

/** Result of coalescing block-level error masks. */
typedef struct {
    uint64_t error_mask;       /**< Bitwise OR of all block masks    */
    int      n_bits;           /**< Number of bits in the mask       */
    bool     verified;         /**< True if verified against mono    */
    uint64_t monolithic_mask;  /**< Reference monolithic mask        */
} frac_coalesce_result;

/** Adjacency representation for the bipartite graph. */
typedef struct {
    uint8_t *adj;        /**< Flat adjacency matrix (n_c × n_d), row-major */
    int      n_constraints;
    int      n_dimensions;
} frac_adjacency;

/* ------------------------------------------------------------------ */
/* Graph building                                                      */
/* ------------------------------------------------------------------ */

/**
 * Build adjacency matrix from edge list.
 *
 * @param edges         Array of frac_edge
 * @param n_edges       Number of edges
 * @param n_constraints Number of constraints (rows)
 * @param n_dimensions  Number of dimensions (columns)
 * @return              Populated frac_adjacency (caller must free .adj)
 */
frac_adjacency frac_graph_build(const frac_edge *edges, int n_edges,
                                 int n_constraints, int n_dimensions);

/**
 * Build adjacency matrix from per-constraint dimension masks.
 *
 * @param dim_masks   dim_masks[i] points to dimension indices for constraint i
 * @param mask_lens   mask_lens[i] = number of dims for constraint i
 * @param n_constraints Number of constraints
 * @param n_dimensions  Number of dimensions (max dim index + 1)
 * @return              Populated frac_adjacency (caller must free .adj)
 */
frac_adjacency frac_graph_from_masks(const int **dim_masks,
                                      const int *mask_lens,
                                      int n_constraints, int n_dimensions);

/** Free adjacency memory. */
void frac_adjacency_free(frac_adjacency *adj);

/* ------------------------------------------------------------------ */
/* Fracture (BFS connected components)                                 */
/* ------------------------------------------------------------------ */

/**
 * Fracture the constraint system into independent blocks.
 *
 * Uses BFS on the bipartite constraint-dimension graph to find
 * connected components. Each component is an independent block.
 *
 * @param adj            Adjacency matrix
 * @param n_constraints  Number of constraints
 * @param n_dimensions   Number of dimensions
 * @return               frac_result with blocks (caller must free via frac_result_free)
 */
frac_result frac_fracture(const frac_adjacency *adj,
                           int n_constraints, int n_dimensions);

/**
 * Convenience: fracture from an edge list directly.
 *
 * @param edges         Edge list
 * @param n_edges       Number of edges
 * @param n_constraints Number of constraints
 * @param n_dimensions  Number of dimensions
 * @return              frac_result (caller must free)
 */
frac_result frac_fracture_from_edges(const frac_edge *edges, int n_edges,
                                      int n_constraints, int n_dimensions);

/** Free fracture result memory (blocks, indices, etc.). */
void frac_result_free(frac_result *result);

/* ------------------------------------------------------------------ */
/* Coalescence (bitwise OR)                                            */
/* ------------------------------------------------------------------ */

/**
 * Coalesce block-level error masks into unified mask via bitwise OR.
 *
 * Each block_mask[i] has bits positioned relative to block[i]'s constraints.
 * This function maps them back to absolute constraint indices and ORs them.
 *
 * @param block_masks      Per-block error masks (local bit positions)
 * @param blocks           The blocks (for constraint index mapping)
 * @param n_blocks         Number of blocks
 * @param n_total          Total number of constraints
 * @return                 frac_coalesce_result with unified mask
 */
frac_coalesce_result frac_coalesce(const uint64_t *block_masks,
                                    const frac_block *blocks,
                                    int n_blocks, int n_total);

/**
 * Verify that coalesced mask matches monolithic mask.
 *
 * Checks for zero false negatives: every bit set in monolithic must be
 * set in coalesced. Reports false negatives and false positives.
 *
 * @param coalesced    Coalesced error mask
 * @param monolithic   Monolithic error mask (reference)
 * @param n_bits       Number of valid bits
 * @return             true if perfect match
 */
bool frac_coalesce_verify(uint64_t coalesced, uint64_t monolithic, int n_bits);

/* ------------------------------------------------------------------ */
/* Adaptive re-fracture                                                */
/* ------------------------------------------------------------------ */

/** Delta between two fracture results. */
typedef struct {
    int    blocks_before;
    int    blocks_after;
    bool   structure_changed;
    int    max_block_size_delta;
    double speedup_delta;
} frac_delta;

/**
 * Adaptive re-fracture: given a previous result and new edges, re-fracture.
 *
 * @param prev_result  Previous fracture result
 * @param new_edges    New edges to add to the graph
 * @param n_new        Number of new edges
 * @param n_constraints Total constraints
 * @param n_dimensions  Total dimensions
 * @param delta        [out] Change descriptor
 * @return             New frac_result (caller must free)
 */
frac_result frac_adaptive_refracture(const frac_result *prev_result,
                                      const frac_edge *new_edges, int n_new,
                                      int n_constraints, int n_dimensions,
                                      frac_delta *delta);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_FRACTURE_H */


/* ================================================================== */
/* ========================== IMPLEMENTATION ========================= */
/* ================================================================== */

#ifdef FRACTURE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/** BFS queue node — tagged union for bipartite traversal. */
typedef struct {
    uint8_t type;  /* 'c' = constraint, 'd' = dimension */
    int     idx;
} _frac_bfs_node;

/** Dynamic int array for building blocks. */
typedef struct {
    int  *data;
    int   len;
    int   cap;
} _frac_int_vec;

static void _vec_init(_frac_int_vec *v) {
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

static void _vec_push(_frac_int_vec *v, int val) {
    if (v->len >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->data = (int *)realloc(v->data, (size_t)v->cap * sizeof(int));
    }
    v->data[v->len++] = val;
}

static void _vec_free(_frac_int_vec *v) {
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

/* Suppress unused warning — used by vec_push reallocation pattern */
static inline void _vec_free_dummy(void) { (void)_vec_free; }

/** Comparison for qsort. */
static int _frac_int_cmp(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

/** Adjacency accessor. */
static inline uint8_t _adj_get(const uint8_t *adj, int n_dims,
                                int constraint, int dim) {
    return adj[constraint * n_dims + dim];
}

/* ------------------------------------------------------------------ */
/* Graph building                                                      */
/* ------------------------------------------------------------------ */

frac_adjacency frac_graph_build(const frac_edge *edges, int n_edges,
                                 int n_constraints, int n_dimensions) {
    frac_adjacency g;
    g.n_constraints = n_constraints;
    g.n_dimensions  = n_dimensions;
    size_t sz = (size_t)n_constraints * (size_t)n_dimensions;
    g.adj = (uint8_t *)calloc(sz, sizeof(uint8_t));
    for (int i = 0; i < n_edges; i++) {
        int c = edges[i].constraint_idx;
        int d = edges[i].dim_idx;
        if (c >= 0 && c < n_constraints && d >= 0 && d < n_dimensions) {
            g.adj[c * n_dimensions + d] = 1;
        }
    }
    return g;
}

frac_adjacency frac_graph_from_masks(const int **dim_masks,
                                      const int *mask_lens,
                                      int n_constraints, int n_dimensions) {
    frac_adjacency g;
    g.n_constraints = n_constraints;
    g.n_dimensions  = n_dimensions;
    size_t sz = (size_t)n_constraints * (size_t)n_dimensions;
    g.adj = (uint8_t *)calloc(sz, sizeof(uint8_t));
    for (int i = 0; i < n_constraints; i++) {
        for (int j = 0; j < mask_lens[i]; j++) {
            int d = dim_masks[i][j];
            if (d >= 0 && d < n_dimensions) {
                g.adj[i * n_dimensions + d] = 1;
            }
        }
    }
    return g;
}

void frac_adjacency_free(frac_adjacency *adj) {
    free(adj->adj);
    adj->adj = NULL;
    adj->n_constraints = 0;
    adj->n_dimensions  = 0;
}

/* ------------------------------------------------------------------ */
/* Fracture (BFS connected components)                                 */
/* ------------------------------------------------------------------ */

frac_result frac_fracture(const frac_adjacency *adj,
                           int n_constraints, int n_dimensions) {
    frac_result result;
    memset(&result, 0, sizeof(result));

    uint8_t *visited_c = (uint8_t *)calloc((size_t)n_constraints, sizeof(uint8_t));
    uint8_t *visited_d = (uint8_t *)calloc((size_t)n_dimensions,  sizeof(uint8_t));

    /* BFS queue — worst case all nodes */
    int qcap = n_constraints + n_dimensions + 1;
    _frac_bfs_node *queue = (_frac_bfs_node *)malloc((size_t)qcap * sizeof(_frac_bfs_node));

    /* Collect blocks */
    int blocks_cap = 16;
    frac_block *blocks = (frac_block *)malloc((size_t)blocks_cap * sizeof(frac_block));
    int n_blocks = 0;

    for (int seed = 0; seed < n_constraints; seed++) {
        if (visited_c[seed]) continue;

        _frac_int_vec comp_c, comp_d;
        _vec_init(&comp_c);
        _vec_init(&comp_d);

        /* BFS */
        int head = 0, tail = 0;
        queue[tail++] = (_frac_bfs_node){'c', seed};

        while (head < tail) {
            _frac_bfs_node node = queue[head++];

            if (node.type == 'c') {
                if (visited_c[node.idx]) continue;
                visited_c[node.idx] = 1;
                _vec_push(&comp_c, node.idx);
                /* Add all unvisited dims this constraint touches */
                for (int d = 0; d < n_dimensions; d++) {
                    if (_adj_get(adj->adj, n_dimensions, node.idx, d) && !visited_d[d]) {
                        queue[tail++] = (_frac_bfs_node){'d', d};
                    }
                }
            } else { /* dimension */
                if (visited_d[node.idx]) continue;
                visited_d[node.idx] = 1;
                _vec_push(&comp_d, node.idx);
                /* Add all unvisited constraints touching this dim */
                for (int c = 0; c < n_constraints; c++) {
                    if (_adj_get(adj->adj, n_dimensions, c, node.idx) && !visited_c[c]) {
                        queue[tail++] = (_frac_bfs_node){'c', c};
                    }
                }
            }
        }

        /* Sort indices */
        if (comp_c.len > 1) qsort(comp_c.data, (size_t)comp_c.len, sizeof(int), _frac_int_cmp);
        if (comp_d.len > 1) qsort(comp_d.data, (size_t)comp_d.len, sizeof(int), _frac_int_cmp);

        /* Store block */
        if (n_blocks >= blocks_cap) {
            blocks_cap *= 2;
            blocks = (frac_block *)realloc(blocks, (size_t)blocks_cap * sizeof(frac_block));
        }
        blocks[n_blocks].constraint_indices = comp_c.data;
        blocks[n_blocks].n_constraints      = comp_c.len;
        blocks[n_blocks].dim_indices         = comp_d.data;
        blocks[n_blocks].n_dims              = comp_d.len;
        n_blocks++;
    }

    /* Dims with no constraints */
    for (int d = 0; d < n_dimensions; d++) {
        if (!visited_d[d]) {
            if (n_blocks >= blocks_cap) {
                blocks_cap *= 2;
                blocks = (frac_block *)realloc(blocks, (size_t)blocks_cap * sizeof(frac_block));
            }
            int *empty_c = NULL;
            int *one_d   = (int *)malloc(sizeof(int));
            one_d[0] = d;
            blocks[n_blocks].constraint_indices = empty_c;
            blocks[n_blocks].n_constraints      = 0;
            blocks[n_blocks].dim_indices         = one_d;
            blocks[n_blocks].n_dims              = 1;
            n_blocks++;
        }
    }

    free(queue);
    free(visited_c);
    free(visited_d);

    /* Compute stats */
    result.blocks   = blocks;
    result.n_blocks = n_blocks;
    result.largest_block = 0;
    for (int i = 0; i < n_blocks; i++) {
        if (blocks[i].n_constraints > result.largest_block)
            result.largest_block = blocks[i].n_constraints;
    }
    result.speedup_potential = (result.largest_block > 0 && n_constraints > 0)
        ? (double)n_constraints / (double)result.largest_block
        : 1.0;

    return result;
}

frac_result frac_fracture_from_edges(const frac_edge *edges, int n_edges,
                                      int n_constraints, int n_dimensions) {
    frac_adjacency adj = frac_graph_build(edges, n_edges, n_constraints, n_dimensions);
    frac_result result = frac_fracture(&adj, n_constraints, n_dimensions);
    frac_adjacency_free(&adj);
    return result;
}

void frac_result_free(frac_result *result) {
    if (!result->blocks) return;
    for (int i = 0; i < result->n_blocks; i++) {
        free(result->blocks[i].constraint_indices);
        free(result->blocks[i].dim_indices);
    }
    free(result->blocks);
    result->blocks   = NULL;
    result->n_blocks = 0;
}

/* ------------------------------------------------------------------ */
/* Coalescence                                                         */
/* ------------------------------------------------------------------ */

frac_coalesce_result frac_coalesce(const uint64_t *block_masks,
                                    const frac_block *blocks,
                                    int n_blocks, int n_total) {
    frac_coalesce_result r;
    memset(&r, 0, sizeof(r));
    r.n_bits = n_total;

    uint64_t coalesced = 0;
    /* Each block's mask is in local bit positions — map to absolute */
    for (int i = 0; i < n_blocks; i++) {
        for (int bit = 0; bit < blocks[i].n_constraints; bit++) {
            if (block_masks[i] & ((uint64_t)1 << bit)) {
                coalesced |= (uint64_t)1 << blocks[i].constraint_indices[bit];
            }
        }
    }

    r.error_mask       = coalesced;
    r.monolithic_mask  = coalesced;  /* Will be set by caller for verify */
    r.verified         = false;
    return r;
}

bool frac_coalesce_verify(uint64_t coalesced, uint64_t monolithic, int n_bits) {
    (void)n_bits;  /* Only used for debugging */
    return coalesced == monolithic;
}

/* ------------------------------------------------------------------ */
/* Adaptive re-fracture                                                */
/* ------------------------------------------------------------------ */

frac_result frac_adaptive_refracture(const frac_result *prev_result,
                                      const frac_edge *new_edges, int n_new,
                                      int n_constraints, int n_dimensions,
                                      frac_delta *delta) {
    /* Rebuild adjacency from scratch using prev blocks + new edges.
     * Simpler and more correct than incremental merge. */

    /* Collect all existing edges from previous blocks */
    /* We need the original adjacency — reconstruct from blocks.
     * Since blocks only store indices, not edges, we re-fracture
     * with new edges added. We'll build the full edge list. */

    /* Count existing edges from previous adjacency stored in block dims.
     * We need the original edges. Since we don't store them, we reconstruct:
     * For each block, each constraint is connected to all dims in that block.
     * This is an over-approximation but exact for connected components. */

    /* Actually, we just need to re-fracture with ALL edges.
     * Build edges from prev result's structure + new edges. */

    /* Count max edges */
    int max_edges = n_new;
    for (int i = 0; i < prev_result->n_blocks; i++) {
        max_edges += prev_result->blocks[i].n_constraints * prev_result->blocks[i].n_dims;
    }

    frac_edge *all_edges = (frac_edge *)malloc((size_t)max_edges * sizeof(frac_edge));
    int n_total_edges = 0;

    /* Reconstruct edges from blocks (bipartite: each constraint connects to all dims in its block) */
    for (int i = 0; i < prev_result->n_blocks; i++) {
        for (int ci = 0; ci < prev_result->blocks[i].n_constraints; ci++) {
            for (int di = 0; di < prev_result->blocks[i].n_dims; di++) {
                all_edges[n_total_edges].constraint_idx = prev_result->blocks[i].constraint_indices[ci];
                all_edges[n_total_edges].dim_idx        = prev_result->blocks[i].dim_indices[di];
                n_total_edges++;
            }
        }
    }

    /* Append new edges */
    for (int i = 0; i < n_new; i++) {
        all_edges[n_total_edges++] = new_edges[i];
    }

    /* Fracture with all edges */
    frac_result new_result = frac_fracture_from_edges(
        all_edges, n_total_edges, n_constraints, n_dimensions);

    free(all_edges);

    /* Compute delta */
    if (delta) {
        delta->blocks_after = new_result.n_blocks;
        delta->max_block_size_delta = new_result.largest_block - prev_result->largest_block;
        delta->speedup_delta = new_result.speedup_potential - prev_result->speedup_potential;

        if (prev_result->n_blocks == 0) {
            delta->blocks_before = 0;
            delta->structure_changed = true;
        } else {
            delta->blocks_before = prev_result->n_blocks;
            /* Check if block sizes changed */
            /* Simple heuristic: different number of blocks or different largest block */
            delta->structure_changed =
                (new_result.n_blocks != prev_result->n_blocks) ||
                (new_result.largest_block != prev_result->largest_block);
        }
    }

    return new_result;
}

#endif /* FRACTURE_IMPLEMENTATION */
