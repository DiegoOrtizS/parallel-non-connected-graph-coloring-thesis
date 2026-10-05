# Revisión del código

Revisión del estado del repositorio en `ae95e17` (rama `main`), que es la versión con la que se obtuvieron los resultados de la tesis. Las correcciones están en la rama `fix/experimentos`.

**Importante:** en la máquina donde se hizo la revisión no había compilador de C++ ni MPI, así que **los cambios no se compilaron ni se ejecutaron**. Antes de lanzar la campaña hay que compilar en Khipu (`make` en cada carpeta) y hacer una corrida corta de humo (ver README).

Severidad:

- **Crítica**: invalida o distorsiona resultados ya reportados.
- **Alta**: error de corrección o de compilación.
- **Media**: rendimiento o robustez.
- **Baja**: mantenibilidad.

## Hallazgos que afectan los resultados de la tesis

| # | Severidad | Archivo | Problema | Impacto en los resultados | Estado |
|---|-----------|---------|----------|---------------------------|--------|
| 1 | Crítica | `omp/main.cpp` | El tiempo se guardaba en `float` (`float start = omp_get_wtime()`). `omp_get_wtime` cuenta desde un origen arbitrario, normalmente el uptime del sistema, y un `float` solo conserva unos 7 dígitos significativos. Con días de uptime, la resolución queda en decenas de milisegundos. | Los tiempos de OpenMP en $G_1$ (10–20 ms) están por debajo de la resolución y son ruido de cuantización. | Corregido: `double` |
| 2 | Crítica | `hybrid/main.cpp` | `omp_set_num_threads(threads)` solo se ejecutaba en el proceso 0. Los demás procesos usaban el valor por defecto, que es un hilo por núcleo del nodo. | Con 16 procesos se lanzaban hasta 16 × 32 hilos en 32 núcleos (sobresuscripción). Es la explicación más probable del colapso del híbrido con 32 núcleos en ambos grafos. | Corregido: todos los procesos fijan los hilos y el script exporta `OMP_NUM_THREADS` |
| 3 | Crítica | todos los `Makefile` | No había flag de optimización (`-O0` implícito). | Todos los tiempos reportados son de binarios sin optimizar. Las proporciones entre paradigmas pueden cambiar con `-O3`. | Corregido: `-O3 -march=native` en `src/common.mk` |
| 4 | Alta | `omp/coloringOMP.h`, `mpi/coloringAlgorithms.h`, `sequential/main.cpp` | `new lli[n]` sin inicializar. El código usa `colors[j] != 0` para decidir si un vecino está coloreado, así que leía basura (comportamiento indefinido). | El coloreo final se verificaba, pero el número de colores y el tiempo podían variar entre corridas sin razón algorítmica. | Corregido: `new lli[n]()` |
| 5 | Alta | `hybrid/main.cpp` | `isWellColored` se llamaba sin `result.labels`. Los colores vuelven en el orden de las componentes, no en el de los vértices originales. | La verificación del híbrido comparaba vértices equivocados: podía aceptar un coloreo inválido o rechazar uno válido. | Corregido |
| 6 | Alta | `mpi/coloringMPI.h` (y su copia en `hybrid/`) | Dos `MPI_Igatherv` compartían una única variable `request` y solo se esperaba la segunda. | La primera recolección (colores) podía no haber terminado al usarse: comportamiento indefinido según el estándar MPI. | Corregido: dos `MPI_Request` y `MPI_Waitall` |
| 7 | Alta | `sequential/main.cpp` | Grafo fijo en el código, una llamada a `generateGraph(6245000, 8, 1, 42)` que no coincide con la firma actual (no compilaba) y un cronómetro que incluía la verificación y la impresión. No queda claro qué programa produjo el $T_1^*$ de la tesis. | La eficiencia $E \approx 1.2$ de MPI con $p=2$ sugiere que la referencia no hacía el mismo trabajo. | Reescrito: tres referencias (`seq-ff-full`, `seq-ldf-full`, `seq-ldf-components`) medidas por separado |
| 8 | Alta | `mpi/main.cpp`, `hybrid/main.cpp` | Sin `MPI_Barrier` antes de iniciar el cronómetro, y el tiempo era el del proceso 0. | Pequeño sesgo por llegadas desfasadas al inicio. | Corregido |

## Corrección y compilación

