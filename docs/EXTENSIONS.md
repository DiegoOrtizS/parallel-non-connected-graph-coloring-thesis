# Extensiones de la investigación: análisis y diseño

Documento de diseño de las ocho extensiones acordadas, cuatro de ellas con cambios de algoritmo. Para cada una se indica:

- qué cambia en la teoría de la tesis,
- con qué experimento se mide,
- qué riesgo tiene implementarla sin compilador local,
- de qué depende.

Todavía no hay código de estas extensiones.

**Principio rector: v1 → v2, no reemplazo.** El análisis ya escrito (Cap. de metodología, §Análisis de rendimiento) predice el cuello de botella de la versión actual: la fase serial de la raíz, con $T_{\text{raíz}} = \Theta(n^2)$, y una isoeficiencia independiente de $n$. Las extensiones salen de ese análisis y se miden contra v1. Así, los cambios de algoritmo pasan a ser el hilo narrativo de la tesis en lugar de invalidar lo escrito. Cada cambio de algoritmo va detrás de un flag, con v1 por defecto, y la campaña base de v1 se corre igual.

## 0. Requisito previo: integración continua

Todo el C++ de la rama `fix/experiments` se escribió sin compilarlo, y cada extensión añadiría más código en las mismas condiciones. Antes de cualquier cambio de algoritmo hace falta un workflow de GitHub Actions que:

1. instale `g++`, `libopenmpi-dev`, `openmpi-bin` y `freeglut3-dev`;
2. ejecute `make` en los cinco programas;
3. corra la prueba de humo del README con un grafo de 200 vértices (`mpirun --oversubscribe -np 4`);
4. verifique que todos los programas imprimen `The graph is well colored.` y el mismo número de componentes.

Requiere hacer push de la rama, y esa decisión es del autor.

## Resumen

| # | Extensión | Tipo | Qué altera en la tesis | Experimento | Riesgo sin compilar | Depende de | Hito |
|---|-----------|------|------------------------|-------------|---------------------|-----------|------|
| 1 | Validar el modelo de costo | medición | cierra Ec. (tp), Prop. de comunicación y cota de speedup con $\alpha, \beta$ medidos | ping-pong + ajuste por mínimos cuadrados; error de predicción por configuración | bajo | campaña v1 | A (código), C (análisis) |
| 2 | Baseline de replicación (`--replicate`) | medición | vuelve medible H1 | $T_{\text{send}}$ por componentes frente a `MPI_Bcast` de la matriz completa | bajo | — | A |
| 3 | Calidad del coloreo | análisis + algoritmo pequeño | nueva proposición: $\chi$ exacto por emparejamiento en el complemento | colores LDF / DSatur / RSOC frente a $\chi$ o sus cotas | medio | — | A |
| 4 | Caso desfavorable + LPT | generador + algoritmo | cambia el supuesto de componentes iguales en la cota de speedup | componentes con tamaños sesgados; reparto contiguo frente a LPT | bajo–medio | 0 | A |
| 5 | Grafos reales | datos | validez externa | SuiteSparse / SNAP con varias componentes | bajo | 7 (CSR) | C |
| 6 | Reproducibilidad (`make results`, CI) | infraestructura | anexos | regenerar todas las figuras desde los datos | bajo | 0 | A |
| 7 | Representación según la densidad (CSR global + bloques bitmap/CSR) | **algoritmo** | memoria $\Theta(n+m)$; volumen comunicado hasta 64 veces menor; P3 mejora | memoria, $T_{\text{dsu}}$, $T_{\text{send}}$ frente a v1 | **alto** | 0 | B |
| 8 | Diseño sin raíz (DSU distribuido + redistribución) | **algoritmo** | **cambia la isoeficiencia**: único ítem que vuelve escalable el sistema | escalabilidad fuerte y débil frente a v1 | **alto** | 0, 7 | B |

---

## 1. Validar el modelo de costo

**Qué falta.** La Ec. (tp) y la Proposición de comunicación usan $\alpha$ (latencia) y $\beta$ (tiempo por palabra) sin medirlos.

**Diseño.**

- Un programa `pingpong` de unas 40 líneas: mensajes de $2^j$ palabras con $j = 0, \dots, 24$, 100 repeticiones cada uno. Ajustar $t(w) = \alpha + \beta w$ por mínimos cuadrados.
- La instrumentación actual ya da, en cada corrida, $T_{\text{send}}$ y el volumen $w_r$, que es conocido por la Ec. (volumen_r). Con eso se puede hacer un segundo ajuste sobre datos reales y contrastarlo con el ping-pong.
- Con $\alpha$, $\beta$ y las constantes de cómputo ($c_1$ del DSU, $c_3$ del coloreo, ajustadas en la referencia secuencial), predecir $T_p$ para cada configuración y reportar el error relativo $|T_p^{\text{modelo}} - T_p^{\text{medido}}| / T_p^{\text{medido}}$.

