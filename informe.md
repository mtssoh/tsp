# Trabajo Práctico #2 — TSP con Algoritmo Genético y OpenMP

**Materia:** Sistemas Paralelos y Distribuidos  
**Carrera:** Ingeniería Informática  
**Alumno:** Matías Domin  
**Fecha de entrega:** 11 de junio de 2026

---

## Introducción

El Problema del Vendedor Viajante (TSP) consiste en encontrar el recorrido de
menor distancia que visite exactamente una vez cada ciudad de un conjunto dado
y regrese al punto de origen. Pertenece a la clase NP-completo: no existe
algoritmo exacto de tiempo polinomial, por lo que para instancias de cientos o
miles de ciudades se recurre a metaheurísticas.

Los **algoritmos genéticos (AG)** son metaheurísticas inspiradas en la evolución
natural. Mantienen una *población* de soluciones candidatas y las mejoran
iterativamente aplicando selección, cruzamiento y mutación. Combinados con
**OpenMP**, una API estándar para programación paralela en memoria compartida,
es posible acelerar la búsqueda aprovechando los múltiples núcleos de los
procesadores modernos.

El objetivo de este trabajo es implementar un AG paralelo para el TSP, comparar
dos estrategias de paralelización con dos técnicas de balanceo de carga, y
analizar el impacto en tiempo de ejecución, speedup y eficiencia.

---

## Metodología

### Algoritmo Genético para TSP

Cada individuo de la población representa un **tour**: una permutación de las N
ciudades. La función de aptitud (*fitness*) es la longitud total del recorrido —
menor es mejor.

El ciclo de cada generación consta de cuatro pasos:

1. **Selección por torneo**: se eligen 3 individuos al azar y se retiene el de
   menor fitness. Favorece a los mejores individuos manteniendo diversidad
   genética en la población.

2. **Cruzamiento de 1 punto**: se copia el prefijo `[0..cut]` del padre 1 al
   hijo; las ciudades restantes se toman del padre 2 en orden, saltando las ya
   presentes. Para TSP, donde cada ciudad debe aparecer exactamente una vez,
   este paso de reparación es necesario para mantener la validez del tour.

3. **Mutación por reemplazo**: con probabilidad 0.02 por ciudad, se elige una
   posición aleatoria y se intercambian las ciudades de ambas posiciones. Para
   permutaciones, el intercambio es equivalente al reemplazo aleatorio y
   garantiza que el tour siga siendo válido.

4. **Elitismo**: los 2 mejores individuos pasan directamente a la siguiente
   generación sin modificaciones, garantizando que la mejor solución nunca
   empeora entre generaciones.

**Parámetros finales** (resultado de experimentación):

| Parámetro | Valor | Justificación |
|-----------|-------|---------------|
| Población | 500 | Balance entre diversidad y tiempo por generación |
| Generaciones | 1000 | Mayor impacto en calidad que aumentar población |
| Torneo k | 3 | Presión selectiva moderada, evita convergencia prematura |
| Tasa de mutación | 2% | Introduce variabilidad sin destruir buenos tours |
| Elitismo | 5 | Los 5 mejores pasan intactos, garantiza no retroceso |

Se probaron configuraciones alternativas (población 200/generaciones 500 y
población 1000/generaciones 500) observándose que duplicar las generaciones
produce mejor calidad de solución que duplicar la población, con tiempo de
ejecución equivalente.

### Estrategias de paralelización con OpenMP

#### Estrategia 1 — Generación paralela de hijos

La creación de cada hijo en una generación es independiente: cada hijo requiere
seleccionar dos padres, cruzarlos, mutarlos y calcular su fitness sin depender
de los demás hijos. Esto permite distribuir el trabajo entre hilos con
`#pragma omp parallel for`.

Un aspecto importante es la generación de números aleatorios: la función
estándar `rand()` usa una semilla global compartida, lo que produce condiciones
de carrera cuando varios hilos la llaman simultáneamente. Se utilizó `rand_r()`
con una semilla independiente por hilo, eliminando este problema.

Al final de cada generación OpenMP introduce una **barrera de sincronización**
implícita: todos los hilos deben terminar antes de continuar con el
ordenamiento y el elitismo. Con 1000 generaciones, esto representa 1000
sincronizaciones por ejecución y es el principal limitante del speedup.

Se implementaron dos técnicas de balanceo de carga:

- **`schedule(static)`**: divide las iteraciones en bloques iguales antes de
  empezar. Simple y con mínimo overhead; funciona bien cuando todas las
  iteraciones tienen costo similar.

- **`schedule(dynamic)`**: asigna iteraciones bajo demanda a medida que los
  hilos quedan libres. Más justo ante variaciones en el costo por iteración,
  aunque introduce mayor overhead de coordinación.

#### Estrategia 2 — Modelo de islas

Cada hilo evoluciona su propia sub-población (**isla**) de forma completamente
independiente durante todas las generaciones, sin ninguna sincronización
intermedia. Al finalizar, se compara el mejor individuo de cada isla y se
retiene el mejor global.

La diferencia clave respecto a la estrategia anterior es la cantidad de
sincronizaciones: en lugar de una barrera por generación (1000 en total),
existe una única barrera al final de toda la ejecución. Esto permite que los
hilos trabajen a máxima velocidad durante toda la evolución sin esperarse entre
sí. Cada isla recibe `POP_SIZE / nthreads` individuos, manteniendo el trabajo
total comparable al de la versión secuencial.

