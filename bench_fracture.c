/**
 * bench_fracture.c — Simple benchmark for flux_fracture
 */

#define _POSIX_C_SOURCE 199309L
#define FRACTURE_IMPLEMENTATION
#include "flux_fracture.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void bench_build_and_fracture(int n) {
    /* Build a chain: constraint i → dims i and (i+1)%n */
    frac_edge *edges = malloc((size_t)(n * 2) * sizeof(frac_edge));
    for (int i = 0; i < n; i++) {
        edges[2*i]     = (frac_edge){i, i};
        edges[2*i + 1] = (frac_edge){i, (i + 1) % n};
    }

    double t0 = now_sec();
    int iters = 1000;
    frac_result last;
    for (int i = 0; i < iters; i++) {
        frac_result r = frac_fracture_from_edges(edges, n * 2, n, n);
        if (i == iters - 1) last = r;
        else frac_result_free(&r);
    }
    double t1 = now_sec();

    double per = (t1 - t0) / iters * 1e6; /* microseconds */
    printf("  n=%5d  chain: %.1f µs/iter  blocks=%d  largest=%d\n",
           n, per, last.n_blocks, last.largest_block);

    free(edges);
    frac_result_free(&last);
}

int main(void) {
    printf("=== flux_fracture benchmark ===\n");

    int sizes[] = {8, 32, 64, 128, 256, 512, 1024};
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < nsizes; i++) {
        bench_build_and_fracture(sizes[i]);
    }

    printf("=== done ===\n");
    return 0;
}
