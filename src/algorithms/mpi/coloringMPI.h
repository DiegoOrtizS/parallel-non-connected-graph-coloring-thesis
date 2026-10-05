#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include <algorithm>
#include <climits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"
#include "../../utils/structs/PhaseTimes.h"
#include "../../utils/structs/CommVolume.h"

void coloringComponents(const std::vector<lli> &componentData, ColoringResult (*coloringAlgorithm)(lli, lli**), std::vector<lli> &colors, std::vector<lli> &labels, lli &chromaticNumber) {
    size_t currentIndex = 0;
    while (currentIndex < componentData.size()) {
        lli submatrixSize = componentData[currentIndex];
        currentIndex++;

        for (lli i = 0; i < submatrixSize; i++) {
            labels.push_back(componentData[currentIndex + i]);
        }
        currentIndex += submatrixSize;

        lli** submatrix = new lli*[submatrixSize];
        for (lli i = 0; i < submatrixSize; i++) {
            submatrix[i] = new lli[submatrixSize];
            for (lli j = 0; j < submatrixSize; j++) {
                submatrix[i][j] = componentData[currentIndex++];
            }
        }

        ColoringResult result = coloringAlgorithm(submatrixSize, submatrix);
        for (lli i = 0; i < submatrixSize; i++) {
            colors.push_back(result.colors[i]);
        }
        chromaticNumber = std::max(chromaticNumber, result.chromaticNumber);
        delete[] result.colors;

        for (lli i = 0; i < submatrixSize; i++) {
            delete[] submatrix[i];
        }
        delete[] submatrix;
    }
}

// Appends (size, labels, block) of component c to data: 1 + n_i + n_i^2 words (Eq. volumen_r).
void packComponent(const std::vector<lli> &c, lli **graph, std::vector<lli> &data) {
    lli componentSize = c.size();
    data.reserve(data.size() + 1 + componentSize + componentSize * componentSize);
    data.push_back(componentSize);
    data.insert(data.end(), c.begin(), c.end());
    for (lli k = 0; k < componentSize; k++) {
        for (lli l = 0; l < componentSize; l++) {
            data.push_back(graph[c[k]][c[l]]);
        }
    }
}

// Distribution options. The defaults are the thesis v1 algorithm.
struct DistributionOptions {
    bool lpt = false;       // Longest Processing Time assignment by estimated cost n_i^2 (v1: contiguous blocks)
    bool replicate = false; // baseline for hypothesis H1: broadcast the whole matrix, each rank extracts its blocks
};

// Reads --lpt and --replicate from argv[first..]; suffix gets "-lpt" / "-replicate" for the CSV algorithm name.
inline DistributionOptions parseDistributionOptions(int argc, char **argv, int first, std::string &suffix) {
    DistributionOptions options;
    for (int i = first; i < argc; i++) {
        std::string flag = argv[i];
        if (flag == "--lpt") {
            options.lpt = true;
            suffix += "-lpt";
        } else if (flag == "--replicate") {
            options.replicate = true;
            suffix += "-replicate";
        } else {
            throw std::invalid_argument("Unknown option: " + flag);
        }
    }
    return options;
}

// Indices of the components assigned to each rank.
inline std::vector<std::vector<int>> assignComponents(const std::vector<std::vector<lli>> &components, int processSize, bool lpt) {
    std::vector<std::vector<int>> assignment(processSize);
    int total = components.size();
    if (!lpt) {
        // Contiguous blocks of floor(k/p) or ceil(k/p) components.
        int perProcess = total / processSize, remaining = total % processSize, next = 0;
        for (int r = 0; r < processSize; r++) {
            int count = perProcess + (r < remaining ? 1 : 0);
            for (int j = next; j < next + count; j++) {
                assignment[r].push_back(j);
            }
            next += count;
        }
        return assignment;
    }
    // LPT (Graham 1969): largest estimated cost first, always to the least loaded rank.
    std::vector<int> order(total);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return components[a].size() > components[b].size(); });
    using Load = std::pair<lli, int>;
    std::priority_queue<Load, std::vector<Load>, std::greater<Load>> leastLoaded;
    for (int r = 0; r < processSize; r++) {
        leastLoaded.push({0, r});
    }
    for (int j : order) {
        auto [load, r] = leastLoaded.top();
        leastLoaded.pop();
        assignment[r].push_back(j);
        lli size = components[j].size();
        leastLoaded.push({load + size * size, r});
    }
    return assignment;
}

