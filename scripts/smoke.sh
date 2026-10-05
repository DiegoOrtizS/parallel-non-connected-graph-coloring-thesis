#!/usr/bin/env bash
# Builds every program and runs each one on small graphs. Fails unless every run prints a
# proper-coloring confirmation and a CSV line. Used by CI and as the local smoke test.
# Usage: bash scripts/smoke.sh [n m k]   (default 200 2000 4)
set -euo pipefail

N=${1:-200}; M=${2:-2000}; K=${3:-4}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/src
LOG=$ROOT/scripts/results/smoke.log
MPIRUN=(mpirun --oversubscribe)
mkdir -p "$SRC/data" "$ROOT/scripts/results"
: > "$LOG"

for d in generator algorithms/sequential algorithms/omp algorithms/mpi algorithms/hybrid benchmarks/pingpong; do
    echo "== build $d"
    make -s -C "$SRC/$d"
done

run() {  # run <dir> <command...>
    local dir=$1; shift
    echo "== $dir: $*"
    (cd "$SRC/$dir" && "$@") | tee -a "$LOG"
}

all_programs() {  # all_programs n m k: 7 colorings (3 baselines, 2 OpenMP, MPI, hybrid) + 3 MPI variants
    run generator ./a.out "$1" "$2" "$3"
    run algorithms/sequential ./a.out "$1" "$2" "$3"
    OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$1" "$2" "$3" 2 rsoc
    OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$1" "$2" "$3" 2 components
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3"
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3" --lpt
    OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$1" "$2" "$3" --replicate
    OMP_NUM_THREADS=2 run algorithms/hybrid "${MPIRUN[@]}" -np 2 ./a.out "$1" "$2" "$3" 2
    OMP_NUM_THREADS=2 run algorithms/hybrid "${MPIRUN[@]}" -np 2 ./a.out "$1" "$2" "$3" 2 --lpt --replicate
}

# v1 graph: equal components, contiguous labels
all_programs "$N" "$M" "$K"
# v2 graph: Zipf component sizes and permuted labels (worst case of the volume corollary)
GRAPH_VARIANT="zipf=1.0,permute,seed=7" all_programs "$N" "$M" 8

run benchmarks/pingpong "${MPIRUN[@]}" -np 2 ./a.out 12 5

# The edge file derived from the matrix and the one written by --edges-only must be the same instance.
same_instance() {  # same_instance n m k
    local name="$1 $2 $3"
    cp "$SRC/data/$name$4.edges" "$ROOT/scripts/results/from-matrix.edges"
    (cd "$SRC/generator" && ./a.out "$1" "$2" "$3" --edges-only > /dev/null)
    cmp "$ROOT/scripts/results/from-matrix.edges" "$SRC/data/$name$4.edges"
    echo "== same instance in both formats: $name$4"
}
same_instance "$N" "$M" "$K" ""
GRAPH_VARIANT="zipf=1.0,permute,seed=7" same_instance "$N" "$M" 8 " zipf1 perm7"

# per graph: 3 baselines + 2 OpenMP + 3 MPI + 2 hybrid = 10 colorings and 10 CSV lines
colored=$(grep -c "The graph is well colored." "$LOG" || true)
csv=$(grep -c "^CSV," "$LOG" || true)
pingpong=$(grep -c "^PINGPONG," "$LOG" || true)
echo "== proper colorings: $colored/20, CSV lines: $csv/20, ping-pong sizes: $pingpong/13"
[[ "$colored" -eq 20 && "$csv" -eq 20 && "$pingpong" -eq 13 ]]

echo "== aggregate"
bash "$ROOT/scripts/aggregate.sh" "$LOG"
