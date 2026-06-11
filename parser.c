#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

City   *cities = NULL;
int     N      = 0;
double *dist   = NULL;

static double euclidean(City a, City b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx*dx + dy*dy);
}

void build_dist_matrix(void) {
    dist = malloc(N * N * sizeof(double));
    if (!dist) { perror("malloc dist"); exit(1); }

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            DIST(i, j) = euclidean(cities[i], cities[j]);
}

int load_tsp(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 0; }

    char line[256];
    int  in_coords = 0;
    int  capacity  = 0;

    while (fgets(line, sizeof(line), f)) {

        if (strncmp(line, "NODE_COORD_SECTION", 18) == 0) {
            in_coords = 1;
            continue;
        }
        if (strncmp(line, "EOF", 3) == 0) break;

        if (in_coords) {
            int    id;
            double x, y;
            if (sscanf(line, "%d %lf %lf", &id, &x, &y) != 3) continue;

            if (N >= capacity) {
                capacity = capacity ? capacity * 2 : 64;
                cities   = realloc(cities, capacity * sizeof(City));
                if (!cities) { perror("realloc"); exit(1); }
            }

            cities[N].x = x;
            cities[N].y = y;
            N++;
        }
    }

    fclose(f);
    return N > 0;
}
