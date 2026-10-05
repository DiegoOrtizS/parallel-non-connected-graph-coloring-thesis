#ifndef REPORT_H
#define REPORT_H

#include <cstdio>
#include <string>
#include "../types.h"
#include "../structs/PhaseTimes.h"
#include "../structs/CommVolume.h"

// One machine-readable line per run, consumed by scripts/aggregate.sh:
// CSV,algorithm,n,m,k,p,h,t_total,t_dsu,t_pack,t_send,t_color,t_gather,colors
inline void printCsvLine(const std::string &algorithm, lli n, lli m, lli k, int p, int h,
                         double total, const PhaseTimes &phases, lli colors) {
    std::printf("CSV,%s,%lld,%lld,%lld,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%lld\n",
                algorithm.c_str(), n, m, k, p, h, total,
                phases.dsu, phases.pack, phases.send, phases.coloring, phases.gather, colors);
    std::fflush(stdout);
}

// Communication volume of a distributed run, in bytes:
// VOLUME,algorithm,n,m,k,p,h,bytes_dsu,bytes_pack,bytes_send,bytes_gather,bytes_total
inline void printVolumeLine(const std::string &algorithm, lli n, lli m, lli k, int p, int h, const CommVolume &v) {
    std::printf("VOLUME,%s,%lld,%lld,%lld,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f\n",
                algorithm.c_str(), n, m, k, p, h, v.dsu, v.pack, v.send, v.gather,
                v.dsu + v.pack + v.send + v.gather);
    std::fflush(stdout);
}

#endif // REPORT_H
