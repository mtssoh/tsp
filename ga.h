#ifndef GA_H
#define GA_H

/* =========================================================================
   ESTRUCTURAS Y CONSTANTES
   ========================================================================= */

typedef struct {
    int    *tour;    /* permutacion de ciudades (array de N enteros) */
    double  fitness; /* longitud total del recorrido — menor es mejor */
} Individual;

#define TOURNAMENT_K   3      /* candidatos por torneo de seleccion */
#define MUTATION_RATE  0.02   /* probabilidad de mutar cada ciudad  */
#define ELITE_COUNT    5      /* individuos que pasan intactos      */

/* =========================================================================
   INICIALIZACION
   ========================================================================= */

void        ga_seed(unsigned int seed);
Individual *init_population(int size);
void        free_population(Individual *pop, int size);
void        sort_population(Individual *pop, int size);

/* =========================================================================
   ESTRATEGIAS DE EVOLUCION
   Todas devuelven el mejor individuo encontrado.
   El llamador debe liberar best.tour con free().
   ========================================================================= */

/* Secuencial: sin paralelismo, referencia para calcular speedup */
Individual ga_run_sequential(int pop_size, int generations, int elite);

/* Estrategia 1 — Generacion paralela de hijos con schedule(static):
   divide el trabajo en bloques iguales entre los hilos */
Individual ga_run_parallel_static(int pop_size, int generations,
                                  int elite, int nthreads);

/* Estrategia 1 — Generacion paralela de hijos con schedule(dynamic):
   asigna iteraciones bajo demanda, mejor ante cargas desiguales */
Individual ga_run_parallel_dynamic(int pop_size, int generations,
                                   int elite, int nthreads);

/* Estrategia 2 — Modelo de islas:
   cada hilo evoluciona su propia sub-poblacion de forma independiente;
   al final se elige el mejor individuo entre todas las islas */
Individual ga_run_island_model(int pop_per_island, int generations,
                               int nthreads);

/* =========================================================================
   UTILIDADES
   ========================================================================= */

void print_individual(const Individual *ind);

#endif
