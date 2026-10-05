#ifndef GRAPH_VARIANT_H
#define GRAPH_VARIANT_H

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "../types.h"

// Optional generator variants, read from the GRAPH_VARIANT environment variable so that every
// program (generator, sequential, OpenMP, MPI, hybrid) loads the same instance without new arguments.
// Unset or empty means the thesis v1 generator: equal component sizes, contiguous labels.
//
//   GRAPH_VARIANT="zipf=1.2,permute,seed=7"
//     zipf=s   component sizes proportional to (i+1)^(-s): one giant component and many small ones
//     permute  random relabeling of the vertices, so components are not contiguous ranges
//     seed=x   seed of the relabeling (default 42)
struct GraphVariant {
    double zipf = 0.0;
    bool permute = false;
    unsigned seed = 42;

    static GraphVariant fromEnv() {
        GraphVariant variant;
        const char *raw = std::getenv("GRAPH_VARIANT");
        if (raw == nullptr) {
            return variant;
        }
        std::stringstream stream(raw);
        std::string item;
        while (std::getline(stream, item, ',')) {
            if (item.rfind("zipf=", 0) == 0) {
                variant.zipf = std::stod(item.substr(5));
            } else if (item == "permute") {
                variant.permute = true;
            } else if (item.rfind("seed=", 0) == 0) {
                variant.seed = static_cast<unsigned>(std::stoul(item.substr(5)));
            } else if (!item.empty()) {
                throw std::invalid_argument("Unknown GRAPH_VARIANT option: " + item);
            }
        }
        return variant;
    }

    // Suffix of the graph file name; empty for v1 so existing graphs keep their names.
    std::string tag() const {
        std::string result;
        if (zipf > 0) {
            std::ostringstream value;
            value << zipf;
            result += " zipf" + value.str();
        }
        if (permute) {
            result += " perm" + std::to_string(seed);
        }
        return result;
    }
};

// Vertices and edges of each component. v1: as equal as possible. Zipf: n_i proportional to
// (i+1)^(-s) with n_i >= 2, and m_i proportional to the capacity n_i(n_i-1)/2 so that every
// component keeps a similar density, clamped to [n_i - 1, n_i(n_i-1)/2] to stay connected and simple.
inline std::pair<std::vector<lli>, std::vector<lli>> componentSizes(lli n, lli m, lli k, const GraphVariant &variant) {
    std::vector<lli> vertices(k), edges(k);
    if (variant.zipf <= 0) {
        for (lli i = 0; i < k; i++) {
            vertices[i] = n / k + (i < n % k ? 1 : 0);
            edges[i] = m / k + (i < m % k ? 1 : 0);
        }
        return {vertices, edges};
    }

    std::vector<double> weights(k);
    for (lli i = 0; i < k; i++) {
        weights[i] = std::pow(static_cast<double>(i + 1), -variant.zipf);
    }
    double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    lli assigned = 0;
    for (lli i = 0; i < k; i++) {
        vertices[i] = std::max<lli>(2, static_cast<lli>(std::floor(n * weights[i] / total)));
        assigned += vertices[i];
    }
    // Sizes are non-increasing; fix the rounding on the largest component.
    vertices[0] += n - assigned;
    if (vertices[0] < 2) {
        throw std::invalid_argument("Zipf sizes need at least 2 vertices per component");
    }

    auto capacity = [](lli size) { return size * (size - 1) / 2; };
    lli minEdges = 0, maxEdges = 0;
    for (lli size : vertices) {
        minEdges += size - 1;
        maxEdges += capacity(size);
    }
    if (m < minEdges || m > maxEdges) {
        throw std::invalid_argument("m is not feasible for the Zipf component sizes");
    }
    lli assignedEdges = 0;
    for (lli i = 0; i < k; i++) {
        double share = static_cast<double>(m) * capacity(vertices[i]) / maxEdges;
        edges[i] = std::clamp<lli>(static_cast<lli>(std::llround(share)), vertices[i] - 1, capacity(vertices[i]));
        assignedEdges += edges[i];
    }
    // Move the remaining difference one edge at a time, largest components first.
    for (lli i = 0; assignedEdges != m; i = (i + 1) % k) {
        if (assignedEdges < m && edges[i] < capacity(vertices[i])) {
            edges[i]++;
            assignedEdges++;
        } else if (assignedEdges > m && edges[i] > vertices[i] - 1) {
            edges[i]--;
            assignedEdges--;
        }
    }
    return {vertices, edges};
}

// Relabels the vertices with a random permutation: new label of vertex v is permutation[v].
inline void permuteVertices(lli n, lli **graph, unsigned seed) {
    std::vector<lli> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::mt19937 generator(seed);
    std::shuffle(permutation.begin(), permutation.end(), generator);

    std::vector<std::vector<lli>> neighbors(n);
    for (lli u = 0; u < n; u++) {
        for (lli v = 0; v < n; v++) {
            if (graph[u][v]) {
                neighbors[u].push_back(v);
            }
            graph[u][v] = 0;
        }
    }
    for (lli u = 0; u < n; u++) {
        for (lli v : neighbors[u]) {
            graph[permutation[u]][permutation[v]] = 1;
        }
    }
}

#endif // GRAPH_VARIANT_H
