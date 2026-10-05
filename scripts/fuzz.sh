#!/usr/bin/env bash
# Correctness fuzzing: for many seeds, the root-free v2 must find the k components and assign the
# same colors as the v1 LDF on the same instance. Usage: bash scripts/fuzz.sh [seeds=50]
set -euo pipefail

SEEDS=${1:-50}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/src
N=120; M=600; K=5
mkdir -p "$SRC/data"
for d in generator algorithms/sequential algorithms/mpi_v2; do make -s -C "$SRC/$d"; done

for seed in $(seq 1 "$SEEDS"); do
    for variant in "seed=$seed" "zipf=1.2,permute,seed=$seed"; do
        export GRAPH_VARIANT=$variant
        (cd "$SRC/generator" && ./a.out $N $M $K > /dev/null)
        v1=$(cd "$SRC/algorithms/sequential" && ./a.out $N $M $K | awk -F, '$2 == "seq-ldf-components" {print $14}')
        out=$(cd "$SRC/algorithms/mpi_v2" && mpirun --oversubscribe -np 3 ./a.out $N $M $K)
        v2=$(echo "$out" | awk -F, '$1 == "CSV" {print $14}')
        components=$(echo "$out" | awk '/^Components:/ {print $2}')
        if [[ "$v1" != "$v2" || "$components" != "$K" ]]; then
            echo "MISMATCH for $variant: v1 colors $v1, v2 colors $v2, components $components" >&2
            exit 1
        fi
    done
done
echo "== fuzz: $((2 * SEEDS)) instances, v2 matches v1 colors and finds $K components in all"
