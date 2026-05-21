# flux-fracture-c

## Fracture propagation in constraint systems

Imagine a sheet of glass with a crack. When stress is applied, the crack propagates along paths determined by the stress field. The crack doesn't split the glass randomly — it follows the lines of least resistance, separating the sheet into independent pieces that can be analyzed separately.

This library does the same thing for constraint systems. A constraint system is a set of equations where each equation involves some subset of variables. If you build a graph where constraints and variables are nodes, and edges connect each constraint to the variables it involves, the connected components of this graph are independent sub-problems. You can solve each one separately and combine the results.

This is *fracture*: splitting a large constraint system into independent blocks.

## The bipartite graph

A constraint system has two kinds of things: constraints (equations) and dimensions (variables). Constraint 0 might involve dimensions 1 and 3. Constraint 1 might involve dimensions 2 and 4. If constraint 0 and constraint 1 share no dimensions, they're independent — you can solve them in parallel.

The bipartite graph captures this:
- Left nodes: constraints (C₀, C₁, C₂, ...)
- Right nodes: dimensions (D₀, D₁, D₂, ...)
- Edges: constraint Cᵢ involves dimension Dⱼ

Connected components of this graph = independent blocks.

## Walk through an example

Consider 4 constraints on 4 dimensions:

```
C₀ involves D₀, D₁       (coupled pair)
C₁ involves D₀, D₁       (same pair)
C₂ involves D₂, D₃       (different pair)
C₃ involves D₂, D₃       (same pair)
```

The bipartite graph:

```
C₀ ── D₀     C₂ ── D₂
  ╲ ╱           ╲ ╱
   ╳             ╳
  ╱ ╲           ╱ ╲
C₁ ── D₁     C₃ ── D₃
```

Two connected components: {C₀, C₁, D₀, D₁} and {C₂, C₃, D₂, D₃}.

The fracture algorithm finds these components via BFS. You can solve the two blocks in parallel. The speedup potential is `n_constraints / largest_block = 4 / 2 = 2×`.

## The algorithm (BFS connected components)

1. Build adjacency matrix from the edge list
2. For each unvisited constraint node, start a BFS
3. BFS alternates between constraint and dimension nodes (bipartite traversal)
4. Each BFS tree is one connected component = one independent block
5. Sort indices within each block for deterministic output

Time complexity: O(C × D) for the adjacency matrix, O(C + D) for BFS.

## Coalescence

After solving each block independently, you need to combine the results. Each block produces an error mask (which constraints are violated). Coalescence maps local bit positions back to global constraint indices and combines them via bitwise OR.

```
Block 0 mask: 0b01 (constraint 0 violated)
Block 1 mask: 0b10 (constraint 2 violated)
Combined:     0b0101 (constraints 0 and 2 violated)
```

**Theorem**: If fracture correctly identifies connected components, coalescence via bitwise OR has zero false negatives. Every violation in the monolithic solution appears in the coalesced result.

## Use it

Single-header C99 library. Define `FRACTURE_IMPLEMENTATION` in exactly one translation unit:

```c
#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"
```

Build a constraint system and fracture it:

```c
// Define which constraints touch which dimensions
frac_edge edges[] = {
    {0, 0}, {0, 1},  // C₀ → D₀, D₁
    {1, 0}, {1, 1},  // C₁ → D₀, D₁
    {2, 2}, {2, 3},  // C₂ → D₂, D₃
    {3, 2}, {3, 3},  // C₃ → D₂, D₃
};

frac_result result = frac_fracture_from_edges(edges, 8, 4, 4);

printf("Blocks: %d\n", result.n_blocks);          // 2
printf("Speedup: %.1f×\n", result.speedup_potential); // 2.0×

// Solve each block independently, then coalesce
uint64_t masks[] = {0b01, 0b10}; // per-block results
frac_coalesce_result cr = frac_coalesce(masks, result.blocks, 2, 4);
printf("Error mask: 0x%lx\n", cr.error_mask);     // 0x5

frac_result_free(&result);
```

## Adaptive re-fracture

Constraints change at runtime — new equations appear, old ones expire. `frac_adaptive_refracture` takes a previous fracture result and a set of new edges, rebuilds the graph, and re-fractures:

```c
frac_edge new_edges[] = {{1, 2}};  // now C₁ also involves D₂
frac_delta delta;
frac_result new_result = frac_adaptive_refracture(
    &result, new_edges, 1, 4, 4, &delta
);
printf("Structure changed: %s\n", delta.structure_changed ? "yes" : "no");
```

## Test cases

The test suite covers 4 dependency structures plus coalescence and adaptive re-fracture:

| Test | Structure | Blocks | Speedup |
|------|-----------|--------|---------|
| A | Fully independent (8 constraints, 8 dims) | 8 | 8× |
| B | Block diagonal (2 blocks of 4) | 2 | 2× |
| C | Chain (overlapping pairs) | 1 | limited |
| D | Fully connected | 1 | 1× (no parallelism) |

Plus coalescence verification (zero false negatives) and adaptive re-fracture (structure change detection).

Build and run tests:

```bash
make test
```

## Why does this work?

Because connected components partition the graph into truly independent pieces. Two constraints in different components share no variables — they *cannot* affect each other's solution. Solving them together gives the same answer as solving them separately. The coalescence theorem (bitwise OR preserves all violations) guarantees you don't lose any information by splitting.

The BFS algorithm is optimal for this problem — you must visit every node at least once to determine which component it belongs to, and BFS does exactly that with O(C + D) work.

## API reference

| Function | What it does |
|----------|-------------|
| `frac_graph_build` | Build adjacency matrix from edge list |
| `frac_fracture` | BFS connected components → independent blocks |
| `frac_coalesce` | Combine per-block error masks via bitwise OR |
| `frac_coalesce_verify` | Check coalesced mask matches monolithic |
| `frac_adaptive_refracture` | Re-fracture after adding new edges |
| `frac_result_free` | Free all allocated memory |

## License

MIT