| # | Severidad | Archivo | Problema | Estado |
|---|-----------|---------|----------|--------|
| 9 | Alta | `utils/functions/*.h` | Funciones definidas en headers sin `inline`, incluidas desde varias unidades de traducción. Con más de un `.o` que las incluya hay error de "multiple definition" en el enlace. | Corregido: `inline` |
| 10 | Alta | `generator/main.cpp` | Usaba un constructor `GraphGenerator(1e4, 995000, 50, 1, 42)` y un `generateGraph()` sin argumentos que no existen. Además, dibujaba sin registrar la función de display de GLUT. | Reescrito: `./a.out n m k [--draw]` |
| 11 | Alta | `generator/GraphGenerator.h` | Sin include guard, y el miembro estático `currentInstance` estaba declarado pero nunca definido. | Corregido |
| 12 | Media | `mpi/connectedComponents.h` | Arreglos de longitud variable (`lli parent[n]`) en la pila: no son C++ estándar y desbordan la pila con $n$ grande. `find` devolvía `int` siendo `lli`. | Corregido: `std::vector`, tipos coherentes |
| 13 | Media | `-std=c++11` | El código usa structured bindings (`auto& [p, c]`), que son de C++17. | Corregido: `-std=c++17` |
| 14 | Baja | `utils/types.h`, `ColoringResult.h` | Sin include guards, y `ColoringResult.h` dependía de que otro archivo incluyera antes `types.h`. | Corregido |

## Rendimiento

| # | Severidad | Archivo | Problema | Estado |
|---|-----------|---------|----------|--------|
| 15 | Media | `mpi/coloringAlgorithms.h` | LDF probaba cada color candidato con un nuevo recorrido de la fila: $O(n_i^2 \hat{c}_i)$. | Corregido: arreglo `used` de tamaño $d(v)+2$, que da $\Theta(n_i^2)$ con el mismo coloreo resultante |
| 16 | Media | `omp/coloringOMP.h` | Un `std::set` por vértice (asignación dinámica en el bucle caliente) y un `#pragma omp critical` por vértice defectuoso. | Corregido: arreglo de marcas por hilo con sellos y listas locales que se fusionan una vez por ronda |
| 17 | Media | `mpi/connectedComponents.h` | Recorría la matriz completa ($n^2$) aunque es simétrica. | Corregido: solo el triángulo superior |
| 18 | Media | `mpi/coloringMPI.h` | `std::vector<lli> c = components[j]` copiaba cada componente; `coloringComponents` no liberaba `result.colors` (fuga de memoria). | Corregido: `const&` y `delete[]` |
| 19 | Baja | `mpi/connectedComponents.h` | Orden de componentes no determinista (`unordered_map`): la asignación a procesos cambiaba entre corridas. | Corregido: se ordenan |

## Mantenibilidad

| # | Archivo | Problema | Estado |
|---|---------|----------|--------|
| 20 | `hybrid/coloringHybrid.h`, `hybrid/connectedComponents.h` | Copias literales de los archivos de `mpi/`. | Reemplazados por wrappers que incluyen la versión MPI |
| 21 | `hybrid/prueba.cpp` | Archivo de prueba suelto. | Eliminado |
| 22 | `Makefile` × 5 | Cinco Makefiles casi idénticos. Los del secuencial y el generador no enlazaban `initializeGraph.o`. | Unificados en `src/common.mk` |

## Instrumentación añadida (necesaria para la tesis)

- **Tiempo por fase en MPI e híbrido** (`PhaseTimes`): `dsu`, `pack`, `send`, `coloring`, `gather`. Las fases se separan con barreras y se reduce el máximo entre procesos. `send` es comunicación pura: permite contrastar la hipótesis H1 y el título de la tesis.
- **Una línea CSV por corrida** (`utils/functions/report.h`): `CSV,algorithm,n,m,k,p,h,t_total,t_dsu,t_pack,t_send,t_color,t_gather,colors`.
- **Variante `omp-components`**: DSU + LDF por componente con hilos. Aísla el efecto del paradigma de la diferencia de trabajo $n^2$ frente a $\sum n_i^2$ entre RSOC sobre el grafo completo y los algoritmos por componentes.
- **Scripts** `scripts/strong.slurm`, `scripts/weak.slurm` y `scripts/aggregate.sh`. El agregador calcula media, desviación estándar e IC del 95 % y exporta el formato que lee la tesis. Se probó con un log sintético.

## Pendiente (no aplicado a propósito)

Estos cambios alterarían el algoritmo descrito en la tesis. Quedan como trabajo futuro (capítulo de trabajos futuros):

- Reemplazar los `MPI_Send` secuenciales por `MPI_Scatterv`.
- Empaquetar la matriz en bits en lugar de enteros de 64 bits.
- Paralelizar o distribuir el DSU.
- Usar representación CSR en lugar de matriz de adyacencia.
- Asignar componentes con la regla LPT cuando sus tamaños sean heterogéneos.
- Quitar la dependencia de OpenGL/GLUT de los programas de medición (por ejemplo, con un `-DNO_GLUT` que excluya el dibujo).
- `GraphGenerator::setColorIndex` y `drawGraph` usan `colorIndex` de tamaño 0 cuando el grafo se carga desde disco. Solo afecta al dibujo, no a las mediciones.
