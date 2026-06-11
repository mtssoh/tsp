#include "ga.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

/* =========================================================================
   NUMEROS ALEATORIOS
   rand_r() es la version thread-safe de rand(): en vez de usar una semilla
   global compartida, cada hilo tiene la suya propia.
   ========================================================================= */

static unsigned int global_seed;

/* Una semilla por hilo (soporta hasta 64 hilos) */
static unsigned int thread_seeds[64];

void ga_seed(unsigned int seed) {
    global_seed = seed;
    srand(seed);
}

static int rand_between(int min, int max, unsigned int *seed) {
    return min + (int)(rand_r(seed) % (unsigned)(max - min));
}

/* =========================================================================
   FITNESS
   ========================================================================= */

static double tour_length(int *tour) {
    double total = 0.0;
    for (int i = 0; i < N; i++)
        total += DIST(tour[i], tour[(i + 1) % N]);
    return total;
}

/* =========================================================================
   OPERADORES DEL GA (reciben semilla para ser thread-safe)
   ========================================================================= */

/* Fisher-Yates: genera una permutacion aleatoria uniforme */
static void shuffle(int *arr, int n, unsigned int *seed) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand_between(0, i + 1, seed);
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

/* Torneo: elige k individuos al azar, devuelve el indice del mejor */
static int tournament(Individual *pop, int pop_size, int k, unsigned int *seed) {
    int best = rand_between(0, pop_size, seed);
    for (int i = 1; i < k; i++) {
        int c = rand_between(0, pop_size, seed);
        if (pop[c].fitness < pop[best].fitness)
            best = c;
    }
    return best;
}

/* Cruzamiento de 1 punto para permutaciones:
   se copia el prefijo [0..cut-1] de p1 al hijo; las ciudades restantes
   se toman de p2 en el orden en que aparecen, saltando las ya usadas.
   Garantiza que el hijo sea siempre un tour valido (sin repetidos). */
static void crossover(int *p1, int *p2, int *child, unsigned int *seed) {
    int cut = rand_between(1, N, seed);

    char used[4096];
    memset(used, 0, N);

    /* Copia el prefijo de p1 */
    for (int i = 0; i < cut; i++) {
        child[i]    = p1[i];
        used[p1[i]] = 1;
    }

    /* Rellena el resto con las ciudades de p2 que faltan */
    int pos = cut;
    for (int i = 0; i < N; i++) {
        if (!used[p2[i]]) {
            child[pos++] = p2[i];
        }
    }
}

/* Mutacion por reemplazo: con probabilidad MUTATION_RATE, reemplaza la
   ciudad en la posicion i por una ciudad aleatoria j. Como el tour es
   una permutacion, el reemplazo se implementa como un intercambio de
   posiciones para no generar ciudades repetidas. */
static void mutate(int *tour, unsigned int *seed) {
    for (int i = 0; i < N; i++) {
        double p = (double)rand_r(seed) / RAND_MAX;
        if (p < MUTATION_RATE) {
            int j = rand_between(0, N, seed);
            int tmp = tour[i]; tour[i] = tour[j]; tour[j] = tmp;
        }
    }
}

/* =========================================================================
   POBLACION
   ========================================================================= */

Individual *init_population(int size) {
    Individual *pop = malloc(size * sizeof(Individual));
    if (!pop) { perror("malloc"); exit(1); }

    for (int i = 0; i < size; i++) {
        pop[i].tour = malloc(N * sizeof(int));
        if (!pop[i].tour) { perror("malloc"); exit(1); }

        for (int j = 0; j < N; j++) pop[i].tour[j] = j;
        shuffle(pop[i].tour, N, &global_seed);
        pop[i].fitness = tour_length(pop[i].tour);
    }
    return pop;
}

void free_population(Individual *pop, int size) {
    for (int i = 0; i < size; i++) free(pop[i].tour);
    free(pop);
}

static int cmp_fitness(const void *a, const void *b) {
    double fa = ((Individual *)a)->fitness;
    double fb = ((Individual *)b)->fitness;
    if (fa < fb) return -1;
    if (fa > fb) return  1;
    return 0;
}

