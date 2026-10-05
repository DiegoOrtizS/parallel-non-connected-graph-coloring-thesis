#include <mpi.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "../../utils/types.h"
#include "../../utils/functions/edgeListIO.h"
#include "../../utils/functions/graphVariant.h"
#include "../../utils/functions/report.h"
#include "../../utils/structs/PhaseTimes.h"
#include "../../utils/structs/CommVolume.h"

// Component-based coloring on the binary edge list (docs/EXTENSIONS.md, items 7 and 8).
//
// Root-free mode (default), CSV name mpi-v2-ldf:
//   dsu    local union-find on a slice of m/p edges, then a sparse binomial-tree merge of the pairs
//          (v, root(v)) with root(v) != v (as in SiskinCC, Koohi Esfahani 2025)
//   pack   broadcast of component ids, MPI_Allreduce of edges per component, replicated LPT,
//          bucketing of edges by owner
//   send   MPI_Alltoallv of edges to the owners of their components
//
// Root mode (--root), CSV name mpi-v2r-ldf: the v1 algorithm on the edge list instead of the matrix
//   dsu    union-find of all edges on the root, Theta(n + m)
//   pack   the root encodes each component as a bitmap of n_i^2 bits, a list of its edges, or a
//          list of the edges of its complement (16-bit indices when n_i <= 65536), whichever has
//          the fewest bytes (--blocks=auto, default; or --blocks=bitmap|edges|complement)
//   send   MPI_Send per process, or one MPI_Scatterv with --scatterv
//   color  complement blocks are colored on the complement, never expanded:
//          --color=ldf (default) same colors as LDF on G; --color=matching coloring from a maximal
//          matching of the complement; --color=cliques disjoint cliques of the complement, larger
//          first, improved by local search, then a matching of the rest; --color=best the fewest
//          colors of the three per component (never more than LDF). Bitmap blocks of density at
//          least 1/2 are turned into their complement at the owner for matching, cliques and best
//
// Both modes then build a CSR of each process's components, color it with greedy Largest-Degree-First
// in the same vertex order as the v1 program (so every version assigns the same colors), gather the
// colors on the root and verify them against the edge file. Reading the file and verifying are not timed.
//
// GRAPH_FILE=<path> reads any .edges file (for example, a converted real graph) instead of the
// generated instance "<n> <m> <k>[variant].edges".

namespace {

struct Options {
    bool root = false;
    bool scatterv = false;
    std::string blocks = "auto";
    std::string color = "ldf";
};

uint32_t findRoot(std::vector<uint32_t> &parent, uint32_t x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

// Links by index (larger root under smaller): the root of a component is its smallest vertex.
void unite(std::vector<uint32_t> &parent, uint32_t a, uint32_t b) {
    a = findRoot(parent, a);
    b = findRoot(parent, b);
    if (a != b) {
        parent[std::max(a, b)] = std::min(a, b);
    }
}

// Component id of every vertex, numbered by smallest vertex; returns the number of components.
uint32_t labelComponents(std::vector<uint32_t> &parent, std::vector<uint32_t> &componentOf) {
    uint32_t n = parent.size(), components = 0;
    std::vector<uint32_t> idOfRoot(n, UINT32_MAX);
    componentOf.assign(n, 0);
    for (uint32_t v = 0; v < n; v++) {
        uint32_t root = findRoot(parent, v);
        if (idOfRoot[root] == UINT32_MAX) {
            idOfRoot[root] = components++;
        }
        componentOf[v] = idOfRoot[root];
    }
    return components;
}

// LPT on estimated cost n_i + m_i (greedy LDF on CSR is linear). Deterministic, so every rank
// computes the same assignment from the same counts.
std::vector<int> assignOwners(const std::vector<uint64_t> &cost, int processSize) {
    std::vector<int> order(cost.size()), owner(cost.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return cost[a] > cost[b]; });
    using Load = std::pair<uint64_t, int>;
    std::priority_queue<Load, std::vector<Load>, std::greater<Load>> leastLoaded;
    for (int r = 0; r < processSize; r++) {
        leastLoaded.push({0, r});
    }
    for (int c : order) {
        auto [load, r] = leastLoaded.top();
        leastLoaded.pop();
        owner[c] = r;
        leastLoaded.push({load + cost[c], r});
    }
    return owner;
}

// Greedy Largest-First on a CSR graph; returns the colors (1-based). Same order as the v1
// largestDegreeFirst: degree descending, ties by larger index first. Local indices follow
// global ids within a component, so every version colors every vertex identically.
std::vector<uint32_t> largestDegreeFirstCsr(const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets) {
    uint32_t vertices = offsets.size() - 1;
    std::vector<uint32_t> order(vertices), colors(vertices, 0);
    std::iota(order.begin(), order.end(), 0);
    auto degree = [&](uint32_t v) { return offsets[v + 1] - offsets[v]; };
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return degree(a) != degree(b) ? degree(a) > degree(b) : a > b;
    });
    uint32_t maxDegree = vertices ? degree(order[0]) : 0;
    std::vector<uint32_t> mark(maxDegree + 2, UINT32_MAX);
    for (uint32_t v : order) {
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            uint32_t c = colors[targets[i]];
            if (c > 0 && c <= degree(v) + 1) {
                mark[c] = v;
            }
        }
        uint32_t color = 1;
        while (mark[color] == v) {
            color++;
        }
        colors[v] = color;
    }
    return colors;
}