**Valor.** Un modelo de rendimiento validado con error medido es el aporte que más acerca la tesis a un artículo. La teoría ya está escrita; solo falta cerrar el círculo con datos.

**Cambio en la tesis.** Nueva subsección en resultados, "Validación del modelo", con la tabla de errores y la figura de predicción frente a medición.

## 2. Baseline de replicación

**Qué falta.** La reducción de comunicación (H1) está demostrada analíticamente, pero no medida.

**Diseño.** Un flag `--replicate` en `mpi/main.cpp`: la raíz hace `MPI_Bcast` de la matriz completa, cada proceso extrae localmente sus bloques y el resto es igual. Se comparan $T_{\text{send}}$ y $T_p$ de ambas variantes. La Proposición de comunicación predice un cociente de $\lceil\log_2 p\rceil \frac{p}{p-1} k$ en el régimen dominado por el ancho de banda.

**Riesgo.** Bajo: es una llamada colectiva estándar.

## 3. Calidad del coloreo

**Qué falta.** La tesis reporta $\hat{c}$ (colores de la heurística) sin compararlo con el óptimo.

**Resultado nuevo (a demostrar en la tesis).** Sea $H_i = \overline{G_i}$ el complemento de una componente. Las clases de color de $G_i$ son conjuntos independientes de $G_i$, es decir, cliques de $H_i$.

> **Proposición.** Si $H_i$ no tiene triángulos, entonces $\chi(G_i) = n_i - \nu(H_i)$, donde $\nu$ es el tamaño de un emparejamiento máximo.
>
> *Demostración.* Sin triángulos, toda clique de $H_i$ tiene a lo sumo 2 vértices. Un coloreo con $c$ colores es entonces una partición de $V_i$ en $c$ partes, cada una un vértice o una arista de $H_i$. Si hay $t$ partes de tamaño 2, forman un emparejamiento, así que $c = n_i - t \ge n_i - \nu(H_i)$. Recíprocamente, un emparejamiento máximo junto con los vértices no emparejados da un coloreo con exactamente $n_i - \nu(H_i)$ colores. ∎

$\nu$ se calcula en tiempo polinomial con el algoritmo de Edmonds (blossom) [Edmonds 1965].

**Aplicabilidad.** En $G_1$, el complemento de cada componente tiene densidad $\approx 0.024$ y unos 62 vértices. El número esperado de triángulos es $\binom{62}{3} \cdot 0.024^3 \approx 0.5$ por componente, así que en la mayoría de las componentes $\chi$ es **exacto**. En $G_2$ ($n_i \approx 312$, densidad del complemento $\approx 0.037$) se esperan cientos de triángulos y solo hay cotas:

- inferior: $\chi(G_i) \ge \lceil n_i / \omega(H_i) \rceil$, con $\omega(H_i)$ pequeño y calculable de forma exacta en un $H_i$ disperso;
- superior: el mejor resultado entre LDF, DSatur [Brélaz 1979] y RSOC.

La proposición debe verificarse en cada instancia comprobando que $H_i$ no tiene triángulos.

**Sobre la asintótica de grafos aleatorios.** Grimmett y McDiarmid [1975] muestran que, en $G(n,p)$ con $p$ fijo y $n \to \infty$, el voraz usa $\approx n / \log_b n$ colores con $b = 1/(1-p)$, aproximadamente el doble que $\chi \approx n/(2\log_b n)$ [Bollobás 1988]. Con $p \approx 0.97$ y $n_i \approx 60$ el régimen no es asintótico, así que conviene presentarla solo como contexto. El emparejamiento da la respuesta exacta.

**Riesgo.** Medio: implementar Edmonds desde cero sin compilador es propenso a errores. Alternativas:

- usar Boost.Graph (`edmonds_maximum_cardinality_matching`), si está en Khipu;
- calcularlo fuera de línea con NetworkX (`max_weight_matching`) sobre los grafos guardados. **Recomendado:** es análisis, no forma parte del tiempo medido.

## 4. Caso desfavorable y balance con LPT

**Generador.**

