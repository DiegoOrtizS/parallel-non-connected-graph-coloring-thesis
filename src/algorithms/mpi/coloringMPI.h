#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"
#include "../../utils/structs/PhaseTimes.h"

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

ColoringResult coloringMPI(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**), PhaseTimes *phaseTimes = nullptr) {
    std::vector<lli> colors, labels, componentData;
    lli chromaticNumber = 0;
    int processSize, sumSubmatrixSize = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);
    double tDsu, tPack, tSend, tColoring, tGather, t;

    // Phase 1: connected components (root only).
    MPI_Barrier(MPI_COMM_WORLD);
    t = MPI_Wtime();
    std::vector<std::vector<lli>> components;
    if (processId == 0) {
        components = dsu(n, graph);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    tDsu = MPI_Wtime() - t;

    // Phase 2: the root packs one buffer per process. Contiguous blocks of floor(k/p) or ceil(k/p) components.
    t = MPI_Wtime();
    std::vector<std::vector<lli>> buffers;
    std::vector<int> bufferVertices;
    if (processId == 0) {
        int totalComponents = components.size();
        int componentsPerProcess = totalComponents / processSize;
        int remainingComponents = totalComponents % processSize;
        buffers.resize(processSize);
        bufferVertices.assign(processSize, 0);

        int next = 0;
        for (int r = 0; r < processSize; r++) {
            int count = componentsPerProcess + (r < remainingComponents ? 1 : 0);
            for (int j = next; j < next + count; j++) {
                bufferVertices[r] += components[j].size();
                packComponent(components[j], graph, buffers[r]);
            }
            next += count;
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
            MPI_Send(buffers[r].data(), buffers[r].size(), MPI_LONG_LONG_INT, r, 0, MPI_COMM_WORLD);
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