void sort_population(Individual *pop, int size) {
    qsort(pop, size, sizeof(Individual), cmp_fitness);
}

/* =========================================================================
   UNA GENERACION (interna, usada por todas las estrategias)
   Toma la poblacion actual (pop) y produce la nueva generacion en (buf).
   ========================================================================= */

static void one_generation(Individual *pop, Individual *buf,
                            int pop_size, int elite,
                            unsigned int *seed) {
    sort_population(pop, pop_size);

    /* Elitismo: los mejores 'elite' pasan sin cambios */
    for (int i = 0; i < elite; i++) {
        memcpy(buf[i].tour, pop[i].tour, N * sizeof(int));
        buf[i].fitness = pop[i].fitness;
    }

    /* Generamos el resto: seleccion -> crossover -> mutacion */
    for (int i = elite; i < pop_size; i++) {
        int p1 = tournament(pop, pop_size, TOURNAMENT_K, seed);
        int p2 = tournament(pop, pop_size, TOURNAMENT_K, seed);
        crossover(pop[p1].tour, pop[p2].tour, buf[i].tour, seed);
        mutate(buf[i].tour, seed);
        buf[i].fitness = tour_length(buf[i].tour);
    }
}

/* Copia el mejor individuo de pop[] a best */
static void get_best(Individual *pop, int pop_size, Individual *best) {
    sort_population(pop, pop_size);
    best->tour = malloc(N * sizeof(int));
    memcpy(best->tour, pop[0].tour, N * sizeof(int));
    best->fitness = pop[0].fitness;
}

/* =========================================================================
   ESTRATEGIA SECUENCIAL
   ========================================================================= */

Individual ga_run_sequential(int pop_size, int generations, int elite) {
    Individual *pop = init_population(pop_size);
    Individual *buf = init_population(pop_size);
    unsigned int seed = global_seed;

    for (int g = 0; g < generations; g++) {
        one_generation(pop, buf, pop_size, elite, &seed);

        /* Intercambiamos pop y buf: buf pasa a ser la nueva poblacion */
        Individual *tmp = pop;
        pop = buf;
        buf = tmp;
    }

    Individual best;
    get_best(pop, pop_size, &best);
    free_population(pop, pop_size);
    free_population(buf, pop_size);
    return best;
}

/* =========================================================================
   ESTRATEGIA 1 — GENERACION PARALELA DE HIJOS
   La parte mas costosa de cada generacion (crear cada hijo) se reparte
   entre hilos con #pragma omp parallel for.

   Tecnica de balanceo A — schedule(static):
     Divide las iteraciones en bloques iguales antes de empezar.
     Ejemplo con 4 hilos y 200 hijos: cada hilo genera 50.

   Tecnica de balanceo B — schedule(dynamic):
     Asigna iteraciones de a 1 bajo demanda. Si un hilo termina antes,
     toma la proxima iteracion disponible. Util cuando el costo por
     iteracion varia.
   ========================================================================= */