- Opción de tamaños sesgados: $n_i \propto i^{-s}$ (Zipf), con una componente gigante y muchas pequeñas. Es el peor caso del Corolario de volumen: $\sum n_i^2 \to n^2$.
- **Permutación aleatoria de etiquetas al generar.** Hoy cada componente ocupa un rango contiguo de vértices, lo que favorece la localidad del DSU y del empaquetado. Es una amenaza a la validez barata de cerrar y no cambia el análisis.

**LPT.** Ordenar las componentes por costo estimado decreciente ($n_i^2$ con matriz o bitmap, $n_i + m_i$ con CSR) y asignar cada una al proceso menos cargado. Por Graham [1969], $\max_r T_{\text{col}}(r) \le \left(\frac{4}{3} - \frac{1}{3p}\right)\text{OPT}$. En la Proposición de cota de speedup, $\lceil k/p \rceil \tau$ se reemplaza por esa cota. Con componentes iguales, LPT coincide con el reparto contiguo, así que el resultado de v1 se mantiene como caso particular.

**Experimento.** Escalabilidad fuerte con un grafo Zipf: reparto contiguo frente a LPT, reportando el desbalance $\max_r T_{\text{col}}(r) / \overline{T_{\text{col}}}$.

## 5. Grafos reales

Instancias de SuiteSparse o SNAP con varias componentes conexas. Estas colecciones suelen tener una componente gigante y muchas pequeñas, que es justo el caso del ítem 4. Necesitan CSR (ítem 7): con $n \ge 10^5$, la matriz no cabe en memoria ($8n^2$ bytes = 80 GB). Antes de citarlas hay que verificar los nombres y los tamaños de las instancias.

## 6. Reproducibilidad

- `make results` en la raíz del repositorio: agrega los logs y copia los CSV a la tesis. Recompilar la tesis regenera todas las figuras y tablas.
- Tabla fija del entorno en la tesis (CPU, compilador, MPI, flags).
- Integración continua (ítem 0).

## 7. Representación según la densidad de la componente

**Volumen por bloque, en bits** ($m_i \approx D_i n_i^2/2$):

| Representación | Bits por bloque | Frente a v1 (enteros de 64 bits) |
|---|---|---|
| v1: matriz de `long long` | $64\, n_i^2$ | 1× |
| Bitmap | $n_i^2$ | **64× menos** |
| CSR, ambos sentidos, índices de 32 bits | $\approx 32 (D_i n_i^2 + n_i)$ | $\approx 2/D_i$ veces menos |

**Punto de cruce.** El bitmap y CSR ocupan lo mismo cuando $n_i^2 = 32 D_i n_i^2$, es decir, cuando $D_i^{*} = 1/32 \approx 0.031$. Guardando solo el triángulo superior, CSR ocupa la mitad y el cruce pasa a $D_i^{*} = 1/16$.

| Grafo | $D_i$ | Representación ganadora | Margen |
|---|---|---|---|
| $G_1$, $G_2$ | 0.96–0.98 | bitmap | ≈ 31× frente a CSR |
| $G_3$ | 0.032 | en el cruce | prueba natural de la regla de selección |
| SuiteSparse típico | $\sim 10^{-4}$ | CSR | ≈ 300× frente a bitmap |

**Diseño.**

- **Grafo global en CSR en la raíz.** Memoria $\Theta(n+m)$ y DSU $\Theta(n + m\,\alpha(n))$ en lugar de $\Theta(n^2)$. Es el único camino a $n \ge 10^5$. Obliga a cambiar el formato de archivo del generador a una lista de aristas binaria: hoy cada carga parsea $n^2 = 10^8$ números en texto.
- **Cada bloque se envía en la representación más compacta según $D_i$**, con un byte de tipo por bloque.
- El coloreo local trabaja sobre la representación recibida. LDF con CSR cuesta $\Theta(n_i + m_i)$, ordenando los grados por cubetas. Con bitmap, recorrer los vecinos cuesta $\Theta(n_i^2/64)$ operaciones de palabra más los bits activos.

**Qué cambia en la teoría.**

