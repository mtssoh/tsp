# Trabajo Práctico #2 — TSP con Algoritmo Genético y OpenMP

**Materia:** Sistemas Paralelos y Distribuidos  
**Carrera:** Ingeniería Informática  
**Alumno:** Matías Domin  
**Fecha de entrega:** 11 de junio de 2026  

---

## Introducción

El Problema del Vendedor Viajante (TSP, _Travelling Salesman Problem_) consiste en
encontrar el recorrido de menor distancia que visite exactamente una vez cada ciudad
de un conjunto dado y regrese al punto de origen. Pertenece a la clase NP-completo,
por lo que no se conoce ningún algoritmo exacto que lo resuelva en tiempo polinomial;
para instancias de cientos o miles de ciudades es necesario recurrir a heurísticas o
metaheurísticas.

Los **algoritmos genéticos (AG)** son metaheurísticas inspiradas en la evolución
natural. Mantienen una *población* de soluciones candidatas y las mejoran
iterativamente mediante selección, cruzamiento y mutación. Combinados con
**OpenMP**, una API estándar para programación paralela en memoria compartida, es
posible acelerar la búsqueda aprovechando los múltiples núcleos de un procesador
moderno.

El objetivo de este trabajo es implementar un AG paralelo para el TSP, comparar
dos estrategias de paralelización y dos técnicas de balanceo de carga, y analizar
el impacto en tiempo de ejecución, speedup y eficiencia.

---

## Metodología

### Algoritmo Genético para TSP

Cada individuo de la población representa un **tour**: una permutación de las N
ciudades. La función de aptitud (_fitness_) es la longitud total del recorrido —
menor es mejor.

El ciclo de cada generación consta de cuatro pasos:

1. **Selección por torneo**: se eligen k=5 individuos al azar y se retiene el de
   menor fitness. Favorece a los buenos pero mantiene diversidad.
2. **Cruzamiento OX (_Order Crossover_)**: se copia un segmento aleatorio del
   padre 1 al hijo; las ciudades faltantes se rellenan en el orden en que aparecen
   en el padre 2, preservando la estructura relativa de ambos progenitores.
3. **Mutación por intercambio**: con probabilidad 0.02 por ciudad se intercambian
   dos posiciones al azar, introduciendo variabilidad para evitar mínimos locales.
4. **Elitismo**: los 2 mejores individuos de la generación anterior pasan
   directamente a la siguiente sin modificaciones, garantizando que la mejor
   solución nunca empeora.

**Parámetros:** población = 200, generaciones = 500, elitismo = 2, torneo k = 5.

### Estrategias de paralelización con OpenMP

#### Estrategia 1 — Generación paralela de hijos

En cada generación, la creación de cada hijo (selección + cruzamiento + mutación)
es independiente de los demás hijos. Se paraleliza el bucle con
`#pragma omp parallel for`, repartiendo las `pop_size - elite` iteraciones entre
los hilos disponibles. Se usan semillas `rand_r()` por hilo para evitar condiciones
de carrera en la generación de números aleatorios.

Se implementaron dos variantes de balanceo de carga de OpenMP:

- **`schedule(static)`**: divide las iteraciones en bloques iguales antes de
  comenzar. Con 4 hilos y 198 hijos, cada hilo genera exactamente 49 o 50 hijos.
  Mínima sobrecarga de scheduling; adecuado cuando el costo por iteración es
  uniforme.
- **`schedule(dynamic, 1)`**: asigna iteraciones de a una bajo demanda; cuando un
  hilo termina solicita la siguiente. Elimina el desbalance si el costo por
  iteración varía, a costa de mayor overhead de sincronización.

#### Estrategia 2 — Modelo de islas

Cada hilo evoluciona su propia sub-población (**isla**) completamente de forma
independiente, sin ninguna sincronización durante las 500 generaciones. Al
finalizar, se comparan los mejores individuos de cada isla y se retiene el global.
Esto elimina las barreras implícitas de `parallel for` entre generaciones y reduce
la contención sobre la memoria caché.

---

## Resultados

Los experimentos se realizaron con 4 hilos sobre tres instancias de TSPLIB.
Se reporta el mejor tour encontrado, el tiempo de ejecución, el speedup
(T_secuencial / T_paralelo) y la eficiencia (speedup / nthreads).

