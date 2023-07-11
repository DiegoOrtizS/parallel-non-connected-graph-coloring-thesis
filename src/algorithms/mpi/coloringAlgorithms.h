#ifndef COLORING_ALGORITHMS_MPI_H
#define COLORING_ALGORITHMS_MPI_H

#include <vector>
#include <algorithm>
#include "../../utils/structs/ColoringResult.h"

ColoringResult largestDegreeFirst(lli submatrixSize, lli** submatrix)
{
    lli* colors = new lli[submatrixSize];
    std::vector<std::pair<lli, lli>> degrees;
    for (lli i = 0; i < submatrixSize; i++) {
        lli degree = 0;
        for (lli j = 0; j < submatrixSize; j++) {
            if (submatrix[i][j]) {
                degree++;
            }
        }
        degrees.push_back({degree, i});
    }

    std::sort(degrees.begin(), degrees.end(), std::greater<std::pair<lli, lli>>());

    lli chromaticNumber = 0;
    for (lli i = 0; i < submatrixSize; i++) {
        lli node = degrees[i].second;
        lli color = 1;
        while (true) {
            bool ok = true;
            for (lli j = 0; j < submatrixSize; j++) {
                if (submatrix[node][j] && colors[j] == color) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                break;
            }
            color++;
        }
        colors[node] = color;
        chromaticNumber = std::max(chromaticNumber, color);
    }

    return ColoringResult(colors, chromaticNumber);
}

// int smallestDegreeLast(int submatrixSize, int** submatrix)
// {

// }

#endif // COLORING_ALGORITHMS_MPI_H