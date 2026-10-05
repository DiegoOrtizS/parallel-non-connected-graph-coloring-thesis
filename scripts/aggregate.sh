#!/usr/bin/env bash
# Aggregates the raw "CSV,..." lines printed by every program into one row per configuration.
#
# Usage: ./aggregate.sh results/*.log > summary.csv
#        ./aggregate.sh --thesis N M K results/*.log > ../../Tesis_Coloreo_Grafos/data/graphX.csv
#
# Raw line:  CSV,algorithm,n,m,k,p,h,t_total,t_dsu,t_pack,t_send,t_color,t_gather,colors
# Summary:   algorithm,n,m,k,p,h,cores,reps,t,tstd,ci95,t_dsu,t_pack,t_send,t_color,t_gather,colors
#            (t = mean total time, tstd = sample standard deviation, ci95 = half-width of the 95% CI)
# --thesis:  paradigm,p,threads,cores,t,tstd  for the graph (N, M, K), the format read by pgfplots.
set -euo pipefail

thesis=0
if [[ "${1:-}" == "--thesis" ]]; then
    thesis=1; gn=$2; gm=$3; gk=$4; shift 4
fi

grep -h '^CSV,' "$@" | awk -F, -v thesis="$thesis" -v gn="${gn:-}" -v gm="${gm:-}" -v gk="${gk:-}" '
# Two-sided Student t quantile t_{0.975, df} for small df; 1.96 beyond 30.
function tq(df) {
    split("12.706 4.303 3.182 2.776 2.571 2.447 2.365 2.306 2.262 2.228 2.201 2.179 2.160 2.145 2.131 2.120 2.110 2.101 2.093 2.086 2.080 2.074 2.069 2.064 2.060 2.056 2.052 2.048 2.045 2.042", T, " ")
    return (df >= 1 && df <= 30) ? T[df] : 1.96
}
{
    key = $2 "," $3 "," $4 "," $5 "," $6 "," $7
    if (!(key in reps)) order[++nkeys] = key
    reps[key]++
    sum[key] += $8; sq[key] += $8 * $8
    for (f = 9; f <= 13; f++) ph[key, f] += $f
    colors[key] = $14
}
END {
    if (!thesis) print "algorithm,n,m,k,p,h,cores,reps,t,tstd,ci95,t_dsu,t_pack,t_send,t_color,t_gather,colors"
    else         print "paradigm,p,threads,cores,t,tstd"
    for (i = 1; i <= nkeys; i++) {
        key = order[i]; r = reps[key]
        mean = sum[key] / r
        var = (r > 1) ? (sq[key] - r * mean * mean) / (r - 1) : 0
        if (var < 0) var = 0
        sd = sqrt(var)
        split(key, k, ",")
        cores = k[5] * k[6]
        if (!thesis) {
            ci = (r > 1) ? tq(r - 1) * sd / sqrt(r) : 0
            printf "%s,%d,%d,%.9f,%.9f,%.9f", key, cores, r, mean, sd, ci
            for (f = 9; f <= 13; f++) printf ",%.9f", ph[key, f] / r
            printf ",%s\n", colors[key]
        } else if (k[2] == gn && k[3] == gm && k[4] == gk) {
            name = (k[1] == "mpi-ldf") ? "MPI" : (k[1] == "omp-rsoc") ? "OMP" : (k[1] == "hybrid-rsoc") ? "Hybrid" : (k[1] == "omp-components") ? "OMPComp" : k[1]
            if (name ~ /^seq-/) continue
            printf "%s,%d,%d,%d,%.9f,%.9f\n", name, k[5], k[6], cores, mean, sd
        }
    }
}'
