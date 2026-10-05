#include <mpi.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "../../utils/types.h"
#include "../../utils/functions/edgeListIO.h"
#include "../../utils/functions/graphVariant.h"
#include "../../utils/functions/report.h"
#include "../../utils/structs/PhaseTimes.h"
#include "../../utils/structs/CommVolume.h"

// v2: root-free component-based coloring on the binary edge list (docs/EXTENSIONS.md, item 8).
//
//   1. Each rank reads a contiguous slice of m/p edges (not timed: I/O).
//   2. dsu:    local union-find on the slice, then a sparse merge up a binomial tree: a rank sends
//              only the pairs (v, root(v)) with root(v) != v, as in SiskinCC (Koohi Esfahani 2025).
//   3. pack:   the root broadcasts the component id of every vertex; edge counts per component are
//              summed with MPI_Allreduce; every rank computes the same LPT assignment; edges are
//              bucketed by the owner of their component.
//   4. send:   MPI_Alltoallv moves every edge to the owner of its component.
//   5. color:  each rank builds a CSR of its components and runs greedy Largest-Degree-First.
//   6. gather: colors return to the root, which verifies them against the edge file (not timed).
//
// The CSV keeps the v1 columns; the mapping of v2 phases to them is the one above.

namespace {

uint32_t findRoot(std::vector<uint32_t> &parent, uint32_t x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

// Links by index (larger root under smaller): the root of a component is its smallest vertex.
void unite(std::vector<uint32_t> &parent, uint32_t a, uint32_t b) {
    a = findRoot(parent, a);
    b = findRoot(parent, b);
    if (a != b) {
        parent[std::max(a, b)] = std::min(a, b);
    }
}

// LPT on estimated cost n_i + m_i (greedy LDF on CSR is linear). Deterministic, so every rank
// computes the same assignment from the same counts without another broadcast.
std::vector<int> assignOwners(const std::vector<uint64_t> &cost, int processSize) {
    std::vector<int> order(cost.size()), owner(cost.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return cost[a] > cost[b]; });
    using Load = std::pair<uint64_t, int>;
    std::priority_queue<Load, std::vector<Load>, std::greater<Load>> leastLoaded;
    for (int r = 0; r < processSize; r++) {
        leastLoaded.push({0, r});
    }
    for (int c : order) {
        auto [load, r] = leastLoaded.top();
        leastLoaded.pop();
        owner[c] = r;
        leastLoaded.push({load + cost[c], r});
    }
    return owner;
}

// Greedy Largest-First on a CSR graph in O(n + m log n); returns the colors (1-based).
std::vector<uint32_t> largestDegreeFirstCsr(const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets) {
    uint32_t vertices = offsets.size() - 1;
    std::vector<uint32_t> order(vertices), colors(vertices, 0);
    std::iota(order.begin(), order.end(), 0);
    auto degree = [&](uint32_t v) { return offsets[v + 1] - offsets[v]; };
    // Same order as the v1 largestDegreeFirst: degree descending, ties by larger index first. Local
    // indices follow global ids, so v1 and v2 color every vertex identically for any p.
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return degree(a) != degree(b) ? degree(a) > degree(b) : a > b;
    });
    uint32_t maxDegree = vertices ? degree(order[0]) : 0;
    std::vector<uint32_t> mark(maxDegree + 2, UINT32_MAX);
    for (uint32_t v : order) {
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            uint32_t c = colors[targets[i]];
            if (c > 0 && c <= degree(v) + 1) {
                mark[c] = v;
            }
        }
        uint32_t color = 1;
        while (mark[color] == v) {
            color++;
        }
        colors[v] = color;
    }
    return colors;
}

}  // namespace

