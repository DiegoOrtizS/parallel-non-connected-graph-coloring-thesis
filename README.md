# Coloreo paralelo de grafos no conexos

Código de la tesis de bachiller *Optimización en tiempo de comunicación del coloreo de nodos en grafos no conexos en paralelo. Un análisis bajo los paradigmas de memoria compartida, distribuida e híbrida* (Diego Sebastián Ortiz Sánchez, UTEC; asesor: José Antonio Fiestas Iquira).

La idea central: en un grafo no conexo, la matriz de adyacencia es diagonal por bloques una vez que los vértices se ordenan por componente conexa. Por eso cada componente puede colorearse por separado. El proceso raíz identifica las componentes con DSU y envía a cada proceso solo sus bloques. Así no hay comunicación durante el coloreo, y el volumen enviado baja de $n^2$ a $\sum_i n_i^2$ palabras.

## Algoritmos

| Programa | Identificador en el CSV | Descripción |
|----------|------------------------|-------------|
| `algorithms/sequential` | `seq-ff-full` | First-Fit en orden de vértices sobre la matriz completa (programa secuencial original) |
| | `seq-ldf-full` | Voraz Largest-Degree-First sobre la matriz completa |
| | `seq-ldf-components` | DSU + LDF por componente. Es la referencia $T_1^*$ que exige la tesis |
| `algorithms/omp` | `omp-rsoc` | RSOC de Rokos, Gorman y Kelly (Euro-Par 2015) con OpenMP sobre el grafo completo |
| | `omp-components` | DSU + LDF por componente, con las componentes repartidas entre hilos |
| `algorithms/mpi` | `mpi-ldf` | Distribución por componentes con MPI y LDF local |
| `algorithms/hybrid` | `hybrid-rsoc` | Distribución por componentes con MPI y RSOC local con OpenMP |

Todos los programas verifican al final que el coloreo sea propio, es decir, que ninguna arista tenga sus dos extremos del mismo color.

## Estructura

```
src/
├── common.mk                 reglas y flags de compilación compartidos
├── generator/                generador de grafos no conexos (jngen) y su programa de validación
├── algorithms/
│   ├── sequential/           referencias secuenciales
│   ├── omp/                  coloringOMP.h (RSOC), coloringOMPComponents.h
│   ├── mpi/                  connectedComponents.h (DSU), coloringAlgorithms.h (LDF), coloringMPI.h
│   └── hybrid/               coloringHybrid.h (reutiliza coloringMPI.h con RSOC como coloreo local)
├── utils/                    Graph, verificación, PhaseTimes y salida CSV
└── data/                     grafos generados (se crea al generar; no se versiona)
scripts/
├── strong.slurm              campaña de escalabilidad fuerte
├── weak.slurm                campaña de escalabilidad débil
└── aggregate.sh              agrega los logs: media, desviación estándar e IC del 95 %
```

## Requisitos

- g++ ≥ 9 (C++17) con OpenMP
- Una implementación de MPI (OpenMPI o MPICH) con `mpic++` y `mpirun`
- OpenGL/GLUT (`freeglut`), usado por el generador para dibujar grafos pequeños

En Khipu, cargar los módulos correspondientes (por ejemplo, `module load gcc openmpi`; los nombres exactos dependen del clúster).

## Compilación

Cada programa tiene su `Makefile` y se compila desde su propia carpeta:

```bash
cd src/generator            && make
cd src/algorithms/sequential && make
cd src/algorithms/omp        && make
cd src/algorithms/mpi        && make
cd src/algorithms/hybrid     && make
```

Las opciones por defecto son `-std=c++17 -O3 -march=native -Wall -Wextra` (más `-fopenmp` donde corresponde).

## Uso

Todos los programas reciben `n m k`: número de vértices, de aristas y de componentes conexas. El grafo se genera la primera vez y se guarda en `src/data/` con el nombre `"n m k.txt"`. Las ejecuciones siguientes lo reutilizan, de modo que todos los programas trabajan sobre la misma instancia.

```bash
# Generar y validar (m aristas y k componentes). --draw abre una ventana (solo grafos pequeños).
cd src/generator && ./a.out 1000 30000 16

# Referencias secuenciales: imprime tres líneas CSV
cd src/algorithms/sequential && ./a.out 1000 30000 16

# OpenMP con 8 hilos: rsoc (por defecto) o components
cd src/algorithms/omp && ./a.out 1000 30000 16 8 rsoc

# MPI con 8 procesos
cd src/algorithms/mpi && mpirun -np 8 --bind-to core ./a.out 1000 30000 16

# Híbrido: 4 procesos × 2 hilos. OMP_NUM_THREADS debe valer lo mismo en todos los procesos.
cd src/algorithms/hybrid && OMP_NUM_THREADS=2 mpirun -np 4 ./a.out 1000 30000 16 2
```

### Salida

Además de un resumen legible, cada corrida imprime una línea para el agregador:

