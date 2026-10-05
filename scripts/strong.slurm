#!/bin/bash
# Strong-scaling campaign: fixed graph, cores c in {1,2,4,8,16,32}, R repetitions + 1 warm-up.
# Submit from scripts/:  sbatch --export=ALL,N=10000,M=1500000,K=32 strong.slurm
#
# TODO(Khipu): adjust PARTITION and the module names to the cluster's configuration.
#SBATCH --job-name=coloring-strong
#SBATCH --partition=standard
#SBATCH --nodes=1
#SBATCH --ntasks=32
#SBATCH --exclusive
#SBATCH --time=04:00:00
#SBATCH --output=results/strong-%j.log

set -euo pipefail
module load gcc openmpi 2>/dev/null || true

N=${N:?set N}; M=${M:?set M}; K=${K:?set K}
R=${R:-10}
CORES="1 2 4 8 16 32"
SRC=$(cd .. && pwd)/src
export OMP_PROC_BIND=close OMP_PLACES=cores

run() {  # run <dir> <command...>: one warm-up (discarded), then R measured repetitions
    local dir=$1; shift
    (cd "$SRC/algorithms/$dir" && "$@" > /dev/null)
    for ((i = 0; i < R; i++)); do (cd "$SRC/algorithms/$dir" && "$@"); done
}

mkdir -p "$SRC/data" results
# Generate the graph once (it is cached in src/data) and build every program.
(cd "$SRC/generator" && make -s && ./a.out "$N" "$M" "$K")
for d in sequential omp mpi hybrid; do (cd "$SRC/algorithms/$d" && make -s); done

run sequential ./a.out "$N" "$M" "$K"
for c in $CORES; do
    export OMP_NUM_THREADS=$c
    run omp ./a.out "$N" "$M" "$K" "$c" rsoc
    run omp ./a.out "$N" "$M" "$K" "$c" components
    export OMP_NUM_THREADS=1
    run mpi mpirun -np "$c" --bind-to core ./a.out "$N" "$M" "$K"
    if (( c >= 2 )); then
        p=$((c / 2))
        export OMP_NUM_THREADS=2
        run hybrid mpirun -np "$p" --map-by "ppr:$p:node:pe=2" ./a.out "$N" "$M" "$K" 2
    fi
done