ColoringResult coloringMPI(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**), PhaseTimes *phaseTimes = nullptr, const DistributionOptions &options = DistributionOptions(), CommVolume *volume = nullptr) {
    // Bytes this process delivers to others in each phase; summed over processes at the end.
    double sentSend = 0, sentGather = 0;
    std::vector<lli> colors, labels, componentData;
    lli chromaticNumber = 0;
    int processSize, sumSubmatrixSize = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);
    double tDsu, tPack, tSend, tColoring, tGather, t;

    // Phase 1: connected components (root only).
    MPI_Barrier(MPI_COMM_WORLD);
    t = MPI_Wtime();
    std::vector<std::vector<lli>> components;
    std::vector<std::vector<int>> assignment;
    if (processId == 0) {
        components = dsu(n, graph);
        assignment = assignComponents(components, processSize, options.lpt);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    tDsu = MPI_Wtime() - t;

    if (!options.replicate) {
        // Phase 2: the root packs one buffer per process with only that process's diagonal blocks.
        t = MPI_Wtime();
        std::vector<std::vector<lli>> buffers;
        std::vector<int> bufferVertices;
        if (processId == 0) {
            buffers.resize(processSize);
            bufferVertices.assign(processSize, 0);
            for (int r = 0; r < processSize; r++) {
                for (int j : assignment[r]) {
                    bufferVertices[r] += components[j].size();
                    packComponent(components[j], graph, buffers[r]);
                }
            }
            componentData = std::move(buffers[0]);
            sumSubmatrixSize = bufferVertices[0];
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tPack = MPI_Wtime() - t;

        // Phase 3: point-to-point distribution (Eq. sends). Only communication is timed here.
        t = MPI_Wtime();
        if (processId == 0) {
            for (int r = 1; r < processSize; r++) {
                MPI_Send(buffers[r].data(), static_cast<int>(buffers[r].size()), MPI_LONG_LONG_INT, r, 0, MPI_COMM_WORLD);
                sentSend += buffers[r].size() * sizeof(lli) + sizeof(int);
                MPI_Send(&bufferVertices[r], 1, MPI_INT, r, 1, MPI_COMM_WORLD);
                std::vector<lli>().swap(buffers[r]);
            }
        }
        else {
            MPI_Status status;
            int recvCount;
            MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_LONG_LONG_INT, &recvCount);
            componentData.resize(recvCount);
            MPI_Recv(componentData.data(), recvCount, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&sumSubmatrixSize, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tSend = MPI_Wtime() - t;
    }
    else {
        if (n * n > INT_MAX) {
            throw std::invalid_argument("--replicate needs n^2 <= INT_MAX for a single MPI_Bcast");
        }
        // Phase 2 (root): flatten the matrix and describe the components of every rank.
        t = MPI_Wtime();
        std::vector<lli> flat, layout;
        if (processId == 0) {
            flat.resize(n * n);
            for (lli i = 0; i < n; i++) {
                std::copy(graph[i], graph[i] + n, flat.begin() + i * n);
            }
            for (int r = 0; r < processSize; r++) {
                layout.push_back(assignment[r].size());
                for (int j : assignment[r]) {
                    layout.push_back(components[j].size());
                    layout.insert(layout.end(), components[j].begin(), components[j].end());
                }
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tPack = MPI_Wtime() - t;

        // Phase 3: broadcast the whole matrix (Eq. trep) plus the layout.
        t = MPI_Wtime();
        lli layoutSize = layout.size();
        MPI_Bcast(&layoutSize, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        layout.resize(layoutSize);
        MPI_Bcast(layout.data(), static_cast<int>(layoutSize), MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        flat.resize(n * n);
        MPI_Bcast(flat.data(), static_cast<int>(n * n), MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        if (processId == 0) {
            sentSend += static_cast<double>(processSize - 1) * (1 + layoutSize + n * n) * sizeof(lli);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tSend = MPI_Wtime() - t;

        // Each rank extracts its own blocks from its full copy; counted as packing work.
        t = MPI_Wtime();
        size_t position = 0;
        for (int r = 0; r < processSize; r++) {
            lli count = layout[position++];
            for (lli c = 0; c < count; c++) {
                lli size = layout[position++];
                if (r == processId) {
                    componentData.push_back(size);
                    componentData.insert(componentData.end(), layout.begin() + position, layout.begin() + position + size);
                    for (lli a = 0; a < size; a++) {
                        for (lli b = 0; b < size; b++) {
                            componentData.push_back(flat[layout[position + a] * n + layout[position + b]]);
                        }
                    }
                    sumSubmatrixSize += size;
                }
                position += size;
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tPack += MPI_Wtime() - t;
    }

    // Phase 4: independent local coloring, no communication. Each process times its own work.
    t = MPI_Wtime();
    coloringComponents(componentData, coloringAlgorithm, colors, labels, chromaticNumber);
    tColoring = MPI_Wtime() - t;
    MPI_Barrier(MPI_COMM_WORLD);

    // Phase 5: number of colors and gather of colors + labels on the root.
    t = MPI_Wtime();
    lli maxChromaticNumber = 0;
    MPI_Reduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_LONG_LONG_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    std::vector<int> submatrixSizes(processSize), displacements(processSize, 0);
    MPI_Gather(&sumSubmatrixSize, 1, MPI_INT, submatrixSizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    lli* totalColors = nullptr;
    lli* totalLabels = nullptr;
    if (processId == 0) {
        totalColors = new lli[n];
        totalLabels = new lli[n];
        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + submatrixSizes[i - 1];
        }
    }

    MPI_Request requests[2];
    MPI_Igatherv(colors.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalColors, submatrixSizes.data(), displacements.data(), MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &requests[0]);
    MPI_Igatherv(labels.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalLabels, submatrixSizes.data(), displacements.data(), MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &requests[1]);
    MPI_Waitall(2, requests, MPI_STATUSES_IGNORE);
    MPI_Barrier(MPI_COMM_WORLD);
    tGather = MPI_Wtime() - t;

    if (processId != 0) {
        sentGather = sizeof(lli) + sizeof(int) + 2.0 * sumSubmatrixSize * sizeof(lli);
    }
    if (volume != nullptr) {
        double local[2] = {sentSend, sentGather}, global[2];
        MPI_Reduce(local, global, 2, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        if (processId == 0) {
            volume->send = global[0];
            volume->gather = global[1];
        }
    }

    if (phaseTimes != nullptr) {
        double local[5] = {tDsu, tPack, tSend, tColoring, tGather}, global[5];
        MPI_Reduce(local, global, 5, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (processId == 0) {
            phaseTimes->dsu = global[0];
            phaseTimes->pack = global[1];
            phaseTimes->send = global[2];
            phaseTimes->coloring = global[3];
            phaseTimes->gather = global[4];
        }
    }

    return ColoringResult(totalColors, maxChromaticNumber, totalLabels);
}

#endif // COLORING_MPI_H
