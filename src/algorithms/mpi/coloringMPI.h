#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

ColoringResult coloringMPI(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**)) {
    std::vector<lli> colors, labels;
    lli chromaticNumber = 0;
    int submatrixSize, processSize;
    MPI_Request request;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);

    if (processId == 0) {
        std::vector<std::vector<lli>> components = dsu(n, graph);

        int totalComponents = components.size();
        int componentsPerProcess = totalComponents / processSize;
        int remainingComponents = totalComponents % processSize;
        int sendCounts[processSize];
        int displacements[processSize];

        for (int i = 0; i < processSize; i++) {
            sendCounts[i] = componentsPerProcess;
            if (i < remainingComponents) {
                sendCounts[i]++;
            }
        }

        displacements[0] = 0;
        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + sendCounts[i - 1];
        }

        for (int i = 0; i < processSize; i++) {
            std::vector<lli> componentData;
            int sum = 0;
            for (int j = displacements[i]; j < displacements[i] + sendCounts[i]; j++) {
                std::vector<lli> c = components[j];
                lli componentSize = c.size();
                sum += componentSize;
                componentData.push_back(componentSize);
                componentData.insert(componentData.end(), c.begin(), c.end());
                for (lli k = 0; k < componentSize; k++) {
                    for (lli l = 0; l < componentSize; l++) {
                        componentData.push_back(graph[c[k]][c[l]]);
                    }
                }
            }
                        
            MPI_Isend(componentData.data(), componentData.size(), MPI_LONG_LONG_INT, i, 0, MPI_COMM_WORLD, &request);
            MPI_Isend(&sum, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &request);
        }
    }

    std::vector<lli> receivedComponent;
    MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
    int recvCount;
    MPI_Get_count(&status, MPI_LONG_LONG_INT, &recvCount);
    receivedComponent.resize(recvCount);
    MPI_Irecv(receivedComponent.data(), recvCount, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, &request);
    int sumSubmatrixSize;
    MPI_Irecv(&sumSubmatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    int numReceivedComponents = receivedComponent.size();
    int currentIndex = 0;

    while (currentIndex < numReceivedComponents) {
        submatrixSize = receivedComponent[currentIndex];
        currentIndex++;

        for (int i = 0; i < submatrixSize; i++) {
            labels.push_back(receivedComponent[currentIndex + i]);
        }

        currentIndex += submatrixSize;

        lli** submatrix = new lli*[submatrixSize];
        std::cout << "Process " << processId << ":\n";
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new lli[submatrixSize];
            for (int j = 0; j < submatrixSize; j++) {
                submatrix[i][j] = receivedComponent[currentIndex++];
                std::cout << submatrix[i][j] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

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

    lli maxChromaticNumber;
    MPI_Ireduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_LONG_LONG_INT, MPI_MAX, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    int* submatrixSizes = new int[processSize];
    MPI_Gather(&sumSubmatrixSize, 1, MPI_INT, submatrixSizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int* displacements = nullptr;
    if (processId == 0) {
        displacements = new int[processSize];
        displacements[0] = 0;
        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + submatrixSizes[i - 1];
        }
    }

    lli* totalColors = nullptr;
    lli* totalLabels = nullptr;
    if (processId == 0) {
        totalColors = new lli[n];
        totalLabels = new lli[n];
    }

    // std::cout << "Process " << processId << ":\n";
    // std::cout << "Colors: \n";
    // for (int i = 0; i < colors.size(); i++) {
    //     std::cout << colors[i] << " ";
    // }
    // std::cout << std::endl;
    // std::cout << "Labels: \n";
    // for (int i = 0; i < labels.size(); i++) {
    //     std::cout << labels[i] << " ";
    // }
    // std::cout << std::endl << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Igatherv(colors.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalColors, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Igatherv(labels.data(), sumSubmatrixSize, MPI_LONG_LONG_INT, totalLabels, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    if (processId == 0) {
        delete[] displacements;
        delete[] submatrixSizes;
    }

    return ColoringResult(totalColors, maxChromaticNumber, totalLabels);
}

#endif // COLORING_MPI_H
