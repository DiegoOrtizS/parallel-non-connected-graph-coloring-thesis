#ifndef COMM_VOLUME_H
#define COMM_VOLUME_H

// Bytes delivered to other processes in each phase, summed over all processes (deterministic for a
// given instance and p, so unlike CI timings it is valid thesis data). A broadcast of w bytes to p
// processes counts (p - 1) * w: the data every receiver needs, independent of the collective algorithm.
struct CommVolume {
    double dsu = 0;    // v2: (v, parent) pairs sent up the binomial merge tree
    double pack = 0;   // v2: component-id broadcast and per-component edge counts
    double send = 0;   // v1: diagonal blocks; replicate: whole matrix; v2: edge redistribution
    double gather = 0; // colors and labels returned to the root
};

#endif // COMM_VOLUME_H