// CSR of `vertices` local vertices from a list of local edges (pairs of local indices).
void buildCsr(uint32_t vertices, const std::vector<uint32_t> &localEdges, std::vector<uint32_t> &offsets, std::vector<uint32_t> &targets) {
    offsets.assign(vertices + 1, 0);
    for (size_t i = 0; i < localEdges.size(); i += 2) {
        offsets[localEdges[i] + 1]++;
        offsets[localEdges[i + 1] + 1]++;
    }
    std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
    targets.assign(localEdges.size(), 0);
    std::vector<uint32_t> fill(offsets.begin(), offsets.end() - 1);
    for (size_t i = 0; i < localEdges.size(); i += 2) {
        targets[fill[localEdges[i]]++] = localEdges[i + 1];
        targets[fill[localEdges[i + 1]]++] = localEdges[i];
    }
}

// A component as received by its owner, before coloring.
//   kind 0 (bitmap) and 1 (edges): `pairs` are edges of G in local indices
//   kind 2 (complement):           `pairs` are edges of the complement H = G-bar in local indices
struct Block {
    std::vector<uint32_t> labels;
    uint32_t kind = 1;
    std::vector<uint32_t> pairs;
};

// Bytes of each encoding of a component with n vertices and m edges (index width w bits):
//   edges 2*(w/8)*m, bitmap n^2/8, complement 2*(w/8)*(n(n-1)/2 - m).
// With w = 16 the cheapest is: edges for D < 1/16, bitmap for 1/16 <= D <= 15/16, complement for D > 15/16.
struct EncodingSizes {
    uint64_t edges, bitmap, complement;
};

uint32_t indexWidth(uint64_t size) { return size <= 65536 ? 16 : 32; }

EncodingSizes encodingSizes(uint64_t size, uint64_t edgeCount) {
    uint64_t pairBytes = indexWidth(size) == 16 ? 4 : 8;
    uint64_t complementCount = size * (size - 1) / 2 - edgeCount;
    return {pairBytes * edgeCount, (size * size + 31) / 32 * 4, pairBytes * complementCount};
}

// Pairs are written one word per pair with 16-bit indices, or two words with 32-bit indices.
void pushPairs(const std::vector<uint32_t> &pairs, uint32_t width, std::vector<uint32_t> &out) {
    out.push_back(static_cast<uint32_t>(pairs.size() / 2));
    for (size_t i = 0; i < pairs.size(); i += 2) {
        if (width == 16) {
            out.push_back(pairs[i] | pairs[i + 1] << 16);
        } else {
            out.push_back(pairs[i]);
            out.push_back(pairs[i + 1]);
        }
    }
}

size_t readPairs(const std::vector<uint32_t> &message, size_t position, uint32_t width, std::vector<uint32_t> &pairs) {
    uint32_t count = message[position++];
    pairs.reserve(pairs.size() + 2 * count);
    for (uint32_t i = 0; i < count; i++) {
        if (width == 16) {
            uint32_t word = message[position++];
            pairs.push_back(word & 0xFFFFu);
            pairs.push_back(word >> 16);
        } else {
            pairs.push_back(message[position++]);
            pairs.push_back(message[position++]);
        }
    }
    return position;
}

// Message format per component (uint32 words): n_i, labels[n_i], kind, width, payload
//   kind 0 bitmap:     ceil(n_i^2 / 32) words, bit a*n_i + b set for every edge a < b of G
//   kind 1 edges:      edges of G as packed pairs
//   kind 2 complement: edges of the complement as packed pairs
// `blocks` = auto picks the encoding with the fewest bytes; counts[kind] records the choice.
void encodeComponent(const std::vector<uint32_t> &labels, const std::vector<uint32_t> &localEdges, const std::string &blocks,
                     std::vector<uint32_t> &out, uint64_t counts[3]) {
    uint64_t size = labels.size(), edgeCount = localEdges.size() / 2;
    EncodingSizes bytes = encodingSizes(size, edgeCount);
    uint32_t kind;
    if (blocks == "bitmap") {
        kind = 0;
    } else if (blocks == "edges") {
        kind = 1;
    } else if (blocks == "complement") {
        kind = 2;
    } else {
        kind = bytes.bitmap < bytes.edges ? 0 : 1;
        if (bytes.complement < std::min(bytes.bitmap, bytes.edges)) {
            kind = 2;
        }
    }
    counts[kind]++;
    uint32_t width = indexWidth(size);
    out.push_back(static_cast<uint32_t>(size));
    out.insert(out.end(), labels.begin(), labels.end());
    out.push_back(kind);
    out.push_back(width);
    if (kind == 1) {
        pushPairs(localEdges, width, out);
        return;
    }
    // Bitmap and complement both need the adjacency of the component: Theta(n_i^2) on the encoder.
    std::vector<uint32_t> bits((size * size + 31) / 32, 0);
    for (size_t i = 0; i < localEdges.size(); i += 2) {
        uint64_t a = std::min(localEdges[i], localEdges[i + 1]), b = std::max(localEdges[i], localEdges[i + 1]);
        uint64_t bit = a * size + b;
        bits[bit / 32] |= 1u << (bit % 32);
    }
    if (kind == 0) {
        out.insert(out.end(), bits.begin(), bits.end());
        return;
    }
    std::vector<uint32_t> missing;
    for (uint64_t a = 0; a < size; a++) {
        for (uint64_t b = a + 1; b < size; b++) {
            uint64_t bit = a * size + b;
            if (!(bits[bit / 32] >> (bit % 32) & 1u)) {
                missing.push_back(static_cast<uint32_t>(a));
                missing.push_back(static_cast<uint32_t>(b));
            }
        }
    }
    pushPairs(missing, width, out);
}

