# flux-fracture-c

Single-header C99 library for constraint system fracture-coalesce analysis. Decomposes constraint dependency graphs into independent blocks via BFS connected components, then coalesces block-level error masks via bitwise OR with provable correctness.

## What It Does

Given a set of constraints and the dimensions they depend on, `flux_fracture.h` finds connected components in the bipartite constraint-dimension graph. Each component is an independent block that can be checked in parallel. Block-level error masks are then coalesced into a unified mask.

**Theorem:** If fracture correctly identifies connected components, coalescence via bitwise OR preserves zero false negatives.

## Usage

```c
#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"
```

Single header — include with `FRACTURE_IMPLEMENTATION` defined in exactly one translation unit.

### Fracture from Edges

```c
frac_edge edges[] = {
    {0, 0}, {0, 1},  // constraint 0 touches dimensions 0 and 1
    {1, 0},          // constraint 1 touches dimension 0
    {2, 2},          // constraint 2 touches dimension 2 (independent)
    {3, 2},          // constraint 3 touches dimension 2
};

frac_result result = frac_fracture_from_edges(edges, 5, 4, 3);
// result.n_blocks == 2
// Block 0: constraints {0, 1}, dims {0, 1}
// Block 1: constraints {2, 3}, dims {2}
// result.speedup_potential == 2.0
```

### Fracture from Dimension Masks

```c
const int masks[] = {0, 1};      // constraint 0 → dims [0, 1]
const int mask_lens[] = {2};
const int *mask_ptrs[] = {masks};

frac_adjacency adj = frac_graph_from_masks(mask_ptrs, mask_lens, 1, 2);
frac_result result = frac_fracture(&adj, 1, 2);
frac_adjacency_free(&adj);
```

### Coalescence

```c
uint64_t block_masks[] = {0x3, 0x0};  // block 0: bits 0-1 violated
frac_coalesce_result cr = frac_coalesce(block_masks, result.blocks, 2, 4);
// cr.error_mask contains the unified mask with absolute bit positions

bool ok = frac_coalesce_verify(cr.error_mask, reference_mask, 4);
```

### Adaptive Re-Fracture

```c
frac_edge new_edges[] = {{1, 2}};  // add constraint 1 → dim 2
frac_delta delta;
frac_result updated = frac_adaptive_refracture(
    &result, new_edges, 1, 4, 3, &delta
);
// delta.structure_changed == true if blocks merged/split
```

## API

### Data Structures

| Type | Description |
|------|-------------|
| `frac_edge` | Bipartite graph edge: `{constraint_idx, dim_idx}` |
| `frac_block` | Independent block: sorted constraint + dimension indices |
| `frac_result` | Fracture output: array of blocks with stats |
| `frac_adjacency` | Flat row-major adjacency matrix |
| `frac_coalesce_result` | Coalesced error mask with verification |
| `frac_delta` | Change descriptor for adaptive re-fracture |

### Functions

| Function | Description |
|----------|-------------|
| `frac_graph_build(edges, n, nc, nd)` | Build adjacency from edge list |
| `frac_graph_from_masks(masks, lens, nc, nd)` | Build adjacency from per-constraint dim masks |
| `frac_adjacency_free(adj)` | Free adjacency memory |
| `frac_fracture(adj, nc, nd)` | BFS connected components → blocks |
| `frac_fracture_from_edges(edges, n, nc, nd)` | Convenience: build + fracture |
| `frac_result_free(result)` | Free all block memory |
| `frac_coalesce(masks, blocks, nb, total)` | Bitwise OR coalescence |
| `frac_coalesce_verify(coalesced, mono, bits)` | Verify zero false negatives |
| `frac_adaptive_refracture(prev, edges, n, nc, nd, delta)` | Incremental re-fracture |

### Memory Management

All returned structures own their memory. Call `frac_result_free()` and `frac_adjacency_free()` when done. `frac_adaptive_refracture()` returns a new result (does not modify the previous one).

## Building

```bash
make test   # Build and run test suite
make bench  # Build and run benchmarks
make clean
```

Requires a C99 compiler. No external dependencies.

## Test Suite

6 test groups covering:
- **A)** Fully independent constraints (8 blocks, 8× speedup)
- **B)** Block diagonal (2 blocks, 2× speedup)
- **C)** Cyclic chain (1 block, no parallelism)
- **D)** Statistical coalescence correctness (1000 random trials)
- **E)** Adaptive re-fracture (incremental merging)
- **F)** Edge cases (empty, single, orphan dimensions)

## Related Repos

- **[flux-check-py](https://github.com/SuperInstance/flux-check-py)** — Python CLI with this fracture logic built in
- **[constraint-theory-rust-python](https://github.com/SuperInstance/constraint-theory-rust-python)** — Rust engine with PyO3 Python bindings
- **[constraint-theory-engine-cpp-lua](https://github.com/SuperInstance/constraint-theory-engine-cpp-lua)** — C++ engine with LuaJIT, CDCL solver, AVX-512

## License

MIT