// Usage: mpirun -np P ./a.out n m nPrime   (GRAPH_VARIANT selects the variant, as for the generator)
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (argc < 4) {
        if (rank == 0) {
            std::cerr << "Usage: mpirun -np P " << argv[0] << " n m nPrime" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    lli n = std::stoll(argv[1]), m = std::stoll(argv[2]), nPrime = std::stoll(argv[3]);
    const std::string path = "../../data/" + graphFileName(n, m, nPrime, GraphVariant::fromEnv()) + ".edges";

    EdgeFileHeader header;
    std::vector<Edge> local = readEdgeSlice(path, rank, size, header);
    if (header.n != n || header.m != m) {
        if (rank == 0) {
            std::cerr << "Edge file " << path << " does not match n and m" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    PhaseTimes phases;
    CommVolume volume;
    double sentDsu = 0, sentPack = 0, sentSend = 0, sentGather = 0;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime(), t = start;

    // ---- dsu: local union-find + sparse binomial-tree merge ----
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    for (const Edge &edge : local) {
        unite(parent, edge.first, edge.second);
    }
    for (int step = 1; step < size; step <<= 1) {
        if (rank & step) {
            std::vector<uint32_t> pairs;
            for (uint32_t v = 0; v < n; v++) {
                uint32_t root = findRoot(parent, v);
                if (root != v) {
                    pairs.push_back(v);
                    pairs.push_back(root);
                }
            }
            MPI_Send(pairs.data(), static_cast<int>(pairs.size()), MPI_UINT32_T, rank - step, 7, MPI_COMM_WORLD);
            sentDsu += pairs.size() * sizeof(uint32_t);
            break;
        }
        if (rank + step < size) {
            MPI_Status status;
            int count;
            MPI_Probe(rank + step, 7, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_UINT32_T, &count);
            std::vector<uint32_t> pairs(count);
            MPI_Recv(pairs.data(), count, MPI_UINT32_T, rank + step, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < count; i += 2) {
                unite(parent, pairs[i], pairs[i + 1]);
            }
        }
    }
    // The root now holds the global forest: component id = rank of the root among all roots.
    std::vector<uint32_t> componentOf(n);
    uint32_t components = 0;
    if (rank == 0) {
        std::vector<uint32_t> idOfRoot(n, UINT32_MAX);
        for (uint32_t v = 0; v < n; v++) {
            uint32_t root = findRoot(parent, v);
            if (idOfRoot[root] == UINT32_MAX) {
                idOfRoot[root] = components++;
            }
            componentOf[v] = idOfRoot[root];
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    phases.dsu = MPI_Wtime() - t;

    // ---- pack: share component ids, count edges per component, LPT, bucket edges ----
    t = MPI_Wtime();
    MPI_Bcast(&components, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(componentOf.data(), static_cast<int>(n), MPI_UINT32_T, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        sentPack += (size - 1) * (1.0 + n) * sizeof(uint32_t);
    }
    std::vector<uint64_t> cost(components, 0), localEdges(components, 0);
    for (uint32_t v = 0; v < n; v++) {
        cost[componentOf[v]]++;
    }
    for (const Edge &edge : local) {
        localEdges[componentOf[edge.first]]++;
    }
    std::vector<uint64_t> edgesPerComponent(components);
    MPI_Allreduce(localEdges.data(), edgesPerComponent.data(), static_cast<int>(components), MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);
    sentPack += 2.0 * components * sizeof(uint64_t);
    for (uint32_t c = 0; c < components; c++) {
        cost[c] += edgesPerComponent[c];
    }
    std::vector<int> owner = assignOwners(cost, size);
    std::vector<int> sendCounts(size, 0), sendOffsets(size, 0);
    for (const Edge &edge : local) {
        sendCounts[owner[componentOf[edge.first]]] += 2;
    }
    for (int r = 1; r < size; r++) {
        sendOffsets[r] = sendOffsets[r - 1] + sendCounts[r - 1];
    }
    std::vector<uint32_t> sendBuffer(2 * local.size());
    std::vector<int> cursor = sendOffsets;
    for (const Edge &edge : local) {
        int r = owner[componentOf[edge.first]];
        sendBuffer[cursor[r]++] = edge.first;
        sendBuffer[cursor[r]++] = edge.second;
    }
    std::vector<Edge>().swap(local);
    MPI_Barrier(MPI_COMM_WORLD);
    phases.pack = MPI_Wtime() - t;

    // ---- send: redistribute edges to the owners of their components ----
    t = MPI_Wtime();
    std::vector<int> recvCounts(size), recvOffsets(size, 0);
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    for (int r = 1; r < size; r++) {
        recvOffsets[r] = recvOffsets[r - 1] + recvCounts[r - 1];
    }
    std::vector<uint32_t> received(recvOffsets[size - 1] + recvCounts[size - 1]);
    MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendOffsets.data(), MPI_UINT32_T,
                  received.data(), recvCounts.data(), recvOffsets.data(), MPI_UINT32_T, MPI_COMM_WORLD);
    for (int r = 0; r < size; r++) {
        if (r != rank) {
            sentSend += sendCounts[r] * sizeof(uint32_t) + sizeof(int);
        }
    }
    std::vector<uint32_t>().swap(sendBuffer);
    MPI_Barrier(MPI_COMM_WORLD);
    phases.send = MPI_Wtime() - t;

    // ---- color: CSR of my components, greedy Largest-Degree-First ----
    t = MPI_Wtime();
    std::vector<uint32_t> mine;
    std::vector<int64_t> localId(n, -1);
    for (uint32_t v = 0; v < n; v++) {
        if (owner[componentOf[v]] == rank) {
            localId[v] = mine.size();
            mine.push_back(v);
        }
    }
    std::vector<uint32_t> offsets(mine.size() + 1, 0);
    for (size_t i = 0; i < received.size(); i += 2) {
        offsets[localId[received[i]] + 1]++;
        offsets[localId[received[i + 1]] + 1]++;
    }
    std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
    std::vector<uint32_t> targets(received.size()), fill(offsets.begin(), offsets.end() - 1);
    for (size_t i = 0; i < received.size(); i += 2) {
        uint32_t a = localId[received[i]], b = localId[received[i + 1]];
        targets[fill[a]++] = b;
        targets[fill[b]++] = a;
    }
    std::vector<uint32_t> colors = largestDegreeFirstCsr(offsets, targets);
    uint32_t localColors = colors.empty() ? 0 : *std::max_element(colors.begin(), colors.end());
    phases.coloring = MPI_Wtime() - t;
    MPI_Barrier(MPI_COMM_WORLD);

    // ---- gather: colors (and their vertices) to the root ----
    t = MPI_Wtime();
    uint32_t totalColors = 0;
    MPI_Reduce(&localColors, &totalColors, 1, MPI_UINT32_T, MPI_MAX, 0, MPI_COMM_WORLD);
    int mineCount = mine.size();
    std::vector<int> counts(size), displacements(size, 0);
    MPI_Gather(&mineCount, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int r = 1; r < size && rank == 0; r++) {
        displacements[r] = displacements[r - 1] + counts[r - 1];
    }
    std::vector<uint32_t> allVertices(rank == 0 ? n : 0), allColors(rank == 0 ? n : 0);
    MPI_Gatherv(mine.data(), mineCount, MPI_UINT32_T, allVertices.data(), counts.data(), displacements.data(), MPI_UINT32_T, 0, MPI_COMM_WORLD);
    MPI_Gatherv(colors.data(), mineCount, MPI_UINT32_T, allColors.data(), counts.data(), displacements.data(), MPI_UINT32_T, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        sentGather += sizeof(uint32_t) + sizeof(int) + 2.0 * mineCount * sizeof(uint32_t);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    phases.gather = MPI_Wtime() - t;
    double stop = MPI_Wtime();

    double localTimes[5] = {phases.dsu, phases.pack, phases.send, phases.coloring, phases.gather}, maxTimes[5];
    MPI_Reduce(localTimes, maxTimes, 5, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    double localSent[4] = {sentDsu, sentPack, sentSend, sentGather}, totalSent[4];
    MPI_Reduce(localSent, totalSent, 4, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        phases = {maxTimes[0], maxTimes[1], maxTimes[2], maxTimes[3], maxTimes[4]};
        volume = {totalSent[0], totalSent[1], totalSent[2], totalSent[3]};
        // Verification against every edge of the file (not timed).
        std::vector<uint32_t> colorOf(n, 0);
        for (lli i = 0; i < n; i++) {
            colorOf[allVertices[i]] = allColors[i];
        }
        bool proper = std::all_of(colorOf.begin(), colorOf.end(), [](uint32_t c) { return c > 0; });
        forEachEdge(path, [&](const Edge &edge) {
            if (colorOf[edge.first] == colorOf[edge.second]) {
                proper = false;
            }
        });
        if (!proper) {
            std::cerr << "Error: the coloring is not proper." << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        std::cout << "The graph is well colored." << std::endl;
        std::cout << "Components: " << components << std::endl;
        std::cout << "Number of colors: " << totalColors << std::endl;
        printCsvLine("mpi-v2-ldf", n, m, nPrime, size, 1, stop - start, phases, totalColors);
        printVolumeLine("mpi-v2-ldf", n, m, nPrime, size, 1, volume);
    }

    MPI_Finalize();
    return 0;
}