// Splits a message into blocks. A bitmap is expanded to edges of G; a complement is kept as is,
// so a dense component is never expanded to its Theta(n_i^2) edges.
std::vector<Block> decodeBlocks(const std::vector<uint32_t> &message) {
    std::vector<Block> blocks;
    size_t position = 0;
    while (position < message.size()) {
        Block block;
        uint64_t size = message[position++];
        block.labels.assign(message.begin() + position, message.begin() + position + size);
        position += size;
        block.kind = message[position++];
        uint32_t width = message[position++];
        if (block.kind == 0) {
            for (uint64_t a = 0; a < size; a++) {
                for (uint64_t b = a + 1; b < size; b++) {
                    uint64_t bit = a * size + b;
                    if (message[position + bit / 32] >> (bit % 32) & 1u) {
                        block.pairs.push_back(static_cast<uint32_t>(a));
                        block.pairs.push_back(static_cast<uint32_t>(b));
                    }
                }
            }
            position += (size * size + 31) / 32;
        } else {
            position = readPairs(message, position, width, block.pairs);
        }
        blocks.push_back(std::move(block));
    }
    return blocks;
}

// Greedy Largest-Degree-First run on the complement H of a dense component, with the same order and
// the same choices as on G, so it assigns exactly the same colors. deg_G(v) = n - 1 - deg_H(v).
// Color c is free for v iff every vertex already colored c is a neighbor of v in H. Any color that
// no H-neighbor of v uses has a G-neighbor of v, so only the colors seen among the H-neighbors and
// one new color are candidates: O(n log n + m-bar) instead of Theta(m) on G.
std::vector<uint32_t> largestDegreeFirstComplement(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets) {
    std::vector<uint32_t> order(size), colors(size, 0), classSize(size + 2, 0), hits(size + 2, 0), stamp(size + 2, UINT32_MAX);
    std::iota(order.begin(), order.end(), 0);
    auto degreeH = [&](uint32_t v) { return offsets[v + 1] - offsets[v]; };
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return degreeH(a) != degreeH(b) ? degreeH(a) < degreeH(b) : a > b;
    });
    uint32_t used = 0;
    for (uint32_t v : order) {
        uint32_t best = used + 1;
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            uint32_t c = colors[targets[i]];
            if (c == 0) {
                continue;
            }
            if (stamp[c] != v) {
                stamp[c] = v;
                hits[c] = 0;
            }
            if (++hits[c] == classSize[c] && c < best) {
                best = c;
            }
        }
        colors[v] = best;
        classSize[best]++;
        used = std::max(used, best);
    }
    return colors;
}

// Karp-Sipser maximal matching of the complement H on the vertices not yet taken: degree-1 vertices
// first, then the vertex of smallest remaining degree, matched to its free neighbor of smallest
// degree. Marks the matched vertices as taken and returns the matched pairs.
std::vector<std::vector<uint32_t>> karpSipserMatching(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets, std::vector<char> &taken) {
    std::vector<uint32_t> degree(size, 0);
    for (uint32_t v = 0; v < size; v++) {
        for (uint32_t i = offsets[v]; i < offsets[v + 1] && !taken[v]; i++) {
            degree[v] += !taken[targets[i]] && targets[i] != v;
        }
    }
    using Entry = std::pair<uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    for (uint32_t v = 0; v < size; v++) {
        if (degree[v] > 0) {
            queue.push({degree[v], v});
        }
    }
    std::vector<std::vector<uint32_t>> pairs;
    while (!queue.empty()) {
        auto [d, v] = queue.top();
        queue.pop();
        if (taken[v] || d != degree[v] || d == 0) {
            continue;
        }
        uint32_t partner = UINT32_MAX;
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            uint32_t u = targets[i];
            if (!taken[u] && u != v && (partner == UINT32_MAX || degree[u] < degree[partner])) {
                partner = u;
            }
        }
        if (partner == UINT32_MAX) {
            continue;
        }
        taken[v] = taken[partner] = 1;
        pairs.push_back({v, partner});
        for (uint32_t w : {v, partner}) {
            for (uint32_t i = offsets[w]; i < offsets[w + 1]; i++) {
                uint32_t u = targets[i];
                if (!taken[u] && degree[u] > 0) {
                    degree[u]--;
                    queue.push({degree[u], u});
                }
            }
        }
    }
    return pairs;
}

// Colors from disjoint cliques of the complement H (independent sets of G): each class gets one
// color and every other vertex its own, numbered by smallest vertex. Uses n - sum(|Q| - 1) colors.
std::vector<uint32_t> colorsFromClasses(uint32_t size, const std::vector<std::vector<uint32_t>> &classes) {
    std::vector<uint32_t> classOf(size, UINT32_MAX), colors(size, 0);
    for (uint32_t c = 0; c < classes.size(); c++) {
        for (uint32_t v : classes[c]) {
            classOf[v] = c;
        }
    }
    uint32_t next = 0;
    for (uint32_t v = 0; v < size; v++) {
        if (colors[v] != 0) {
            continue;
        }
        colors[v] = ++next;
        if (classOf[v] != UINT32_MAX) {
            for (uint32_t u : classes[classOf[v]]) {
                colors[u] = next;
            }
        }
    }
    return colors;
}

