#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

void coloringComponents(int submatrixSize, const std::vector<lli> &componentData, ColoringResult (*coloringAlgorithm)(lli, lli**), std::vector<lli> &colors, std::vector<lli> &labels, lli &chromaticNumber) {
    int currentIndex = 0;
    while (currentIndex < componentData.size()) {
        submatrixSize = componentData[currentIndex];
        currentIndex++;

        for (int i = 0; i < submatrixSize; i++) {
            labels.push_back(componentData[currentIndex + i]);
        }

        currentIndex += submatrixSize;

        lli** submatrix = new lli*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new lli[submatrixSize];
            for (int j = 0; j < submatrixSize; j++) {
                submatrix[i][j] = componentData[currentIndex++];
            }
        }

        ColoringResult result = coloringAlgorithm(submatrixSize, submatrix);
        for (int i = 0; i < submatrixSize; i++) {
            colors.push_back(result.colors[i]);
        }
        chromaticNumber = std::max(chromaticNumber, result.chromaticNumber);

        for (int i = 0; i < submatrixSize; i++) {
            delete[] submatrix[i];
        }
        delete[] submatrix;
    }

}

ColoringResult coloringMPI(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**)) {
    std::vector<lli> colors, labels;
    lli chromaticNumber = 0;
    int submatrixSize, processSize, sumSubmatrixSize;
    MPI_Request request;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);

    if (processId == 0) {
        std::vector<std::vector<lli>> components = dsu(n, graph);

        int totalComponents = components.size();
        int componentsPerProcess = totalComponents / processSize;
        int remainingComponents = totalComponents % processSize;
        int sendCounts[processSize];

        for (int i = 0; i < processSize; i++) {
            sendCounts[i] = componentsPerProcess;
            if (i < remainingComponents) {
                sendCounts[i]++;
            }
        }

        int displacements[processSize];
        displacements[0] = 0;
        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + sendCounts[i - 1];
        }

        std::vector<lli> componentData;
        sumSubmatrixSize = 0;
        for (int j = displacements[0]; j < displacements[0] + sendCounts[0]; j++) {
            std::vector<lli> c = components[j];
            lli componentSize = c.size();
            sumSubmatrixSize += componentSize;
            componentData.push_back(componentSize);
            componentData.insert(componentData.end(), c.begin(), c.end());
            for (lli k = 0; k < componentSize; k++) {
                for (lli l = 0; l < componentSize; l++) {
                    componentData.push_back(graph[c[k]][c[l]]);
                }
            }
        }

        coloringComponents(submatrixSize, componentData, coloringAlgorithm, colors, labels, chromaticNumber);

        for (int i = 1; i < processSize; i++) {
            std::vector<lli> sendComponentData;
            int sendSum = 0;
            for (int j = displacements[i]; j < displacements[i] + sendCounts[i]; j++) {
                std::vector<lli> c = components[j];
                lli componentSize = c.size();
                sendSum += componentSize;
                sendComponentData.push_back(componentSize);
                sendComponentData.insert(sendComponentData.end(), c.begin(), c.end());
                for (lli k = 0; k < componentSize; k++) {
                    for (lli l = 0; l < componentSize; l++) {
                        sendComponentData.push_back(graph[c[k]][c[l]]);
                    }
                }
            }

            MPI_Send(sendComponentData.data(), sendComponentData.size(), MPI_LONG_LONG_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&sendSum, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
    else { // Processes other than 0
        int recvCount;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_LONG_LONG_INT, &recvCount);

        std::vector<lli> receivedComponent(recvCount);
        MPI_Recv(receivedComponent.data(), recvCount, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(&sumSubmatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

        coloringComponents(submatrixSize, receivedComponent, coloringAlgorithm, colors, labels, chromaticNumber);
    }

    lli maxChromaticNumber;
    MPI_Ireduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_LONG_LONG_INT, MPI_MAX, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    int* submatrixSizes = new int[processSize];
    MPI_Gather(&sumSubmatrixSize, 1, MPI_INT, submatrixSizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    lli* totalColors = nullptr;
    lli* totalLabels = nullptr;
    if (processId == 0) {
        totalColors = new lli[n];
        totalLabels = new lli[n];
    }

    int *displacements = nullptr;
    if (processId == 0) {
        displacements = new int[processSize];
        displacements[0] = 0;
        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + submatrixSizes[i - 1];
        }
    }

    MPI_Igatherv(colors.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalColors, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Igatherv(labels.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalLabels, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    if (processId == 0) {
        delete[] submatrixSizes;
    }

    return ColoringResult(totalColors, maxChromaticNumber, totalLabels);

}

#endif // COLORING_MPI_H