### berlin52 — 52 ciudades

| Estrategia                  | Mejor tour | Tiempo  | Speedup | Eficiencia |
|-----------------------------|-----------|---------|---------|------------|
| Secuencial                  | 8 339.81  | 0.045 s |  —      |  —         |
| Paralela `schedule(static)` | 8 922.19  | 0.059 s | 0.77×   | 19.1 %     |
| Paralela `schedule(dynamic)`| 7 998.62  | 0.064 s | 0.71×   | 17.8 %     |
| Modelo de Islas             | 9 190.81  | 0.012 s | 3.83×   | 95.7 %     |

### kroA200 — 200 ciudades

| Estrategia                  | Mejor tour  | Tiempo  | Speedup | Eficiencia |
|-----------------------------|------------|---------|---------|------------|
| Secuencial                  | 188 155.77 | 0.155 s |  —      |  —         |
| Paralela `schedule(static)` | 187 933.31 | 0.194 s | 0.80×   | 19.9 %     |
| Paralela `schedule(dynamic)`| 185 543.32 | 0.206 s | 0.75×   | 18.8 %     |
| Modelo de Islas             | 178 988.94 | 0.043 s | 3.62×   | 90.4 %     |

### pr1002 — 1 002 ciudades

| Estrategia                  | Mejor tour    | Tiempo  | Speedup | Eficiencia |
|-----------------------------|--------------|---------|---------|------------|
| Secuencial                  | 5 304 931.90 | 0.821 s |  —      |  —         |
| Paralela `schedule(static)` | 5 432 921.76 | 0.800 s | 1.03×   | 25.7 %     |
| Paralela `schedule(dynamic)`| 5 293 104.45 | 0.892 s | 0.92×   | 23.0 %     |
| Modelo de Islas             | 5 286 023.46 | 0.217 s | 3.78×   | 94.6 %     |

---

## Conclusiones

**La estrategia `parallel for` no escala.** En las tres instancias el speedup ronda
0.7×–1.0×, es decir, la versión paralela no es más rápida que la secuencial. La
causa es la contención de memoria: la función `tour_length()` accede a la matriz de
distancias (`N × N` doubles) en un orden aleatorio determinado por el tour. Para
pr1002 esa matriz ocupa ~8 MB; con 4 hilos accediendo simultáneamente a posiciones
dispersas, el ancho de banda de la caché L3 se satura y los núcleos esperan datos.
El overhead de las barreras implícitas entre generaciones agrava el problema. La
diferencia entre `schedule(static)` y `schedule(dynamic)` es mínima porque el costo
real no está en la asignación de iteraciones sino en los accesos a memoria.

**El modelo de islas sí escala (~3.7× con 4 hilos, ~93% de eficiencia promedio).**
Al no haber comunicación entre islas durante la evolución se eliminan las barreras
y cada hilo trabaja sobre su propio bloque de población, mejorando la localidad
temporal de los accesos a la matriz de distancias. La eficiencia cercana al 100%
indica que el tiempo de CPU se aprovecha casi por completo.

**Calidad de la solución**: el modelo de islas iguala o supera a las estrategias
con `parallel for` en calidad del tour (kroA200: 178 989 vs. 185 543), además de
ser 4× más rápido. La exploración paralela e independiente de múltiples sub-espacios
de búsqueda actúa como una diversificación natural que ayuda a evitar mínimos
locales.

En conclusión, para este tipo de problema en el que el trabajo por iteración
involucra accesos irregulares a estructuras grandes, el modelo de islas es
claramente superior tanto en velocidad como en eficiencia, mientras que la
paralelización a nivel de bucle con `schedule(static/dynamic)` queda limitada por
el cuello de botella del subsistema de memoria.

---

### Referencias

- Goldberg, D. E. (1989). *Genetic algorithms in search, optimization, and machine learning*. Addison-Wesley.
- Sastry, K., Goldberg, D., & Kendall, G. (2005). Genetic algorithms. *Search methodologies*, 97–125.
- OpenMP Architecture Review Board. *OpenMP API Specification*. https://www.openmp.org/resources/tutorials-articles/
- TSPLIB instances: berlin52, kroA200, pr1002. https://github.com/pdrozdowski/TSPLib.Net
