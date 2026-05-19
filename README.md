# flux_fracture — Disjoint Linear Algebra for Constraint Systems

Single-header C99 library that fractures constraint systems into independent blocks via BFS connected components on the bipartite constraint-dimension dependency graph.

## Theorem

If fracture correctly identifies connected components of the constraint-dimension dependency graph, coalescence via bitwise OR preserves zero false negatives.

**Proof:** Each constraint violation is a Boolean event. For independent blocks, the event spaces are disjoint (no shared dimensions). The union of all violations = OR of block error masks. QED.

## Usage

```c
#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"
```

### Quick start

```c
/* Define edges: constraint i involves dimension i */
frac_edge edges[8];
for (int i = 0; i < 8; i++)
    edges[i] = (frac_edge){i, i};

/* Fracture */
frac_result result = frac_fracture_from_edges(edges, 8, 8, 8);
printf("Blocks: %d, Largest: %d, Speedup: %.1fx\n",
       result.n_blocks, result.largest_block, result.speedup_potential);

/* Coalesce block results */
uint64_t masks[8] = {1, 0, 1, 0, 1, 0, 1, 0};
frac_coalesce_result cr = frac_coalesce(masks, result.blocks, 8, 8);

/* Verify */
bool ok = frac_coalesce_verify(cr.error_mask, 0x55, 8);

frac_result_free(&result);
```

## API

### Data structures

| Type | Description |
|------|-------------|
| `frac_edge` | Bipartite graph edge: `{constraint_idx, dim_idx}` |
| `frac_block` | Independent block: constraint/dimension index arrays |
| `frac_result` | Fracture result: blocks, stats, speedup potential |
| `frac_coalesce_result` | Coalesced error mask with verification |
| `frac_adjacency` | Bipartite adjacency matrix |
| `frac_delta` | Change between two fracture results |

### Functions

| Function | Description |
|----------|-------------|
| `frac_graph_build()` | Build adjacency from edge list |
| `frac_graph_from_masks()` | Build adjacency from per-constraint dim masks |
| `frac_fracture()` | BFS connected components → independent blocks |
| `frac_fracture_from_edges()` | Convenience: edges → fracture in one call |
| `frac_coalesce()` | Bitwise OR coalescence with index remapping |
| `frac_coalesce_verify()` | Verify zero false negatives |
| `frac_adaptive_refracture()` | Re-fracture with new edges, compute delta |
| `frac_result_free()` | Free all memory |
| `frac_adjacency_free()` | Free adjacency memory |

## Building

```bash
make test    # Compile and run tests
make bench   # Compile and run benchmark
make clean   # Remove build artifacts
```

Requires C99 compiler (gcc or clang). No external dependencies.

## Test results

The test suite validates:

- **Fully independent** (8 constraints → 8 blocks, 8x speedup)
- **Block diagonal** (2 blocks of 4, 2x speedup)
- **Chain** (cyclic overlapping pairs → 1 block, no speedup)
- **Coalescence correctness** (1000 random trials, zero mismatches)
- **Adaptive re-fracture** (progressive merging)
- **Edge cases** (empty, single, orphan dimensions)

## Architecture

```
constraint-dimension bipartite graph
         │
         ▼
    BFS traversal (connected components)
         │
         ▼
    independent blocks ──► parallel solve ──► per-block error masks
         │                                           │
         │              ┌────────────────────────────┘
         ▼              ▼
    frac_coalesce (bitwise OR with index remapping)
         │
         ▼
    unified error mask = monolithic result (proven zero false negatives)
```

## License

Part of the Constraint Theory Ecosystem — Forgemaster ⚒️
