# Trabajo Práctico #2 — TSP con Algoritmo Genético y OpenMP

| | |
|---|---|
| **Materia** | Sistemas Paralelos y Distribuidos |
| **Carrera** | Ingeniería Informática |
| **Alumno** | Matías Domin |
| **Fecha** | 11 de junio de 2026 |

---

## Introducción

El Problema del Vendedor Viajante (TSP) consiste en encontrar el recorrido de menor distancia que visite exactamente una vez cada ciudad de un conjunto dado y regrese al punto de origen. Es un problema NP-completo: no existe algoritmo exacto de tiempo polinomial, por lo que para instancias grandes se recurre a metaheurísticas.

Los **algoritmos genéticos (AG)** simulan la evolución natural: mantienen una población de soluciones candidatas y las mejoran iterativamente aplicando selección, cruzamiento y mutación. Combinados con **OpenMP**, una API para programación paralela en memoria compartida, es posible acelerar la búsqueda aprovechando múltiples núcleos del procesador.

El objetivo de este trabajo es implementar un AG paralelo para el TSP, comparar dos estrategias de paralelización con dos técnicas de balanceo de carga, y analizar el impacto en tiempo de ejecución, speedup y eficiencia.

---

## Metodología

### Algoritmo Genético

Cada individuo de la población representa un **tour**: una permutación de las N ciudades. La función de aptitud (*fitness*) es la longitud total del recorrido — menor es mejor.

El ciclo de cada generación aplica cuatro operadores:

1. **Selección por torneo:** se eligen 3 individuos al azar y avanza el de menor fitness. Favorece a los mejores individuos manteniendo diversidad en la población.

2. **Cruzamiento de 1 punto:** se copia el prefijo del padre 1 al hijo; las ciudades restantes se toman del padre 2 en orden, saltando las ya presentes. Este paso de reparación es necesario porque en TSP cada ciudad debe aparecer exactamente una vez.

3. **Mutación por reemplazo:** con probabilidad 2% por ciudad, se intercambian dos posiciones al azar. Para permutaciones, el intercambio es equivalente al reemplazo aleatorio y mantiene el tour válido.

4. **Elitismo:** los 5 mejores individuos pasan directamente a la siguiente generación sin modificaciones.

Los parámetros finales se determinaron experimentando con distintas combinaciones. Se observó que duplicar las generaciones mejora más la calidad de la solución que duplicar la población, con el mismo tiempo de cómputo total:

| Parámetro | Valor |
|---|---|
| Población | 500 |
| Generaciones | 1000 |
| Torneo k | 3 |
| Tasa de mutación | 2% |
| Elitismo | 5 |

### Estrategias de paralelización

#### Estrategia 1 — Generación paralela de hijos (`parallel for`)

La creación de cada hijo dentro de una generación es independiente de los demás: no hay escritura compartida entre hilos. Esto permite distribuir el bucle con `#pragma omp parallel for`.

Un punto importante es la generación de números aleatorios: `rand()` usa una semilla global compartida, lo que genera condiciones de carrera. Se utilizó `rand_r()` con una semilla independiente por hilo, eliminando el problema.

Al final de cada generación OpenMP inserta una **barrera implícita**: todos los hilos esperan a que el último termine antes de continuar con el ordenamiento y el elitismo de la siguiente generación. Con 1000 generaciones esto representa 1000 sincronizaciones por ejecución, que es el principal limitante del speedup.

Se implementaron dos técnicas de balanceo de carga:

- **`schedule(static)`:** divide las iteraciones en bloques iguales antes de empezar. Mínimo overhead; funciona bien cuando el costo por iteración es uniforme.
- **`schedule(dynamic)`:** asigna iteraciones bajo demanda a medida que los hilos quedan libres. Más justo ante variaciones en el costo, con mayor overhead de coordinación. Muestra ventaja en pr1002 donde calcular el fitness implica recorrer 1002 ciudades.

#### Estrategia 2 — Modelo de islas

Cada hilo evoluciona su propia sub-población (**isla**) de forma completamente independiente durante todas las generaciones. Solo existe una sincronización al final para comparar los mejores individuos de cada isla y retener el mejor global.

A diferencia de la estrategia anterior, no hay barreras entre generaciones: los hilos nunca se esperan entre sí durante la evolución. Cada isla recibe `población / hilos` individuos, manteniendo el trabajo total equivalente al de la versión secuencial.

---

## Resultados

Experimentos realizados en **Intel Core i7-12700** (20 hilos lógicos), Linux. Speedup = T_secuencial / T_paralelo. Eficiencia = Speedup / N_hilos.

### berlin52 — 52 ciudades (T_sec ≈ 0.21 s)

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Paralela `schedule(static)` | 4 | 0.176 s | 1.20× | 29.9% |
| Paralela `schedule(static)` | 8 | 0.209 s | 0.96× | 12.0% |
| Paralela `schedule(static)` | 20 | 0.274 s | 0.75× | 3.8% |
| Paralela `schedule(dynamic)` | 4 | 0.189 s | 1.11× | 27.8% |
| Paralela `schedule(dynamic)` | 8 | 0.227 s | 0.89× | 11.1% |
| Paralela `schedule(dynamic)` | 20 | 0.201 s | 1.03× | 5.1% |
| **Modelo de Islas** | **4** | **0.108 s** | **1.95×** | **48.9%** |
| **Modelo de Islas** | **8** | **0.034 s** | **5.87×** | **73.4%** |
| **Modelo de Islas** | **20** | **0.017 s** | **12.24×** | **61.2%** |

