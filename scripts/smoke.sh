#!/usr/bin/env bash
# Builds every program and runs each one on small graphs. Fails unless every run prints a
# proper-coloring confirmation and a CSV line, both graph formats describe the same instance, and
# the root-free v2 colors every vertex exactly as the v1 LDF does. Used by CI and as the local smoke test.
# Usage: bash scripts/smoke.sh [n m k]   (default 200 2000 4)
set -euo pipefail

N=${1:-200}; M=${2:-2000}; K=${3:-4}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/src
LOG=$ROOT/scripts/results/smoke.log
MPIRUN=(mpirun --oversubscribe)
ZIPF="zipf=1.0,permute,seed=7"
mkdir -p "$SRC/data" "$ROOT/scripts/results"
: > "$LOG"

for d in generator algorithms/sequential algorithms/omp algorithms/mpi algorithms/hybrid algorithms/mpi_v2 benchmarks/pingpong; do
    echo "== build $d"
    make -s -C "$SRC/$d"
done

run() {  # run <dir> <command...>
    local dir=$1; shift
    echo "== $dir: $*"
    (cd "$SRC/$dir" && "$@") | tee -a "$LOG"
}

all_programs() {  # all_programs n m k: 12 colorings (3 baselines, 2 OpenMP, 3 MPI, 2 hybrid, 2 root-free v2)
    run generator ./a.out "$1" "$2" "$3"
    run algorithms/sequential ./a.out "$1" "$2" "$3"
    OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$1" "$2" "$3" 2 rsoc
    OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$1" "$2" "$3" 2 components
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3"
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3" --lpt
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3" --replicate
    OMP_NUM_THREADS=2 run algorithms/hybrid "${MPIRUN[@]}" -np 2 ./a.out "$1" "$2" "$3" 2
    OMP_NUM_THREADS=2 run algorithms/hybrid "${MPIRUN[@]}" -np 2 ./a.out "$1" "$2" "$3" 2 --lpt --replicate
    OMP_NUM_THREADS=1 run algorithms/mpi_v2 "${MPIRUN[@]}" -np 1 ./a.out "$1" "$2" "$3"
    OMP_NUM_THREADS=1 run algorithms/mpi_v2 "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3"
}

# v1 graph: equal components, contiguous labels
all_programs "$N" "$M" "$K"
# Zipf component sizes and permuted labels (worst case of the volume corollary)
GRAPH_VARIANT="$ZIPF" all_programs "$N" "$M" 8

run benchmarks/pingpong "${MPIRUN[@]}" -np 2 ./a.out 12 5

# The edge file derived from the matrix and the one written by --edges-only must be the same instance.
same_instance() {  # same_instance n m k suffix
    local name="$1 $2 $3"
    cp "$SRC/data/$name$4.edges" "$ROOT/scripts/results/from-matrix.edges"
    (cd "$SRC/generator" && ./a.out "$1" "$2" "$3" --edges-only > /dev/null)
    cmp "$ROOT/scripts/results/from-matrix.edges" "$SRC/data/$name$4.edges"
    echo "== same instance in both formats: $name$4"
}
same_instance "$N" "$M" "$K" ""
GRAPH_VARIANT="$ZIPF" same_instance "$N" "$M" 8 " zipf1 perm7"

# Root-free v2 must find the k components and color every vertex exactly as the v1 LDF, for any p.
for k in "$K" 8; do
    ldf=$(awk -F, -v k="$k" '$1 == "CSV" && $2 == "seq-ldf-components" && $5 == k {print $14}' "$LOG")
    v2=$(awk -F, -v k="$k" '$1 == "CSV" && $2 == "mpi-v2-ldf" && $5 == k {print $14}' "$LOG" | sort -u)
    echo "== k=$k: v1 LDF colors $ldf, v2 colors $v2"
    [[ -n "$ldf" && "$v2" == "$ldf" ]]
done
components=$(grep -cE "^Components: ($K|8)\$" "$LOG" || true)
echo "== v2 runs that found the expected number of components: $components/4"
[[ "$components" -eq 4 ]]

# per graph: 12 colorings and 12 CSV lines; MPI, hybrid and v2 runs also print a VOLUME line
colored=$(grep -c "The graph is well colored." "$LOG" || true)
csv=$(grep -c "^CSV," "$LOG" || true)
volume=$(grep -c "^VOLUME," "$LOG" || true)
pingpong=$(grep -c "^PINGPONG," "$LOG" || true)
echo "== proper colorings: $colored/24, CSV lines: $csv/24, VOLUME lines: $volume/14, ping-pong sizes: $pingpong/13"
[[ "$colored" -eq 24 && "$csv" -eq 24 && "$volume" -eq 14 && "$pingpong" -eq 13 ]]

echo "== aggregate"
bash "$ROOT/scripts/aggregate.sh" "$LOG"
