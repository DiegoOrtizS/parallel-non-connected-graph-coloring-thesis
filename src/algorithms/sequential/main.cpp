#include <chrono>
#include <iostream>
#include <set>
#include <vector>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/isWellColored.h"
#include "../../utils/functions/report.h"
#include "../mpi/connectedComponents.h"
#include "../mpi/coloringAlgorithms.h"

using Clock = std::chrono::steady_clock;

static double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// (a) First-Fit in vertex order on the whole matrix, with a std::set per vertex.
//     This is the original sequential program; it is kept to explain earlier speedup figures.
ColoringResult firstFitWholeGraph(lli n, lli **adjMatrix) {
    lli *colors = new lli[n]();
    lli chromaticNumber = 0;
    for (lli i = 0; i < n; i++) {
        std::set<lli> C;
        for (lli j = 0; j < n; j++) {
            if (adjMatrix[i][j] && colors[j] != 0) {
                C.insert(colors[j]);
            }
        }
        lli smallestColor = 1;
        while (C.count(smallestColor) > 0) {
            smallestColor++;
        }
        colors[i] = smallestColor;
        chromaticNumber = std::max(chromaticNumber, smallestColor);
    }
    return ColoringResult(colors, chromaticNumber);
}

// (c) The fair baseline of the thesis (Section "Referencia secuencial"): the same work as the
//     parallel versions, i.e. DSU + LDF on each component, executed by a single core.
ColoringResult ldfByComponents(lli n, lli **adjMatrix, PhaseTimes &phases) {
    Clock::time_point t = Clock::now();
    std::vector<std::vector<lli>> components = dsu(n, adjMatrix);
    phases.dsu = secondsSince(t);

    t = Clock::now();
    lli *colors = new lli[n]();
    lli chromaticNumber = 0;
    for (const std::vector<lli> &c : components) {
        lli size = c.size();
        lli **submatrix = new lli*[size];
        for (lli i = 0; i < size; i++) {
            submatrix[i] = new lli[size];
            for (lli j = 0; j < size; j++) {
                submatrix[i][j] = adjMatrix[c[i]][c[j]];
            }
        }
        ColoringResult result = largestDegreeFirst(size, submatrix);
        for (lli i = 0; i < size; i++) {
            colors[c[i]] = result.colors[i];
        }
        chromaticNumber = std::max(chromaticNumber, result.chromaticNumber);
        delete[] result.colors;
        for (lli i = 0; i < size; i++) {
            delete[] submatrix[i];
        }
        delete[] submatrix;
    }
    phases.coloring = secondsSince(t);
    return ColoringResult(colors, chromaticNumber);
}

// Usage: ./a.out n m nPrime
// Prints one CSV line per baseline: seq-ff-full, seq-ldf-full and seq-ldf-components.
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " n m nPrime" << std::endl;
        return 1;
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);

    GraphGenerator graphGenerator;
    graphGenerator.loadIfExistsOrGenerateNewGraph(n, m, nPrime);
    lli **graph = graphGenerator.getGraph();

    {
        PhaseTimes phases;
        Clock::time_point start = Clock::now();
        ColoringResult result = firstFitWholeGraph(n, graph);
        double total = secondsSince(start);
        phases.coloring = total;
        isWellColored(result.colors, n, graph);
        printCsvLine("seq-ff-full", n, m, nPrime, 1, 1, total, phases, result.chromaticNumber);
        delete[] result.colors;
    }
    {
        // (b) LDF on the whole matrix: same heuristic as the MPI version, without decomposition.
        PhaseTimes phases;
        Clock::time_point start = Clock::now();
        ColoringResult result = largestDegreeFirst(n, graph);
        double total = secondsSince(start);
        phases.coloring = total;
        isWellColored(result.colors, n, graph);
        printCsvLine("seq-ldf-full", n, m, nPrime, 1, 1, total, phases, result.chromaticNumber);
        delete[] result.colors;
    }
    {
        PhaseTimes phases;
        Clock::time_point start = Clock::now();
        ColoringResult result = ldfByComponents(n, graph, phases);
        double total = secondsSince(start);
        isWellColored(result.colors, n, graph);
        printCsvLine("seq-ldf-components", n, m, nPrime, 1, 1, total, phases, result.chromaticNumber);
        delete[] result.colors;
    }
    return 0;
}
