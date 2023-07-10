#ifndef COLORING_MPI_H
#define COLORING_MPI_H

#include <mpi.h>
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

ColoringResult coloringMPI(const int &processId, const int &n, int **graph, ColoringResult (*coloringAlgorithm)(int, int**)) {
    int *colors, *labels, chromaticNumber, submatrixSize;
    MPI_Request request;
    MPI_Status status;

    if (processId == 0) {
        std::vector<std::vector<int>> components = dsu(n, graph);
        
        int nextProcessId = 1;

        for (int component = 1; component < components.size(); component++) {
            std::vector<int> c = components[component];
            submatrixSize = c.size();
            int **submatrix = new int*[submatrixSize];
            for (int i = 0; i < submatrixSize; i++) {
                submatrix[i] = new int[submatrixSize];
                for (int j = 0; j < submatrixSize; j++) {
                    submatrix[i][j] = graph[c[i]][c[j]];
                }
            }

            MPI_Isend(&submatrixSize, 1, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);

            for (int i = 0; i < submatrixSize; i++) {
                MPI_Isend(submatrix[i], submatrixSize, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);
            }

            MPI_Isend(c.data(), submatrixSize, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);
            
            nextProcessId++;
        }

        auto c = components[0];
        labels = new int[c.size()];
        for (int i = 0; i < c.size(); i++) {
            labels[i] = c[i];
        }

        submatrixSize = c.size();
        int **submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
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

        // allocate memory for the submatrix
        int** submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
        }

        for (int i = 0; i < submatrixSize; i++) {
            MPI_Irecv(submatrix[i], submatrixSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        }

        labels = new int[submatrixSize];
        MPI_Irecv(labels, submatrixSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
        
        ColoringResult result = coloringAlgorithm(submatrixSize, submatrix);
        colors = result.colors;
        chromaticNumber = result.chromaticNumber;
    }

    int maxChromaticNumber;
    MPI_Ireduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD, &request);

    int *totalColors = new int[n];
    int *totalLabels = new int[n];
    MPI_Igather(colors, submatrixSize, MPI_INT, totalColors, submatrixSize, MPI_INT, 0, MPI_COMM_WORLD, &request);
    MPI_Igather(labels, submatrixSize, MPI_INT, totalLabels, submatrixSize, MPI_INT, 0, MPI_COMM_WORLD, &request);

    MPI_Wait(&request, &status);

    return ColoringResult(totalColors, maxChromaticNumber, totalLabels);
}

#endif // COLORING_MPI_H