static Individual run_parallel(int pop_size, int generations, int elite,
                                int nthreads, int use_dynamic) {
    Individual *pop = init_population(pop_size);
    Individual *buf = init_population(pop_size);

    /* Cada hilo tiene su propia semilla para evitar condiciones de carrera */
    for (int t = 0; t < nthreads; t++)
        thread_seeds[t] = global_seed + (unsigned int)(t + 1) * 1000;

    for (int g = 0; g < generations; g++) {
        sort_population(pop, pop_size);

        /* Elitismo (secuencial: solo son 'elite' copias) */
        for (int i = 0; i < elite; i++) {
            memcpy(buf[i].tour, pop[i].tour, N * sizeof(int));
            buf[i].fitness = pop[i].fitness;
        }

        /* Cada hijo es independiente: no comparte escritura con otros hilos.
           pop[] solo se lee, buf[i] solo lo escribe el hilo que toma i. */
        if (use_dynamic) {
            /* schedule(dynamic): asignacion bajo demanda */
            #pragma omp parallel for schedule(dynamic, 1) num_threads(nthreads)
            for (int i = elite; i < pop_size; i++) {
                unsigned int *seed = &thread_seeds[omp_get_thread_num()];
                int p1 = tournament(pop, pop_size, TOURNAMENT_K, seed);
                int p2 = tournament(pop, pop_size, TOURNAMENT_K, seed);
                crossover(pop[p1].tour, pop[p2].tour, buf[i].tour, seed);
                mutate(buf[i].tour, seed);
                buf[i].fitness = tour_length(buf[i].tour);
            }
        } else {
            /* schedule(static): division en bloques iguales */
            #pragma omp parallel for schedule(static) num_threads(nthreads)
            for (int i = elite; i < pop_size; i++) {
                unsigned int *seed = &thread_seeds[omp_get_thread_num()];
                int p1 = tournament(pop, pop_size, TOURNAMENT_K, seed);
                int p2 = tournament(pop, pop_size, TOURNAMENT_K, seed);
                crossover(pop[p1].tour, pop[p2].tour, buf[i].tour, seed);
                mutate(buf[i].tour, seed);
                buf[i].fitness = tour_length(buf[i].tour);
            }
        }

        Individual *tmp = pop;
        pop = buf;
        buf = tmp;
    }

    Individual best;
    get_best(pop, pop_size, &best);
    free_population(pop, pop_size);
    free_population(buf, pop_size);
    return best;
}

Individual ga_run_parallel_static(int pop_size, int generations,
                                   int elite, int nthreads) {
    return run_parallel(pop_size, generations, elite, nthreads, 0);
}

Individual ga_run_parallel_dynamic(int pop_size, int generations,
                                    int elite, int nthreads) {
    return run_parallel(pop_size, generations, elite, nthreads, 1);
}

/* =========================================================================
   ESTRATEGIA 2 — MODELO DE ISLAS
   Cada hilo evoluciona su propia sub-poblacion (isla) de forma totalmente
   independiente, sin sincronizacion durante la evolucion.
   Al final se comparan los mejores de cada isla.

   Ventaja sobre la estrategia 1: sin barreras entre generaciones,
   mucho mejor aprovechamiento de los hilos.
   ========================================================================= */

Individual ga_run_island_model(int pop_per_island, int generations,
                               int nthreads) {
    /* Pre-asignamos una poblacion y un buffer por cada isla */
    Individual **islands = malloc(nthreads * sizeof(Individual *));
    Individual **bufs    = malloc(nthreads * sizeof(Individual *));
    for (int t = 0; t < nthreads; t++) {
        islands[t] = init_population(pop_per_island);
        bufs[t]    = init_population(pop_per_island);
    }

    #pragma omp parallel num_threads(nthreads)
    {
        int tid = omp_get_thread_num();
        Individual *pop  = islands[tid];
        Individual *buf  = bufs[tid];
        unsigned int seed = global_seed + (unsigned int)(tid + 1) * 1000;

        /* Cada hilo corre su propio GA completo, sin comunicacion */
        for (int g = 0; g < generations; g++) {
            one_generation(pop, buf, pop_per_island, ELITE_COUNT, &seed);
            Individual *tmp = pop; pop = buf; buf = tmp;
        }

        /* Guardamos la poblacion final de cada isla */
        islands[tid] = pop;
        bufs[tid]    = buf;
    }

    /* Elegimos el mejor individuo entre todas las islas */
    Individual best;
    best.tour    = NULL;
    best.fitness = 1e18;

    for (int t = 0; t < nthreads; t++) {
        sort_population(islands[t], pop_per_island);
        if (islands[t][0].fitness < best.fitness) {
            free(best.tour);
            best.tour = malloc(N * sizeof(int));
            memcpy(best.tour, islands[t][0].tour, N * sizeof(int));
            best.fitness = islands[t][0].fitness;
        }
        free_population(islands[t], pop_per_island);
        free_population(bufs[t],    pop_per_island);
    }

    free(islands);
    free(bufs);
    return best;
}

/* =========================================================================
   UTILIDADES
   ========================================================================= */

void print_individual(const Individual *ind) {
    printf("fitness = %.2f\n", ind->fitness);
}