// Coloring from a maximal matching of H (Karp-Sipser): each matched pair is non-adjacent in G and
// shares a color. Uses n - |M| colors, at most n - nu(H)/2.
std::vector<uint32_t> matchingColoringComplement(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets) {
    std::vector<char> taken(size, 0);
    return colorsFromClasses(size, karpSipserMatching(size, offsets, targets, taken));
}

// Appends every clique of H with at least 3 vertices that extends `clique` with vertices of
// `candidates` (common neighbors larger than every member, sorted). Each clique is listed once,
// from its smallest vertex. Stops at maxCliques.
void listCliques(std::vector<uint32_t> &clique, const std::vector<uint32_t> &candidates, const std::vector<std::vector<uint32_t>> &higher,
                 std::vector<std::vector<uint32_t>> &cliques, size_t maxCliques) {
    for (size_t i = 0; i < candidates.size() && cliques.size() < maxCliques; i++) {
        uint32_t w = candidates[i];
        clique.push_back(w);
        if (clique.size() >= 3) {
            cliques.push_back(clique);
        }
        std::vector<uint32_t> next;
        std::set_intersection(candidates.begin() + i + 1, candidates.end(), higher[w].begin(), higher[w].end(), std::back_inserter(next));
        if (!next.empty()) {
            listCliques(clique, next, higher, cliques, maxCliques);
        }
        clique.pop_back();
    }
}

// Coloring from cliques and a matching of H. A clique of s vertices of H is an independent set of
// G and saves s - 1 colors, (s - 1)/s per vertex against 1/2 for a matched pair, so vertex-disjoint
// cliques of at least 3 vertices are taken first, greedily: larger cliques first, then those whose
// vertices lie in the fewest cliques. Then a Karp-Sipser matching of the vertices left. Uses
// n - sum(|Q| - 1) colors; with only triangles, n - 2T - |M|. The cliques are listed by extending
// each vertex with its larger neighbors, short lists because H is sparse in dense components; the
// listing stops at maxCliques, and the packing then uses the cliques listed so far. A local search
// then improves the packing (below).
std::vector<uint32_t> cliqueColoringComplement(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets) {
    const size_t maxCliques = 500000;
    std::vector<std::vector<uint32_t>> higher(size), cliques;
    for (uint32_t v = 0; v < size; v++) {
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            if (targets[i] > v) {
                higher[v].push_back(targets[i]);
            }
        }
        std::sort(higher[v].begin(), higher[v].end());
    }
    std::vector<uint32_t> clique;
    for (uint32_t v = 0; v < size && cliques.size() < maxCliques; v++) {
        clique.assign(1, v);
        listCliques(clique, higher[v], higher, cliques, maxCliques);
    }

    std::vector<uint64_t> inCliques(size, 0), conflicts(cliques.size(), 0);
    for (const auto &c : cliques) {
        for (uint32_t v : c) {
            inCliques[v]++;
        }
    }
    for (size_t j = 0; j < cliques.size(); j++) {
        for (uint32_t v : cliques[j]) {
            conflicts[j] += inCliques[v];
        }
    }
    std::vector<size_t> order(cliques.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (cliques[a].size() != cliques[b].size()) {
            return cliques[a].size() > cliques[b].size();
        }
        return conflicts[a] != conflicts[b] ? conflicts[a] < conflicts[b] : cliques[a] < cliques[b];
    });

    // Greedy packing: owner[v] is the chosen clique that covers v, or NONE.
    const uint32_t NONE = UINT32_MAX;
    std::vector<uint32_t> owner(size, NONE);
    for (size_t j : order) {
        if (std::all_of(cliques[j].begin(), cliques[j].end(), [&](uint32_t v) { return owner[v] == NONE; })) {
            for (uint32_t v : cliques[j]) {
                owner[v] = j;
            }
        }
    }
    auto countColors = [&]() {
        std::vector<char> taken(size, 0);
        uint64_t saved = 0;
        for (uint32_t v = 0; v < size; v++) {
            taken[v] = owner[v] != NONE;
            if (taken[v] && cliques[owner[v]][0] == v) {
                saved += cliques[owner[v]].size() - 1;
            }
        }
        return size - saved - karpSipserMatching(size, offsets, targets, taken).size();
    };

    // Local search: insert a clique that is not chosen, evict the chosen cliques it overlaps, and
    // refill each freed vertex with its first clique, in greedy order, whose vertices are all free.
    // A move is kept only if the packing weight sum(|Q| - 2) rises (cheap filter) and the color
    // count, with a Karp-Sipser matching of the free vertices, falls. The work is capped by a count
    // of scanned entries, not by time, so the result is deterministic.
    std::vector<std::vector<uint32_t>> of(size);
    for (size_t r = 0; r < order.size(); r++) {
        for (uint32_t v : cliques[order[r]]) {
            of[v].push_back(order[r]);  // in greedy order
        }
    }
    const uint64_t budget = 200000000;
    uint64_t work = 0;
    uint64_t current = countColors();
    bool improved = true;
    for (int round = 0; round < 50 && improved && work < budget; round++) {
        improved = false;
        for (size_t r = 0; r < order.size() && work < budget; r++) {
            uint32_t j = order[r];
            const std::vector<uint32_t> &inserted = cliques[j];
            if (owner[inserted[0]] == j) {
                continue;
            }
            std::vector<uint32_t> evicted;
            int64_t delta = static_cast<int64_t>(inserted.size()) - 2;
            for (uint32_t v : inserted) {
                if (owner[v] != NONE && std::find(evicted.begin(), evicted.end(), owner[v]) == evicted.end()) {
                    evicted.push_back(owner[v]);
                    delta -= static_cast<int64_t>(cliques[owner[v]].size()) - 2;
                }
            }
            std::vector<std::pair<uint32_t, uint32_t>> undo;
            for (uint32_t e : evicted) {
                for (uint32_t v : cliques[e]) {
                    undo.push_back({v, owner[v]});
                    owner[v] = NONE;
                }
            }
            for (uint32_t v : inserted) {
                undo.push_back({v, owner[v]});
                owner[v] = j;
            }
            for (uint32_t e : evicted) {
                for (uint32_t v : cliques[e]) {
                    for (size_t k = 0; k < of[v].size() && owner[v] == NONE; k++) {
                        const std::vector<uint32_t> &refill = cliques[of[v][k]];
                        work += refill.size();
                        if (std::all_of(refill.begin(), refill.end(), [&](uint32_t u) { return owner[u] == NONE; })) {
                            for (uint32_t u : refill) {
                                undo.push_back({u, owner[u]});
                                owner[u] = of[v][k];
                            }
                            delta += static_cast<int64_t>(refill.size()) - 2;
                        }
                    }
                }
            }
            work += inserted.size();
            if (delta > 0) {
                work += size + offsets[size];
                uint64_t colors = countColors();
                if (colors < current) {
                    current = colors;
                    improved = true;
                    continue;
                }
            }
            for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
                owner[it->first] = it->second;
            }
        }
    }

    std::vector<char> taken(size, 0);
    std::vector<std::vector<uint32_t>> classes;
    for (uint32_t v = 0; v < size; v++) {
        taken[v] = owner[v] != NONE;
        if (taken[v] && cliques[owner[v]][0] == v) {
            classes.push_back(cliques[owner[v]]);
        }
    }
    for (auto &pair : karpSipserMatching(size, offsets, targets, taken)) {
        classes.push_back(std::move(pair));
    }
    return colorsFromClasses(size, classes);
}

