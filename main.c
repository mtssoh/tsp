#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#include "parser.h"
#include "ga.h"

/* =========================================================================
   PARAMETROS DEL GA
   ========================================================================= */

#define POP_SIZE    500
#define GENERATIONS 1000
#define ELITE       ELITE_COUNT

/* =========================================================================
   BENCHMARK: ejecuta una estrategia, mide tiempo e imprime resultados
   ========================================================================= */

static void print_row(const char *label, double fitness,
                      double t, double t_seq, int nthreads) {
    if (t_seq < 0) {
        /* fila de referencia (secuencial) */
        printf("  %-30s  %10.2f  %8.3f s    ---      ---\n",
               label, fitness, t);
    } else {
        double speedup    = t_seq / t;
        double efficiency = 100.0 * speedup / nthreads;
        printf("  %-30s  %10.2f  %8.3f s  %5.2fx   %5.1f%%\n",
               label, fitness, t, speedup, efficiency);
    }
}

/* =========================================================================
   MAIN
   ========================================================================= */

int main(int argc, char *argv[]) {
    const char *filename = argc > 1 ? argv[1] : "data/berlin52.tsp";
    int nthreads          = argc > 2 ? atoi(argv[2]) : 4;

    if (!load_tsp(filename))  return 1;
    build_dist_matrix();
    ga_seed((unsigned int)time(NULL));

    printf("\n=== TSP — Algoritmo Genetico + OpenMP ===\n");
    printf("  Instancia    : %s\n", filename);
    printf("  Ciudades     : %d\n", N);
    printf("  Poblacion    : %d\n", POP_SIZE);
    printf("  Generaciones : %d\n", GENERATIONS);
    printf("  Hilos        : %d\n\n", nthreads);

    printf("  %-30s  %10s  %10s  %7s  %8s\n",
           "Estrategia", "Mejor tour", "Tiempo", "Speedup", "Eficiencia");
    printf("  %s\n",
           "------------------------------------------------------------------------");

    double t0, t1;
    Individual best;

    /* --- Secuencial (referencia) --- */
    t0   = omp_get_wtime();
    best = ga_run_sequential(POP_SIZE, GENERATIONS, ELITE);
    t1   = omp_get_wtime();
    double t_seq = t1 - t0;
    print_row("Secuencial", best.fitness, t_seq, -1, nthreads);
    free(best.tour);

    /* --- Paralela: schedule(static) --- */
    t0   = omp_get_wtime();
    best = ga_run_parallel_static(POP_SIZE, GENERATIONS, ELITE, nthreads);
    t1   = omp_get_wtime();
    print_row("Paralela schedule(static)", best.fitness, t1-t0, t_seq, nthreads);
    free(best.tour);

    /* --- Paralela: schedule(dynamic) --- */
    t0   = omp_get_wtime();
    best = ga_run_parallel_dynamic(POP_SIZE, GENERATIONS, ELITE, nthreads);
    t1   = omp_get_wtime();
    print_row("Paralela schedule(dynamic)", best.fitness, t1-t0, t_seq, nthreads);
    free(best.tour);

    /* --- Modelo de islas --- */
    /* Cada isla tiene POP_SIZE/nthreads individuos para que la
       poblacion total sea comparable con las otras estrategias */
    t0   = omp_get_wtime();
    best = ga_run_island_model(POP_SIZE / nthreads, GENERATIONS, nthreads);
    t1   = omp_get_wtime();
    print_row("Modelo de Islas", best.fitness, t1-t0, t_seq, nthreads);
    free(best.tour);

    printf("\n");
    free(cities);
    free(dist);
    return 0;
}
