# flux_fracture — Disjoint Linear Algebra for Constraint Systems

Single-header C99 library that fractures constraint systems into independent blocks via BFS connected components on the bipartite constraint-dimension dependency graph.

## How It Works

You have N constraints and M dimensions (variables). Each constraint depends on some subset of dimensions. The key insight: if two constraints share no dimensions, they can be checked independently. Fracture finds those independent groups automatically.

**Step 1 — Build the graph.** Create a bipartite graph: constraints on one side, dimensions on the other. Draw an edge between constraint `i` and dimension `j` if constraint `i` depends on dimension `j`.

**Step 2 — Find connected components.** Run BFS on this graph. Each connected component is a set of constraints that are transitively coupled (they share dimensions, possibly through other constraints). Different components are truly independent.

**Step 3 — Solve in parallel.** Each component becomes an independent block. Solve them separately — in parallel, on different threads, even on different machines.

**Step 4 — Coalesce.** Merge the per-block error masks with bitwise OR. Because the blocks share no dimensions, this is provably identical to checking everything at once. Zero false negatives, guaranteed.

```c
#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"

/* 8 constraints, each touching only its own dimension */
frac_edge edges[8];
for (int i = 0; i < 8; i++)
    edges[i] = (frac_edge){i, i};

/* Fracture into independent blocks */
frac_result result = frac_fracture_from_edges(edges, 8, 8, 8);
/* result.n_blocks == 8, result.speedup_potential == 8.0 */

/* Each block produces an error mask */
uint64_t masks[8] = {1, 0, 1, 0, 1, 0, 1, 0};

/* Coalesce: bitwise OR merges them perfectly */
frac_coalesce_result cr = frac_coalesce(masks, result.blocks, 8, 8);
/* cr.error_mask == 0x55 — constraints 0,2,4,6 violated */

/* Verify it matches monolithic checking */
bool ok = frac_coalesce_verify(cr.error_mask, 0x55, 8);

frac_result_free(&result);
```

### The Theorem

If fracture correctly identifies connected components of the constraint-dimension dependency graph, coalescence via bitwise OR preserves zero false negatives.

**Proof:** Each constraint violation is a Boolean event. For independent blocks, the event spaces are disjoint (no shared dimensions). The union of all violations = OR of block error masks. QED.

## What C Teaches Us

This library is a single header file. That's not laziness — it's a design choice that teaches you something about how constraint systems interact with their host environment:

- **Single-header pattern** — `flux_fracture.h` is both the declaration and the implementation. `#define FRACTURE_IMPLEMENTATION` before including to get the code; omit it to get just the types and function signatures. This means zero build system integration. Drop the file in, compile, done. For a constraint library that might end up embedded in firmware, game engines, or kernel modules, this matters.
- **Preprocessor for zero-cost optional implementation** — The implementation is emitted exactly once, at the translation unit where you define `FRACTURE_IMPLEMENTATION`. Every other file sees only declarations. No linker tricks, no weak symbols, no RTTI.
- **Manual memory management = you see every allocation** — `frac_result_free()` is explicit. There's no destructor hiding behind a scope exit, no garbage collector deciding when to collect. In a real-time constraint system, you need to know exactly when memory is allocated and freed. C makes that visible — `frac_fracture_from_edges()` allocates the block arrays, `frac_result_free()` frees them. That's the complete story.
- **No hidden costs** — No vtable, no reference counting, no exception unwinding tables. The BFS is a flat loop over a flat array. What you see in the source is what runs on the metal.

The constraint system architecture benefits from C's transparency: when checking 8 values against 8 bounds in a tight loop, you can reason exactly about cycles, cache lines, and allocation timing.

## API

### Data Structures

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

## Test Results

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

## Where to Go Next

| Repo | Language | What You'll Learn |
|------|----------|-------------------|
| [flux-fracture](https://github.com/SuperInstance/flux-fracture) | Rust | Same algorithm with ownership model, zero-cost generics, and parallel iterators |
| [flux-check-js](https://github.com/SuperInstance/flux-check-js) | TypeScript | Full engine with fracture + sediment + industry presets |
| [flux-engine-c](https://github.com/SuperInstance/flux-engine-c) | C | Combined engine: check + fracture + sediment in one header |
| [plato-types](https://github.com/SuperInstance/plato-types) | Python | Tile lifecycle and Lamport clocks for fleet coordination |

## License

Part of the Constraint Theory Ecosystem — Forgemaster ⚒️