// CSR of the complement of a graph given by its CSR: for each v, mark its neighbors and take every
// other unmarked vertex. Theta(n^2) time, like decoding a bitmap, and O(n) extra memory.
void complementCsr(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets,
                   std::vector<uint32_t> &complementOffsets, std::vector<uint32_t> &complementTargets) {
    std::vector<uint32_t> mark(size, UINT32_MAX);
    complementOffsets.assign(size + 1, 0);
    complementTargets.clear();
    for (uint32_t v = 0; v < size; v++) {
        for (uint32_t i = offsets[v]; i < offsets[v + 1]; i++) {
            mark[targets[i]] = v;
        }
        for (uint32_t u = 0; u < size; u++) {
            if (u != v && mark[u] != v) {
                complementTargets.push_back(u);
            }
        }
        complementOffsets[v + 1] = complementTargets.size();
    }
}

std::vector<uint32_t> colorOnComplement(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets,
                                        const std::string &method, const std::vector<uint32_t> &greedy);

// Colors one block with the requested method; returns colors in the block's local order.
std::vector<uint32_t> colorBlock(const Block &block, const std::string &method) {
    uint32_t size = block.labels.size();
    std::vector<uint32_t> offsets, targets;
    buildCsr(size, block.pairs, offsets, targets);
    if (block.kind != 2) {
        std::vector<uint32_t> greedy = largestDegreeFirstCsr(offsets, targets);
        // The complement methods also apply to bitmap blocks of density at least 1/2, where the
        // complement is the sparser graph: 2m / (n(n - 1)) >= 1/2, with targets.size() = 2m. Edge
        // blocks (density below 1/16) and sparser bitmap blocks keep LDF on G.
        bool dense = size > 1 && 2 * static_cast<uint64_t>(targets.size()) >= static_cast<uint64_t>(size) * (size - 1);
        if (method == "ldf" || block.kind != 0 || !dense) {
            return greedy;
        }
        std::vector<uint32_t> complementOffsets, complementTargets;
        complementCsr(size, offsets, targets, complementOffsets, complementTargets);
        return colorOnComplement(size, complementOffsets, complementTargets, method, greedy);
    }
    std::vector<uint32_t> greedy = largestDegreeFirstComplement(size, offsets, targets);
    if (method == "ldf") {
        return greedy;
    }
    return colorOnComplement(size, offsets, targets, method, greedy);
}

// Matching, clique or best coloring of a component from the CSR of its complement H; `greedy` is
// its LDF coloring, which is the same on G and on H.
std::vector<uint32_t> colorOnComplement(uint32_t size, const std::vector<uint32_t> &offsets, const std::vector<uint32_t> &targets,
                                        const std::string &method, const std::vector<uint32_t> &greedy) {
    std::vector<uint32_t> matched = matchingColoringComplement(size, offsets, targets);
    if (method == "matching") {
        return matched;
    }
    std::vector<uint32_t> packed = cliqueColoringComplement(size, offsets, targets);
    if (method == "cliques") {
        return packed;
    }
    // best: per component, the coloring with the fewest colors (first of ldf, matching, cliques on ties).
    auto count = [](const std::vector<uint32_t> &colors) { return colors.empty() ? 0u : *std::max_element(colors.begin(), colors.end()); };
    const std::vector<uint32_t> *best = &greedy;
    for (const std::vector<uint32_t> *candidate : {&matched, &packed}) {
        if (count(*candidate) < count(*best)) {
            best = candidate;
        }
    }
    return *best;
}

