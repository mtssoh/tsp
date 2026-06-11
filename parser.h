#ifndef PARSER_H
#define PARSER_H

typedef struct {
    double x, y;
} City;

extern City   *cities;
extern int     N;
extern double *dist;

/* Acceso a la matriz de distancias como si fuera dist[i][j] */
#define DIST(i, j)  dist[(i) * N + (j)]

int  load_tsp(const char *path);
void build_dist_matrix(void);

#endif