```
CSV,algorithm,n,m,k,p,h,t_total,t_dsu,t_pack,t_send,t_color,t_gather,colors
```

| Campo | Significado |
|-------|-------------|
| `p`, `h` | procesos MPI e hilos OpenMP por proceso (núcleos = `p*h`) |
| `t_total` | tiempo de pared del coloreo en segundos (no incluye la carga del grafo ni la verificación) |
| `t_dsu` | identificación de componentes en la raíz |
| `t_pack` | copia de los bloques diagonales a los buffers de envío (raíz) |
| `t_send` | envío y recepción de los bloques: **comunicación pura** |
| `t_color` | coloreo local del proceso más lento |
| `t_gather` | reducción del número de colores y recolección de colores y etiquetas |
| `colors` | número de colores usados (cota superior del número cromático) |

Las fases de MPI se separan con `MPI_Barrier` y se reporta el máximo entre procesos, así que `t_total` es aproximadamente la suma de las fases. En OpenMP solo se llenan `t_color` y, en `omp-components`, `t_dsu`.

## Experimentos

```bash
cd scripts
# Escalabilidad fuerte (grafo G2 de la tesis)
sbatch --export=ALL,N=10000,M=1500000,K=32 strong.slurm
# Escalabilidad débil: k = p componentes de 312 vértices y 46875 aristas cada una
sbatch --export=ALL,NC=312,MC=46875 weak.slurm
```

Antes de enviar, ajustar en los scripts la partición (`#SBATCH --partition`) y los módulos de Khipu. Cada configuración se ejecuta `R=10` veces tras una corrida de calentamiento que se descarta. El mapeo del híbrido (`--map-by ppr:P:node:pe=2`) usa la sintaxis de OpenMPI.

Para agregar los resultados:

```bash
./aggregate.sh results/*.log > results/summary.csv
# Formato que lee la tesis (pgfplots), un archivo por grafo:
./aggregate.sh --thesis 10000 1500000 32 results/strong-*.log > ../../Tesis_Coloreo_Grafos/data/graph2.csv
```

El tiempo $T_1^*$ de cada grafo es la media de `seq-ldf-components` en `summary.csv`.

## Opciones v2 (desactivadas por defecto)

La versión analizada en la tesis (v1) es el comportamiento por defecto. Las extensiones de `docs/EXTENSIONS.md` se activan explícitamente:

| Opción | Dónde | Efecto |
|--------|-------|--------|
| `--lpt` | `mpi`, `hybrid` | asigna componentes con *Longest Processing Time* sobre el costo estimado $n_i^2$, en lugar de bloques contiguos |
| `--replicate` | `mpi`, `hybrid` | línea base de la hipótesis H1: `MPI_Bcast` de la matriz completa y extracción local de los bloques |
| `GRAPH_VARIANT="zipf=s,permute,seed=x"` | todos los programas | genera o carga un grafo con tamaños de componente Zipf y etiquetas permutadas; el archivo lleva el sufijo de la variante |
| `benchmarks/pingpong` | `mpirun -np 2 ./a.out [max_log2_palabras] [repeticiones]` | mide la latencia $\alpha$ y el costo por palabra $\beta$ del modelo de comunicación |

El nombre del algoritmo en el CSV refleja las opciones (por ejemplo, `mpi-ldf-lpt-replicate`).

## Calidad del coloreo

`scripts/coloring_quality.py` (requiere `networkx`) compara LDF y DSatur con cotas del número cromático por componente. En las componentes densas usa el emparejamiento máximo y los triángulos del grafo complemento, y da $\chi$ exacto cuando el complemento no tiene triángulos. La CI lo ejecuta sobre los grafos G1, G2 y G3 de la tesis (job `coloring-quality`, artefactos `quality-*`).

## Atajos

```bash
make build      # compila todo
make smoke      # lo mismo que la CI: todos los programas y variantes sobre grafos pequeños
make quality GRAPH="src/data/1000 30000 16.txt"
make results LOGS="scripts/results/strong-*.log" N=10000 M=1500000 K=32 OUT=../Tesis_Coloreo_Grafos/data/graph2.csv
```

## Prueba de humo

`bash scripts/smoke.sh` (o `make smoke`) compila los programas y ejecuta todas las variantes sobre un grafo v1 y otro Zipf con etiquetas permutadas. Falla si algún coloreo no es propio o falta una línea CSV. Conviene correrla en Khipu antes de cada campaña.

## Revisión del código

`CODE_REVIEW.md` lista los problemas encontrados en la versión con la que se obtuvieron los resultados preliminares de la tesis (resolución del temporizador, hilos del híbrido, compilación sin optimizar, entre otros), su impacto y su corrección.

## Cita

```bibtex
@thesis{Ortiz2023,
  author = {Diego Sebastián Ortiz Sánchez},
  title  = {Optimización en tiempo de comunicación del coloreo de nodos en grafos no conexos en paralelo},
  school = {Universidad de Ingeniería y Tecnología (UTEC)},
  type   = {Tesis de bachiller}
}
```