Options parseOptions(int argc, char **argv, std::string &suffix) {
    Options options;
    for (int i = 4; i < argc; i++) {
        std::string flag = argv[i];
        if (flag == "--root") {
            options.root = true;
        } else if (flag == "--scatterv") {
            options.scatterv = true;
        } else if (flag.rfind("--blocks=", 0) == 0) {
            options.blocks = flag.substr(9);
            if (options.blocks != "auto" && options.blocks != "bitmap" && options.blocks != "edges" && options.blocks != "complement") {
                throw std::invalid_argument("--blocks must be auto, bitmap, edges or complement");
            }
        } else if (flag.rfind("--color=", 0) == 0) {
            options.color = flag.substr(8);
            if (options.color != "ldf" && options.color != "matching" && options.color != "cliques" && options.color != "best") {
                throw std::invalid_argument("--color must be ldf, matching, cliques or best");
            }
        } else {
            throw std::invalid_argument("Unknown option: " + flag);
        }
    }
    if (!options.root && (options.scatterv || options.blocks != "auto" || options.color != "ldf")) {
        throw std::invalid_argument("--scatterv, --blocks and --color only apply to --root");
    }
    if (options.root) {
        suffix = "r";
        if (options.blocks != "auto") {
            suffix += "-" + options.blocks;
        }
        if (options.scatterv) {
            suffix += "-scatterv";
        }
    }
    return options;
}

}  // namespace

