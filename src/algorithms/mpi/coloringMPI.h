#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

ColoringResult coloringMPI(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**)) {
    lli *colors, *labels, chromaticNumber;
    int submatrixSize, processSize;
    MPI_Request request;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);

    if (processId == 0) {
        std::vector<std::vector<lli>> components = dsu(n, graph);

        int nextProcessId = 1;

        for (lli component = 1; component < components.size(); component++) {
            std::vector<lli> c = components[component];
            submatrixSize = c.size();
            lli **submatrix = new lli*[submatrixSize];
            for (lli i = 0; i < submatrixSize; i++) {
                submatrix[i] = new lli[submatrixSize];
                for (lli j = 0; j < submatrixSize; j++) {
                    submatrix[i][j] = graph[c[i]][c[j]];
                }
            }

            MPI_Isend(&submatrixSize, 1, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);

            for (lli i = 0; i < submatrixSize; i++) {
                MPI_Isend(submatrix[i], submatrixSize, MPI_LONG_LONG_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);
            }

            MPI_Isend(c.data(), submatrixSize, MPI_LONG_LONG_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);
            
            nextProcessId++;
        }

        auto c = components[0];
        labels = new lli[c.size()];
        for (lli i = 0; i < c.size(); i++) {
            labels[i] = c[i];
        }

        submatrixSize = c.size();
        lli **submatrix = new lli*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new lli[submatrixSize];
            for (int j = 0; j < submatrixSize; j++) {
                submatrix[i][j] = graph[c[i]][c[j]];
            }
        }

        ColoringResult result = coloringAlgorithm(components[0].size(), submatrix);
        colors = result.colors;
        chromaticNumber = result.chromaticNumber;
    }
    else {
        MPI_Irecv(&submatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);

        lli** submatrix = new lli*[submatrixSize];
        for (lli i = 0; i < submatrixSize; i++) {
            submatrix[i] = new lli[submatrixSize];
        }

        for (lli i = 0; i < submatrixSize; i++) {
            MPI_Irecv(submatrix[i], submatrixSize, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, &request);
        }

        labels = new lli[submatrixSize];
        MPI_Irecv(labels, submatrixSize, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
        
        ColoringResult result = coloringAlgorithm(submatrixSize, submatrix);
        colors = result.colors;
        chromaticNumber = result.chromaticNumber;
    }

    lli maxChromaticNumber;
    MPI_Ireduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_LONG_LONG_INT, MPI_MAX, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    int* submatrixSizes;
    if (processId == 0) {
        submatrixSizes = new int[processSize];
    }
    MPI_Gather(&submatrixSize, 1, MPI_INT, submatrixSizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int* displacements;
    int totalCount = 0;
    if (processId == 0) {
        displacements = new int[processSize];
        displacements[0] = 0;

        for (int i = 1; i < processSize; i++) {
            displacements[i] = displacements[i - 1] + submatrixSizes[i - 1];
        }

        totalCount = displacements[processSize - 1] + submatrixSizes[processSize - 1];
    }

    lli* totalColors = nullptr;
    lli* totalLabels = nullptr;
    if (processId == 0) {
        totalColors = new lli[totalCount];
        totalLabels = new lli[totalCount];
    }

    MPI_Igatherv(colors, submatrixSize, MPI_LONG_LONG_INT, totalColors, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Igatherv(labels, submatrixSize, MPI_LONG_LONG_INT, totalLabels, submatrixSizes, displacements, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, &status);

    return ColoringResult(totalColors, maxChromaticNumber, totalLabels);
}

#endif // COLORING_MPI_H