- En la Proposición de costo de la identificación, $T_{\text{dsu}}$ pasa de $\Theta(n^2)$ a $\Theta(n+m)$.
- En la Ec. (volumen_r), $w_r$ pasa a medirse en bits con el costo de la representación elegida, y $\beta$ efectivo baja hasta 64 veces en los grafos de la tesis. Es el cambio más alineado con el título.
- En escalabilidad débil ($n = p\,n_c$, $m = p\,m_c$), el costo de la raíz pasa de $\Theta(p^2)$ a $\Theta(p)$. La predicción P3 cambia de $E_w \sim 1/p^2$ a $E_w \sim 1/p$.
- **La isoeficiencia no cambia de forma.** Con CSR, $T_{\text{raíz}} = \Theta(n+m)$ y $W = k\tau = \Theta(n+m)$, así que la condición $W \ge K (p-1) T_{\text{raíz}}$ se reduce a $c_3 \ge K (p-1) c_1$: sigue sin depender de $n$. Cambiar la representación mejora constantes, memoria y volumen, pero no vuelve escalable el sistema. Hay que decirlo explícitamente en la tesis.

**Optimización menor.** Reemplazar los `MPI_Send` secuenciales por `MPI_Scatterv`. El modelo binomial sugiere $\lceil\log_2 p\rceil\alpha$, pero muchas implementaciones lo ejecutan de forma lineal desde la raíz. Va detrás de un flag y se presenta como pregunta empírica, no como mejora garantizada.

## 8. Diseño sin raíz: DSU distribuido y redistribución

