#!/usr/bin/env bash
# Builds every program and runs each one on a small graph. Fails unless every run prints a
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

for d in generator algorithms/sequential algorithms/omp algorithms/mpi algorithms/hybrid; do
    echo "== build $d"
    make -s -C "$SRC/$d"
done

run() {  # run <dir> <command...>
    local dir=$1; shift
    echo "== $dir: $*"
    (cd "$SRC/$dir" && "$@") | tee -a "$LOG"
}

run generator ./a.out "$N" "$M" "$K"
run algorithms/sequential ./a.out "$N" "$M" "$K"
OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$N" "$M" "$K" 2 rsoc
OMP_NUM_THREADS=2 run algorithms/omp ./a.out "$N" "$M" "$K" 2 components
OMP_NUM_THREADS=1 run algorithms/mpi "${MPIRUN[@]}" -np 4 ./a.out "$N" "$M" "$K"
OMP_NUM_THREADS=2 run algorithms/hybrid "${MPIRUN[@]}" -np 2 ./a.out "$N" "$M" "$K" 2

# 3 sequential baselines + 2 OpenMP + MPI + hybrid = 7 colorings and 7 CSV lines
colored=$(grep -c "The graph is well colored." "$LOG" || true)
csv=$(grep -c "^CSV," "$LOG" || true)
echo "== proper colorings: $colored/7, CSV lines: $csv/7"
[[ "$colored" -eq 7 && "$csv" -eq 7 ]]

echo "== aggregate"
bash "$ROOT/scripts/aggregate.sh" "$LOG"