---

## Resultados

Los experimentos se realizaron en un procesador **Intel Core i7-12700** (20
hilos lógicos) bajo Linux. Se reportan los resultados para 1, 4, 12 y 20 hilos.
El speedup se calcula como T_secuencial / T_paralelo y la eficiencia como
speedup / cantidad_de_hilos.

### berlin52 — 52 ciudades

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Secuencial | — | 0.044 s | — | — |
| Paralela `schedule(static)` | 4 | 0.035 s | 1.19× | 29.9% |
| Paralela `schedule(static)` | 12 | 0.066 s | 0.67× | 5.6% |
| Paralela `schedule(static)` | 20 | 0.067 s | 0.80× | 4.0% |
| Paralela `schedule(dynamic)` | 4 | 0.039 s | 1.08× | 26.9% |
| Paralela `schedule(dynamic)` | 20 | 0.047 s | 1.15× | 5.7% |
| Modelo de Islas | 4 | 0.011 s | 3.62× | 90.6% |
| Modelo de Islas | 12 | 0.006 s | 7.11× | 59.2% |
| Modelo de Islas | 20 | 0.004 s | **13.08×** | 65.4% |

### kroA200 — 200 ciudades

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Secuencial | — | 0.145 s | — | — |
| Paralela `schedule(static)` | 4 | 0.076 s | 1.72× | 42.9% |
| Paralela `schedule(static)` | 20 | 0.126 s | 1.08× | 5.4% |
| Paralela `schedule(dynamic)` | 4 | 0.071 s | 1.85× | 46.3% |
| Paralela `schedule(dynamic)` | 20 | 0.081 s | 1.68× | 8.4% |
| Modelo de Islas | 4 | 0.037 s | 3.55× | 88.8% |
| Modelo de Islas | 12 | 0.020 s | 6.44× | 53.7% |
| Modelo de Islas | 20 | 0.011 s | **12.64×** | 63.2% |

### pr1002 — 1 002 ciudades

| Estrategia | Hilos | Tiempo | Speedup | Eficiencia |
|---|---|---|---|---|
| Secuencial | — | 0.648 s | — | — |
| Paralela `schedule(static)` | 4 | 0.390 s | 1.60× | 39.9% |
| Paralela `schedule(static)` | 20 | 0.449 s | 1.42× | 7.1% |
| Paralela `schedule(dynamic)` | 4 | 0.304 s | 2.05× | 51.3% |
| Paralela `schedule(dynamic)` | 20 | 0.293 s | 2.17× | 10.9% |
| Modelo de Islas | 4 | 0.181 s | 3.45× | 86.2% |
| Modelo de Islas | 12 | 0.096 s | 6.59× | 54.9% |
| Modelo de Islas | 20 | 0.060 s | **10.53×** | 52.6% |

---

## Conclusiones

**La estrategia `parallel for` escala pobremente**, con speedup máximo de ~2×
en el mejor caso (pr1002 con dynamic). La causa principal es la **Ley de
Amdahl**: por cada generación existe una barrera de sincronización implícita al
final del `parallel for`, más la parte serial obligatoria (ordenamiento de
población, copia del elitismo). Con 500 generaciones, esto representa 500
sincronizaciones. Para instancias pequeñas como berlin52, el overhead de estas
barreras supera al trabajo paralelo, resultando en speedup < 1× al aumentar los
hilos (0.67× con 12 hilos).

`schedule(dynamic)` muestra una leve ventaja sobre `schedule(static)` en
instancias grandes (pr1002), donde el costo de evaluar el fitness de un tour
varía según su longitud y hay más trabajo real por iteración que justifica el
balanceo dinámico.

**El modelo de islas escala eficientemente**, alcanzando 10.53× con 20 hilos
en pr1002 y 13.08× en berlin52. La razón es que elimina casi toda la
sincronización: en lugar de 500 barreras por ejecución, existe una sola barrera
al final para comparar los mejores individuos de cada isla. Cada hilo trabaja
sobre su propio bloque de memoria durante toda la evolución, reduciendo la
contención en caché. La eficiencia decrece al aumentar los hilos (de ~90% con 4
hilos a ~52-65% con 20 hilos) debido a la saturación del ancho de banda de
memoria al acceder simultáneamente a la matriz de distancias.

En conclusión, para algoritmos genéticos en TSP la granularidad de la
paralelización es determinante: paralelizar dentro de cada generación introduce
demasiado overhead de sincronización, mientras que paralelizar generaciones
completas de forma independiente aprovecha eficientemente los recursos del
procesador.

---

### Referencias

- Goldberg, D. E. (1989). *Genetic algorithms in search, optimization, and machine learning*. Addison-Wesley.
- Sastry, K., Goldberg, D., & Kendall, G. (2005). Genetic algorithms. *Search methodologies*, 97–125.
- OpenMP Architecture Review Board. *OpenMP API Specification*. https://www.openmp.org/resources/tutorials-articles/
- TSPLIB instances: berlin52, kroA200, pr1002. https://github.com/pdrozdowski/TSPLib.Net