// Usage: mpirun -np P ./a.out n m nPrime [--root [--blocks=auto|bitmap|edges|complement] [--scatterv] [--color=ldf|matching|cliques|best]]
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (argc < 4) {
        if (rank == 0) {
            std::cerr << "Usage: mpirun -np P " << argv[0] << " n m nPrime [--root [--blocks=auto|bitmap|edges] [--scatterv]]" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    lli n = std::stoll(argv[1]), m = std::stoll(argv[2]), nPrime = std::stoll(argv[3]);
    std::string suffix;
    Options options = parseOptions(argc, argv, suffix);
    const char *graphFile = std::getenv("GRAPH_FILE");
    const std::string path = graphFile != nullptr ? std::string(graphFile)
        : "../../data/" + graphFileName(n, m, nPrime, GraphVariant::fromEnv()) + ".edges";
    const std::string algorithm = "mpi-v2" + suffix + "-" + options.color;
    uint64_t blockCounts[3] = {0, 0, 0};  // components sent as bitmap, edges, complement (--root)
    std::vector<Block> blocks;

    // Input (not timed): a slice per rank (root-free) or the whole list on the root (--root).
    EdgeFileHeader header = readEdgeHeader(path);
    if (header.n != n || header.m != m) {
        if (rank == 0) {
            std::cerr << "Edge file " << path << " has n = " << header.n << ", m = " << header.m << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    std::vector<Edge> local;
    if (!options.root) {
        local = readEdgeSlice(path, rank, size, header);
    } else if (rank == 0) {
        local = readEdgeRange(path, 0, header.m);
    }

    PhaseTimes phases;
    double sentDsu = 0, sentPack = 0, sentSend = 0, sentGather = 0;
    std::vector<uint32_t> mine, localEdges;
    uint32_t components = 0;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime(), t = start;

    if (!options.root) {
        // ---- dsu: local union-find + sparse binomial-tree merge ----
        std::vector<uint32_t> parent(n);
        std::iota(parent.begin(), parent.end(), 0);
        for (const Edge &edge : local) {
            unite(parent, edge.first, edge.second);
        }
        for (int step = 1; step < size; step <<= 1) {
            if (rank & step) {
                std::vector<uint32_t> pairs;
                for (uint32_t v = 0; v < n; v++) {
                    uint32_t root = findRoot(parent, v);
                    if (root != v) {
                        pairs.push_back(v);
                        pairs.push_back(root);
                    }
                }
                MPI_Send(pairs.data(), static_cast<int>(pairs.size()), MPI_UINT32_T, rank - step, 7, MPI_COMM_WORLD);
                sentDsu += pairs.size() * sizeof(uint32_t);
                break;
            }
            if (rank + step < size) {
                MPI_Status status;
                int count;
                MPI_Probe(rank + step, 7, MPI_COMM_WORLD, &status);
                MPI_Get_count(&status, MPI_UINT32_T, &count);
                std::vector<uint32_t> pairs(count);
                MPI_Recv(pairs.data(), count, MPI_UINT32_T, rank + step, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                for (int i = 0; i < count; i += 2) {
                    unite(parent, pairs[i], pairs[i + 1]);
                }
            }
        }
        std::vector<uint32_t> componentOf(n);
        if (rank == 0) {
            components = labelComponents(parent, componentOf);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        phases.dsu = MPI_Wtime() - t;

        // ---- pack: share component ids, count edges per component, LPT, bucket edges ----
        t = MPI_Wtime();
        MPI_Bcast(&components, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
        MPI_Bcast(componentOf.data(), static_cast<int>(n), MPI_UINT32_T, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            sentPack += (size - 1) * (1.0 + n) * sizeof(uint32_t);
        }
        std::vector<uint64_t> cost(components, 0), localCount(components, 0), edgesPerComponent(components);
        for (uint32_t v = 0; v < n; v++) {
            cost[componentOf[v]]++;
        }
        for (const Edge &edge : local) {
            localCount[componentOf[edge.first]]++;
        }
        MPI_Allreduce(localCount.data(), edgesPerComponent.data(), static_cast<int>(components), MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);
        sentPack += 2.0 * components * sizeof(uint64_t);
        for (uint32_t c = 0; c < components; c++) {
            cost[c] += edgesPerComponent[c];
        }
        std::vector<int> owner = assignOwners(cost, size);
        std::vector<int> sendCounts(size, 0), sendOffsets(size, 0);
        for (const Edge &edge : local) {
            sendCounts[owner[componentOf[edge.first]]] += 2;
        }
        for (int r = 1; r < size; r++) {
            sendOffsets[r] = sendOffsets[r - 1] + sendCounts[r - 1];
        }
        std::vector<uint32_t> sendBuffer(2 * local.size());
        std::vector<int> cursor = sendOffsets;
        for (const Edge &edge : local) {
            int r = owner[componentOf[edge.first]];
            sendBuffer[cursor[r]++] = edge.first;
            sendBuffer[cursor[r]++] = edge.second;
        }
        std::vector<Edge>().swap(local);
        MPI_Barrier(MPI_COMM_WORLD);
        phases.pack = MPI_Wtime() - t;

        // ---- send: redistribute edges to the owners of their components ----
        t = MPI_Wtime();
        std::vector<int> recvCounts(size), recvOffsets(size, 0);
        MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
        for (int r = 1; r < size; r++) {
            recvOffsets[r] = recvOffsets[r - 1] + recvCounts[r - 1];
        }
        std::vector<uint32_t> received(recvOffsets[size - 1] + recvCounts[size - 1]);
        MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendOffsets.data(), MPI_UINT32_T,
                      received.data(), recvCounts.data(), recvOffsets.data(), MPI_UINT32_T, MPI_COMM_WORLD);
        for (int r = 0; r < size; r++) {
            if (r != rank) {
                sentSend += sendCounts[r] * sizeof(uint32_t) + sizeof(int);
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        phases.send = MPI_Wtime() - t;

        // Local view for the coloring phase (timed there): my vertices and edges in local indices.
        t = MPI_Wtime();
        std::vector<int64_t> localId(n, -1);
        for (uint32_t v = 0; v < n; v++) {
            if (owner[componentOf[v]] == rank) {
                localId[v] = mine.size();
                mine.push_back(v);
            }
        }
        localEdges.resize(received.size());
        for (size_t i = 0; i < received.size(); i++) {
            localEdges[i] = static_cast<uint32_t>(localId[received[i]]);
        }
    } else {
        // ---- dsu: all edges on the root, Theta(n + m) ----
        std::vector<uint32_t> componentOf;
        if (rank == 0) {
            std::vector<uint32_t> parent(n);
            std::iota(parent.begin(), parent.end(), 0);
            for (const Edge &edge : local) {
                unite(parent, edge.first, edge.second);
            }
            components = labelComponents(parent, componentOf);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        phases.dsu = MPI_Wtime() - t;

        // ---- pack: the root encodes each component for its owner ----
        t = MPI_Wtime();
        std::vector<std::vector<uint32_t>> buffers(size);
        std::vector<uint32_t> ownBuffer;
        if (rank == 0) {
            std::vector<std::vector<uint32_t>> labels(components), edgesOf(components);
            std::vector<uint32_t> positionInComponent(n);
            for (uint32_t v = 0; v < n; v++) {
                positionInComponent[v] = labels[componentOf[v]].size();
                labels[componentOf[v]].push_back(v);
            }
            for (const Edge &edge : local) {
                std::vector<uint32_t> &list = edgesOf[componentOf[edge.first]];
                list.push_back(positionInComponent[edge.first]);
                list.push_back(positionInComponent[edge.second]);
            }
            std::vector<uint64_t> cost(components);
            for (uint32_t c = 0; c < components; c++) {
                cost[c] = labels[c].size() + edgesOf[c].size() / 2;
            }
            std::vector<int> owner = assignOwners(cost, size);
            for (uint32_t c = 0; c < components; c++) {
                encodeComponent(labels[c], edgesOf[c], options.blocks, buffers[owner[c]], blockCounts);
            }
            ownBuffer = std::move(buffers[0]);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        phases.pack = MPI_Wtime() - t;

        // ---- send: point-to-point, or one MPI_Scatterv ----
        t = MPI_Wtime();
        std::vector<uint32_t> message;
        if (options.scatterv) {
            std::vector<int> counts(size, 0), offsets(size, 0);
            std::vector<uint32_t> all;
            if (rank == 0) {
                counts[0] = ownBuffer.size();
                for (int r = 1; r < size; r++) {
                    counts[r] = buffers[r].size();
                }
                for (int r = 1; r < size; r++) {
                    offsets[r] = offsets[r - 1] + counts[r - 1];
                }
                all.reserve(offsets[size - 1] + counts[size - 1]);
                all.insert(all.end(), ownBuffer.begin(), ownBuffer.end());
                for (int r = 1; r < size; r++) {
                    all.insert(all.end(), buffers[r].begin(), buffers[r].end());
                    sentSend += buffers[r].size() * sizeof(uint32_t) + sizeof(int);
                }
            }
            int myCount = 0;
            MPI_Scatter(counts.data(), 1, MPI_INT, &myCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
            message.resize(myCount);
            MPI_Scatterv(all.data(), counts.data(), offsets.data(), MPI_UINT32_T,
                         message.data(), myCount, MPI_UINT32_T, 0, MPI_COMM_WORLD);
        } else if (rank == 0) {
            for (int r = 1; r < size; r++) {
                MPI_Send(buffers[r].data(), static_cast<int>(buffers[r].size()), MPI_UINT32_T, r, 3, MPI_COMM_WORLD);
                sentSend += buffers[r].size() * sizeof(uint32_t);
            }
            message = std::move(ownBuffer);
        } else {
            MPI_Status status;
            int count;
            MPI_Probe(0, 3, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_UINT32_T, &count);
            message.resize(count);
            MPI_Recv(message.data(), count, MPI_UINT32_T, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        MPI_Bcast(&components, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        phases.send = MPI_Wtime() - t;

        // Decoding is part of the coloring phase (timed there).
        t = MPI_Wtime();
        blocks = decodeBlocks(message);
    }

    // ---- color (t started above): one CSR for the root-free mode; per component in --root mode,
    // on G for bitmap and edge blocks and on the complement for complement blocks ----
    std::vector<uint32_t> colors;
    if (!options.root) {
        std::vector<uint32_t> offsets, targets;
        buildCsr(mine.size(), localEdges, offsets, targets);
        colors = largestDegreeFirstCsr(offsets, targets);
    } else {
        for (const Block &block : blocks) {
            std::vector<uint32_t> blockColors = colorBlock(block, options.color);
            mine.insert(mine.end(), block.labels.begin(), block.labels.end());
            colors.insert(colors.end(), blockColors.begin(), blockColors.end());
        }
    }
    uint32_t localColors = colors.empty() ? 0 : *std::max_element(colors.begin(), colors.end());
    phases.coloring = MPI_Wtime() - t;
    MPI_Barrier(MPI_COMM_WORLD);

    // ---- gather: colors (and their vertices) to the root ----
    t = MPI_Wtime();
    uint32_t totalColors = 0;
    MPI_Reduce(&localColors, &totalColors, 1, MPI_UINT32_T, MPI_MAX, 0, MPI_COMM_WORLD);
    int mineCount = mine.size();
    std::vector<int> counts(size), displacements(size, 0);
    MPI_Gather(&mineCount, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int r = 1; r < size && rank == 0; r++) {
        displacements[r] = displacements[r - 1] + counts[r - 1];
    }
    std::vector<uint32_t> allVertices(rank == 0 ? n : 0), allColors(rank == 0 ? n : 0);
    MPI_Gatherv(mine.data(), mineCount, MPI_UINT32_T, allVertices.data(), counts.data(), displacements.data(), MPI_UINT32_T, 0, MPI_COMM_WORLD);
    MPI_Gatherv(colors.data(), mineCount, MPI_UINT32_T, allColors.data(), counts.data(), displacements.data(), MPI_UINT32_T, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        sentGather += sizeof(uint32_t) + sizeof(int) + 2.0 * mineCount * sizeof(uint32_t);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    phases.gather = MPI_Wtime() - t;
    double stop = MPI_Wtime();

    double localTimes[5] = {phases.dsu, phases.pack, phases.send, phases.coloring, phases.gather}, maxTimes[5];
    MPI_Reduce(localTimes, maxTimes, 5, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    double localSent[4] = {sentDsu, sentPack, sentSend, sentGather}, totalSent[4];
    MPI_Reduce(localSent, totalSent, 4, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        PhaseTimes reported{maxTimes[0], maxTimes[1], maxTimes[2], maxTimes[3], maxTimes[4]};
        CommVolume volume{totalSent[0], totalSent[1], totalSent[2], totalSent[3]};
        // Verification against every edge of the file (not timed).
        std::vector<uint32_t> colorOf(n, 0);
        for (lli i = 0; i < n; i++) {
            colorOf[allVertices[i]] = allColors[i];
        }
        bool proper = std::all_of(colorOf.begin(), colorOf.end(), [](uint32_t c) { return c > 0; });
        forEachEdge(path, [&](const Edge &edge) {
            if (colorOf[edge.first] == colorOf[edge.second]) {
                proper = false;
            }
        });
        if (!proper) {
            std::cerr << "Error: the coloring is not proper." << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        std::cout << "The graph is well colored." << std::endl;
        std::cout << "Components: " << components << std::endl;
        std::cout << "Number of colors: " << totalColors << std::endl;
        printCsvLine(algorithm, n, m, nPrime, size, 1, stop - start, reported, totalColors);
        printVolumeLine(algorithm, n, m, nPrime, size, 1, volume);
        if (options.root) {
            // Encoding chosen per component: BLOCKS,algorithm,n,m,k,p,bitmap,edges,complement
            std::cout << "BLOCKS," << algorithm << "," << n << "," << m << "," << nPrime << "," << size << ","
                      << blockCounts[0] << "," << blockCounts[1] << "," << blockCounts[2] << std::endl;
        }
    }

    MPI_Finalize();
    return 0;
}