### kroA200 — 200 ciudades (T_sec ≈ 0.66 s)

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Paralela `schedule(static)` | 4 | 0.412 s | 1.57× | 39.3% |
| Paralela `schedule(static)` | 8 | 0.441 s | 1.49× | 18.6% |
| Paralela `schedule(static)` | 20 | 0.613 s | 1.09× | 5.5% |
| Paralela `schedule(dynamic)` | 4 | 0.379 s | 1.71× | 42.7% |
| Paralela `schedule(dynamic)` | 8 | 0.400 s | 1.64× | 20.5% |
| Paralela `schedule(dynamic)` | 20 | 0.382 s | 1.75× | 8.8% |
| **Modelo de Islas** | **4** | **0.187 s** | **3.47×** | **86.7%** |
| **Modelo de Islas** | **8** | **0.113 s** | **5.84×** | **72.9%** |
| **Modelo de Islas** | **20** | **0.057 s** | **11.71×** | **58.5%** |

### pr1002 — 1 002 ciudades (T_sec ≈ 3.22 s)

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Paralela `schedule(static)` | 4 | 1.553 s | 2.08× | 51.9% |
| Paralela `schedule(static)` | 8 | 1.741 s | 1.85× | 23.1% |
| Paralela `schedule(static)` | 20 | 2.131 s | 1.51× | 7.6% |
| Paralela `schedule(dynamic)` | 4 | 1.483 s | 2.17× | 54.4% |
| Paralela `schedule(dynamic)` | 8 | 1.455 s | 2.21× | 27.7% |
| Paralela `schedule(dynamic)` | 20 | 1.445 s | 2.23× | 11.1% |
| **Modelo de Islas** | **4** | **0.950 s** | **3.39×** | **84.9%** |
| **Modelo de Islas** | **8** | **0.548 s** | **5.88×** | **73.4%** |
| **Modelo de Islas** | **20** | **0.292 s** | **11.02×** | **55.1%** |

---

## Conclusiones

**La estrategia `parallel for` escala pobremente.** El speedup máximo observado es ~2.2× (pr1002 con dynamic, 20 hilos), muy por debajo del teórico. La causa es la **Ley de Amdahl**: en cada generación existe una barrera de sincronización implícita, más una parte serial obligatoria (ordenamiento de la población, copia del elitismo). Con 1000 generaciones, el programa ejecuta 1000 sincronizaciones. En instancias pequeñas como berlin52, el overhead de estas barreras supera al trabajo paralelo, produciendo speedup menor que 1× al aumentar los hilos (0.75× con 20 hilos).

`schedule(dynamic)` muestra una ventaja consistente sobre `schedule(static)` en pr1002, donde calcular el fitness de cada tour involucra 1002 accesos a la matriz de distancias. El balanceo dinámico compensa las variaciones de costo entre iteraciones. En berlin52, donde el costo es muy pequeño y uniforme, la diferencia es insignificante.

**El modelo de islas escala eficientemente**, alcanzando 12.24× en berlin52 y 11.02× en pr1002 con 20 hilos. La razón es que reemplaza las 1000 barreras por generación por una única sincronización final. Cada hilo trabaja de forma autónoma durante toda la evolución, sin esperar a los demás. La eficiencia decrece al aumentar los hilos (de ~85% con 4 hilos a ~55% con 20 hilos) principalmente por saturación del ancho de banda de memoria: con muchos hilos accediendo simultáneamente a la misma matriz de distancias, el caché deja de ser suficiente.

Una ventaja adicional del modelo de islas es la **calidad de las soluciones**: al explorar subpoblaciones independientes, el algoritmo evita converger a un único mínimo local. En kroA200 con 8 hilos, el modelo de islas encontró un tour de 163 575 frente a 190 629 de la versión estática — una mejora del 14%.

En conclusión, para algoritmos genéticos en TSP la granularidad de la paralelización es determinante. Paralelizar dentro de cada generación introduce demasiada sincronización para ser eficiente. Paralelizar generaciones completas de forma independiente elimina ese overhead y aprovecha los múltiples núcleos con alta eficiencia.

---

## Referencias

- Goldberg, D. E. (1989). *Genetic algorithms in search, optimization, and machine learning*. Addison-Wesley.
- Sastry, K., Goldberg, D., & Kendall, G. (2005). Genetic algorithms. *Search methodologies*, 97–125.
- OpenMP Architecture Review Board. *OpenMP API Specification*. https://www.openmp.org/resources/tutorials-articles/
- TSPLIB instances: berlin52, kroA200, pr1002. https://github.com/pdrozdowski/TSPLib.Net