> **Estado (2026-10-05): implementado en `algorithms/mpi_v2` (PR #4).**
>
> - La fusión es **dispersa** desde el principio, al estilo SiskinCC: cada proceso envía solo los pares $(v, \text{raíz}(v))$ con raíz distinta.
> - Lectura con franjas contiguas de aristas, sin E/S paralela de MPI.
> - LPT replicado sobre $n_i + m_i$.
> - Verificado en la CI: encuentra las $k$ componentes y colorea igual que el LDF de v1 con $p = 1$ y $p = 4$.
> - Volumen medido en G2 con $p = 4$: 9.0 MB en aristas frente a 18.8 MB de v1 y 2.40 GB de la replicación.
>
> La derivación de costo vigente es la de `methodology.tex`, §Rediseño sin raíz. Con la fusión dispersa, el término de comunicación de la fusión baja a $\beta\min\{n, m/p\}\log p$. Siguen presentes el recorrido de $n$ vértices y la difusión de $n$ identificadores, de modo que la isoeficiencia conserva la forma $m = \Omega(p\,n\log p)$.

Es el único cambio que ataca la isoeficiencia. Mientras exista una fase serial del mismo orden que el trabajo total, el número útil de procesos está acotado por una constante.

**Algoritmo (por proceso $r$).**

1. **Lectura por franjas.** Cada proceso lee $m/p$ aristas del archivo de aristas (con E/S paralela de MPI o con desplazamientos). Costo $\Theta(m/p)$.
2. **DSU local** sobre sus aristas, con un arreglo `parent` de tamaño $n$. Costo $\Theta(n + (m/p)\,\alpha(n))$.
3. **Fusión de bosques en árbol.** En $\lceil\log_2 p\rceil$ rondas, cada par de procesos intercambia su arreglo de raíces ($n$ palabras) y une. Costo $\lceil\log_2 p\rceil\,(\alpha + \beta n + c\,n)$. Con fusiones dispersas, enviando solo los vértices tocados, el término baja a $O(\min(n, m/p)\log p)$.
4. **Difusión de las etiquetas de componente** y asignación de componentes a procesos con LPT (ítem 4). Costo $\lceil\log_2 p\rceil(\alpha + \beta n)$.
5. **Redistribución de aristas** con `MPI_Alltoallv`: cada arista va al proceso dueño de su componente. Costo $\Theta(m/p)$ palabras por proceso más $(p-1)\alpha$.
6. **Coloreo local sin comunicación** (igual que en v1).
7. **Recolección** de los colores, o dejarlos distribuidos.

**Isoeficiencia.**

$$T_p \approx c\,\frac{n+m}{p} + c'\,n\log_2 p + \alpha p, \qquad T_o = p\,T_p - T_1 \approx c'\,p\,n\log_2 p + \alpha p^2 .$$

Con $W = \Theta(n+m)$ y $W \ge K\,T_o$:

$$n + m \;\ge\; K\left(c'\,p\,n\log_2 p + \alpha p^2\right) \quad\Longrightarrow\quad m = \Omega(p\,n\log p), \text{ es decir, } \bar{d} = \Omega(p \log p).$$

**Resultado.** La propuesta escala si y solo si no hay una raíz serial, y en ese caso escala en función del grado medio. En el régimen de la tesis (componentes densas, $\bar d \approx 60$–$300$) la condición se cumple con holgura para $p \le 32$. En grafos reales dispersos ($\bar d \sim 10$) no se cumple, y la fusión dispersa del paso 3 se vuelve necesaria. Es un resultado honesto y publicable. Antes de pasarlo a la tesis hay que verificar los términos con la implementación.

**Alternativas descartadas.** Un union-find concurrente sin bloqueos en memoria compartida no es implementable de forma confiable sin poder compilar ni probar. La fusión de bosques es simple, determinista y verificable.

**Literatura reciente que hay que revisar antes de implementar** (ver `Tesis_Coloreo_Grafos/docs/literature_update.md`):

- SiskinCC y RobinCC [Koohi Esfahani, 2025] ya resuelven componentes conexas distribuidas sobre el DSU concurrente de Jayanti y Tarjan [2021], optimizando memoria y red. Pueden reemplazar los pasos 2–4.
- FastSV [2020], LACC [2019] y Contour [2023] son las referencias de escala a comparar.
- Si alguno de estos algoritmos se adopta, el aporte de la tesis pasa a ser su integración con la distribución por componentes y el análisis de isoeficiencia, no un DSU nuevo.

**Variante con MPI 4.** Con comunicación particionada (`MPI_Psend_init` / `MPI_Pready`), cada hilo de la raíz marca como lista su parte del buffer en cuanto termina de empaquetarla. Así el empaquetado se solapa con el envío y se ataca $T_{\text{pack}} + T_{\text{send}}$ también en la versión v1 con raíz. Open MPI 5 la implementa. Va como flag adicional del hito B.

**Riesgo.** Alto: E/S paralela, `Alltoallv` y fusión de bosques. Requiere CI en verde y una batería de pruebas que compare las componentes con v1 en grafos pequeños.

---

## Plan por hitos

| Hito | Contenido | Necesita Khipu | Bloqueado por |
|------|-----------|----------------|---------------|
| **A** | CI (0); permutación de etiquetas y tamaños Zipf en el generador (4); LPT (4); `--replicate` (2); ping-pong (1); calidad: DSatur + script de $\chi$ por emparejamiento (3); `make results` (6) | no | permiso de push para la CI |
| **B** | formato de lista de aristas + CSR global (7); bloques bitmap/CSR según densidad (7); `Scatterv` tras un flag (7); diseño sin raíz (8) | no para escribirlo; sí para medirlo | CI en verde tras el hito A |
| **C** | campaña v1 + v2; validación del modelo (1); grafos reales (5); subsección "Rediseño" en la tesis | sí | acceso a Khipu |

## Frases de la tesis que cambian al implementar cada ítem

Para no repetir la situación inicial, en la que la tesis prometía cosas que no hacía:

| Ítem | Archivo y sección | Cambio |
|------|-------------------|--------|
| 7 | `metodologia.tex`, §Distribución | "no aplicadas en esta versión" pasa a describir la versión v2 |
| 7, 8 | `metodologia.tex`, §Análisis de rendimiento | añadir las versiones v2 de la Prop. de costo de identificación, la Ec. (volumen_r) y la isoeficiencia (ítem 8) |
| 4 | `metodologia.tex`, Prop. de cota de speedup | generalizar con la cota de LPT |
| 3 | `marco_teorico.tex`, §Coloreo | añadir la Proposición de $\chi$ por emparejamiento |
| 4, 7, 8 | `trabajosFuturos.tex` | quitar lo implementado y conservar lo que siga pendiente (p. ej., fusión dispersa) |
| todos | `CODE_REVIEW.md`, §Pendiente | marcar como hecho con el hash del commit |
| 4 | `metodologia.tex`, §Amenazas a la validez | eliminar la amenaza de etiquetas contiguas |

## Referencias nuevas (verificar antes de citar en la tesis)

- J. Edmonds, "Paths, trees, and flowers", *Canadian Journal of Mathematics*, 17, 449–467, 1965.
- G. R. Grimmett y C. J. H. McDiarmid, "On colouring random graphs", *Mathematical Proceedings of the Cambridge Philosophical Society*, 77(2), 313–324, 1975.
- B. Bollobás, "The chromatic number of random graphs", *Combinatorica*, 8(1), 49–55, 1988.
- R. L. Graham, "Bounds on multiprocessing timing anomalies", *SIAM J. Appl. Math.*, 17(2), 416–429, 1969 (ya está en la tesis).
- Para la fusión de bosques en el DSU distribuido: revisar la literatura de componentes conexas distribuidas antes de presentarla como aporte (por ejemplo, el trabajo de Shiloach y Vishkin y sus versiones distribuidas).
