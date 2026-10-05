#ifndef EDGE_LIST_IO_H
#define EDGE_LIST_IO_H

#include <sys/types.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "../types.h"

// Binary edge-list format used by the v2 programs (no n x n matrix, so graphs with n >= 10^5 fit):
//   header: three int64 values n, m, k
//   body:   m pairs of uint32 (u, v) with u < v, sorted
// Values are written in the machine's byte order; little-endian is assumed (x86 CI and Khipu).
using Edge = std::pair<uint32_t, uint32_t>;

struct EdgeFileHeader {
    int64_t n = 0, m = 0, k = 0;
};
static_assert(sizeof(EdgeFileHeader) == 24, "EdgeFileHeader must have no padding");

inline void writeEdgeList(const std::string &path, const EdgeFileHeader &header, const std::vector<Edge> &edges) {
    FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        throw std::runtime_error("Cannot create edge file " + path);
    }
    std::fwrite(&header, sizeof(header), 1, file);
    std::vector<uint32_t> flat;
    flat.reserve(2 * edges.size());
    for (const Edge &edge : edges) {
        flat.push_back(edge.first);
        flat.push_back(edge.second);
    }
    std::fwrite(flat.data(), sizeof(uint32_t), flat.size(), file);
    std::fclose(file);
}

// Reads the header and checks that the file size matches it, so a truncated copy fails loudly.
inline EdgeFileHeader readEdgeHeader(const std::string &path) {
    FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        throw std::runtime_error("Cannot open edge file " + path + " (run the generator first)");
    }
    EdgeFileHeader header;
    bool ok = std::fread(&header, sizeof(header), 1, file) == 1;
    fseeko(file, 0, SEEK_END);
    off_t size = ftello(file);
    std::fclose(file);
    off_t expected = static_cast<off_t>(sizeof(EdgeFileHeader)) + static_cast<off_t>(header.m) * 2 * sizeof(uint32_t);
    if (!ok || size != expected) {
        throw std::runtime_error("Edge file " + path + " has " + std::to_string(size) + " bytes, expected " + std::to_string(expected));
    }
    return header;
}

// Reads edges [first, last) of the file. Offsets use off_t, so files beyond 2 GB work.
inline std::vector<Edge> readEdgeRange(const std::string &path, int64_t first, int64_t last) {
    std::vector<Edge> edges;
    if (last <= first) {
        return edges;
    }
    FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        throw std::runtime_error("Cannot open edge file " + path);
    }
    off_t offset = static_cast<off_t>(sizeof(EdgeFileHeader)) + static_cast<off_t>(first) * 2 * sizeof(uint32_t);
    fseeko(file, offset, SEEK_SET);
    std::vector<uint32_t> flat(2 * (last - first));
    size_t read = std::fread(flat.data(), sizeof(uint32_t), flat.size(), file);
    std::fclose(file);
    if (read != flat.size()) {
        throw std::runtime_error("Truncated edge file " + path);
    }
    edges.reserve(last - first);
    for (size_t i = 0; i < flat.size(); i += 2) {
        edges.push_back({flat[i], flat[i + 1]});
    }
    return edges;
}

// Contiguous slice of the edges for process `rank` out of `size`.
inline std::vector<Edge> readEdgeSlice(const std::string &path, int rank, int size, EdgeFileHeader &header) {
    header = readEdgeHeader(path);
    int64_t first = header.m * rank / size;
    int64_t last = header.m * (rank + 1) / size;
    return readEdgeRange(path, first, last);
}

// Streams the whole file in chunks, calling visit on every edge (used to verify colorings).
inline void forEachEdge(const std::string &path, const std::function<void(const Edge &)> &visit) {
    EdgeFileHeader header = readEdgeHeader(path);
    const int64_t chunk = int64_t(1) << 22;
    for (int64_t first = 0; first < header.m; first += chunk) {
        for (const Edge &edge : readEdgeRange(path, first, std::min(header.m, first + chunk))) {
            visit(edge);
        }
    }
}

// Edges of an adjacency matrix (upper triangle), in sorted order.
inline std::vector<Edge> edgesFromMatrix(lli n, lli **matrix) {
    std::vector<Edge> edges;
    for (lli u = 0; u < n; u++) {
        for (lli v = u + 1; v < n; v++) {
            if (matrix[u][v]) {
                edges.push_back({static_cast<uint32_t>(u), static_cast<uint32_t>(v)});
            }
        }
    }
    return edges;
}

#endif // EDGE_LIST_IO_H
