#ifndef COLORING_ALGORITHMS_H
#define COLORING_ALGORITHMS_H

#include <vector>
#include <algorithm>

using namespace std;

int largestDegreeFirst(int submatrixSize, int** submatrix)
{
    int* colors = new int[submatrixSize];
    for (int i = 0; i < submatrixSize; i++) {
        colors[i] = -1;
    }
    vector<pair<int, int>> degrees;
    for (int i = 0; i < submatrixSize; i++) {
        int degree = 0;
        for (int j = 0; j < submatrixSize; j++) {
            if (submatrix[i][j]) {
                degree++;
            }
        }
        degrees.push_back({degree, i});
    }

    sort(degrees.begin(), degrees.end(), greater<pair<int, int>>());

    int chromaticNumber = 0;
    for (int i = 0; i < submatrixSize; i++) {
        int node = degrees[i].second;
        int color = 0;
        while (true) {
            bool ok = true;
            for (int j = 0; j < submatrixSize; j++) {
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
        chromaticNumber = max(chromaticNumber, color + 1);
    }

    return chromaticNumber;
}

// int smallestDegreeLast(int submatrixSize, int** submatrix)
// {

// }

#endif // COLORING_ALGORITHMS_H