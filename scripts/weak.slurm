#!/bin/bash
# Weak-scaling campaign: k = p components of fixed size (NC vertices, MC edges each),
# so n = p*NC and m = p*MC. The per-process coloring work is constant (prediction P3 in the thesis).
# Submit from scripts/:  sbatch --export=ALL,NC=312,MC=46875 weak.slurm
#
# Memory: the adjacency matrix uses 8*n^2 bytes; NC=312 and p=32 give n~10^4, i.e. ~0.8 GB.
# TODO(Khipu): adjust PARTITION and the module names to the cluster's configuration.
#SBATCH --job-name=coloring-weak
#SBATCH --partition=standard
#SBATCH --nodes=1
#SBATCH --ntasks=32
#SBATCH --exclusive
#SBATCH --time=04:00:00
#SBATCH --output=results/weak-%j.log

set -euo pipefail
module load gcc openmpi 2>/dev/null || true

NC=${NC:?set NC}; MC=${MC:?set MC}
R=${R:-10}
PROCS="1 2 4 8 16 32"
SRC=$(cd .. && pwd)/src
export OMP_NUM_THREADS=1 OMP_PROC_BIND=close OMP_PLACES=cores

(cd "$SRC/generator" && make -s)
for d in sequential mpi; do (cd "$SRC/algorithms/$d" && make -s); done

for p in $PROCS; do
    n=$((p * NC)); m=$((p * MC))
    (cd "$SRC/generator" && ./a.out "$n" "$m" "$p")
    (cd "$SRC/algorithms/sequential" && ./a.out "$n" "$m" "$p" > /dev/null)
    for ((i = 0; i < R; i++)); do
        (cd "$SRC/algorithms/sequential" && ./a.out "$n" "$m" "$p")
        (cd "$SRC/algorithms/mpi" && mpirun -np "$p" --bind-to core ./a.out "$n" "$m" "$p")
    done
done
