#include "GraphGenerator.h"
#include "../utils/functions/dfs.h"
#include "../utils/functions/distributeIntegers.h"
#include "../utils/functions/adjacencyListToMatrix.h"
#include "../utils/functions/combineComponentsToAdjacencyMatrix.h"
#include "../utils/functions/initializeGraph.h"
#include "../utils/functions/graphVariant.h"
#include "../utils/functions/edgeListIO.h"
#include <algorithm>
#include <numeric>
#include <GL/glut.h>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <random>

GraphGenerator* GraphGenerator::currentInstance = nullptr;


void GraphGenerator::initializeColors() {
    colors.clear();
    colors.push_back({1.0f, 1.0f, 1.0f});
    colors.push_back({1.0f, 0.0f, 0.0f});
    colors.push_back({0.0f, 1.0f, 0.0f});
    colors.push_back({0.0f, 0.0f, 1.0f});
    colors.push_back({1.0f, 1.0f, 0.0f});
    colors.push_back({1.0f, 0.0f, 1.0f});
    colors.push_back({0.0f, 1.0f, 1.0f});
    colors.push_back({1.0f, 0.5f, 0.0f});
    colors.push_back({0.5f, 0.0f, 1.0f});
    colors.push_back({0.0f, 0.5f, 1.0f});
}

GraphGenerator::GraphGenerator() : Graph() {
    colorIndex = new lli[n];
    initializeColors();
}

GraphGenerator::GraphGenerator(lli n) : Graph (n) {
    colorIndex = new lli[n];
    initializeColors();
}

GraphGenerator::GraphGenerator(lli n, lli **graph) : Graph (n, graph) {
    colorIndex = new lli[n];
    initializeColors();
}

GraphGenerator::~GraphGenerator() {
    delete[] colorIndex;
}

void GraphGenerator::setColorIndex(lli *colorIndex, lli *colorLabels) {
    if (colorLabels == nullptr)
        this->colorIndex = colorIndex;
    else
    {
        for (lli i = 0; i < n; ++i) {
            this->colorIndex[colorLabels[i]] = colorIndex[i];
        }
    }
}

void GraphGenerator::setChromaticNumber(lli chromaticNumber) {
    this->chromaticNumber = chromaticNumber;
}

// Single source of every instance: components from jngen, placed in consecutive vertex ranges,
// optionally relabeled. Both the matrix (v1) and the edge file (v2) are built from this list,
// so the two formats always describe the same graph.
std::vector<Edge> generateEdgeList(lli n, lli m, lli nPrime, const GraphVariant &variant) {
    jngen::config.generateLargeObjects = true;
    // jngen seeds itself from std::random_device by default, so every run produced a different
    // instance. A fixed seed makes graphs reproducible across runs, programs and machines.
    jngen::rnd.seed(variant.seed);
    auto [verticesPerComponent, edgesPerComponent] = componentSizes(n, m, nPrime, variant);
    std::vector<Edge> edges;
    edges.reserve(m);
    lli offset = 0;
    for (lli i = 0; i < nPrime; ++i) {
        jngen::Graph component = jngen::Graph::random(verticesPerComponent[i], edgesPerComponent[i]).connected();
        for (lli u = 0; u < verticesPerComponent[i]; ++u) {
            for (lli v : component.edges(u)) {
                // Keep each edge as (min, max); duplicates from both endpoints are removed below.
                uint32_t a = static_cast<uint32_t>(offset + u), b = static_cast<uint32_t>(offset + v);
                edges.push_back({std::min(a, b), std::max(a, b)});
            }
        }
        offset += verticesPerComponent[i];
    }
    if (variant.permute) {
        std::vector<uint32_t> permutation(n);
        std::iota(permutation.begin(), permutation.end(), 0);
        std::mt19937 generator(variant.seed);
        std::shuffle(permutation.begin(), permutation.end(), generator);
        for (Edge &edge : edges) {
            uint32_t a = permutation[edge.first], b = permutation[edge.second];
            edge = {std::min(a, b), std::max(a, b)};
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    return edges;
}

void GraphGenerator::generateGraph(lli m, lli nPrime) {
    this->m = m;
    this->nPrime = nPrime;
    for (const Edge &edge : generateEdgeList(n, m, nPrime, variant)) {
        graph[edge.first][edge.second] = 1;
        graph[edge.second][edge.first] = 1;
    }
}

std::string GraphGenerator::graphName(lli m, lli nPrime) const {
    return graphFileName(n, m, nPrime, variant);
}

void GraphGenerator::saveEdges(std::string dir) {
    EdgeFileHeader header{n, m, nPrime};
    writeEdgeList(dir + "/" + graphName(m, nPrime) + ".edges", header, edgesFromMatrix(n, graph));
}

void GraphGenerator::drawGraph() {
    glClear(GL_COLOR_BUFFER_BIT);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0, 1.0);

    for (lli i = colors.size(); i < chromaticNumber; ++i) {
        colors.push_back({dis(gen), dis(gen), dis(gen)});
    }

    const float radius = 0.1;

    float *precalcXpos = new float[n];
    float *precalcYpos = new float[n];

    for (lli i = 0; i < n; ++i) {
        precalcXpos[i] = cos(i * 2 * M_PI / n);
        precalcYpos[i] = sin(i * 2 * M_PI / n);
    }

    for (lli i = 0; i < n; ++i) {
        auto color = colors[colorIndex[i]];
        glColor3f(color[0], color[1], color[2]);

        glPushMatrix();
        glTranslatef(precalcXpos[i], precalcYpos[i], 0.0);

        glutSolidSphere(radius, 10, 10);

        glPopMatrix();
    }

    glColor3f(0.0, 0.0, 0.0);
    for (lli i = 0; i < n; ++i) {
        
        glPushMatrix();
        glTranslatef(precalcXpos[i], precalcYpos[i], 0.0);

        const float textOffsetY = precalcYpos[i] >= 0 ? -radius + 0.05 : radius - 0.1;
        const float textOffsetX = precalcXpos[i] >= 0 ? -0.035 * std::to_string(i + 1).size() : -0.035 * std::to_string(i + 1).size() + 0.05;

        glRasterPos3f(textOffsetX, textOffsetY, radius * 1.2);

        std::string vertexNumber = std::to_string(i + 1);
        for (char const& c : vertexNumber)
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);

        glPopMatrix();
    }

    glColor3f(0.5, 0.5, 0.5);

    for (lli i = 0; i < n; ++i) {
        for (lli j = i + 1; j < n; ++j) {
            if (graph[i][j] == 1) {
                glBegin(GL_LINES);
                glVertex3f(precalcXpos[i], precalcYpos[i], 0.0);
                glVertex3f(cos(j * 2 * M_PI / n), sin(j * 2 * M_PI / n), 0.0);
                glEnd();
            }
        }
    }

    glFlush();

    delete[] precalcXpos;
    delete[] precalcYpos;
}

void GraphGenerator::validateGraph() {
    lli edgeCount = 0;
    for (lli i = 0; i < n; ++i) {
        for (lli j = i + 1; j < n; ++j) {
            edgeCount += graph[i][j];
        }
    }
    if (edgeCount != m)
        throw std::length_error("Error: Number of edges " + std::to_string(m) + " does not match the actual number of " + std::to_string(edgeCount) + " edges in the graph.");

    lli connectedComponents = 0;
    bool* visited = new bool[n];
    for (lli i = 0; i < n; ++i) {
        visited[i] = false;
    }

    for (lli i = 0; i < n; ++i) {
        if (!visited[i]) {
            lli componentSize = 0;
            lli componentEdges = 0;

            DFS(graph, n, i, visited, componentSize, componentEdges);
            ++connectedComponents;
        }
    }

    if (connectedComponents != nPrime)
        throw std::length_error("Error: Number of connected components " + std::to_string(nPrime) + " does not match the actual number of " + std::to_string(connectedComponents) + " connected components in the graph.");

    std::cout << "Graph is valid." << std::endl;
    delete[] visited;
}

void GraphGenerator::saveGraph(std::string dir) {
    std::cout << "SAVING GRAPH" << std::endl;
    std::ofstream file;
    std::string name = graphName(m, nPrime);
    file.open(dir + "/" + name + ".txt");
    if (!file.is_open()) {
        throw std::invalid_argument("Error: File " + name + ".txt could not be created.");
    }
    file << n << " " << m << " " << nPrime << std::endl;
    for (lli i = 0; i < n; ++i) {
        file << graph[i][0];
        for (lli j = 1; j < n; ++j) {
            file << " " << graph[i][j];
        }
        file << std::endl;
    }
    file.close();
}

bool GraphGenerator::loadGraph(std::string name, std::string dir) {
    std::ifstream file;
    file.open(dir + "/" + name + ".txt");
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    std::getline(file, line);
    std::istringstream iss(line);
    iss >> n >> m >> nPrime;
    graph = new lli*[n];
    for (lli i = 0; i < n; ++i) {
        graph[i] = new lli[n];
    }
    lli i = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        lli j = 0;
        while (iss >> graph[i][j]) {
            ++j;
        }
        ++i;
    }
    file.close();
    return true;
}

void GraphGenerator::loadIfExistsOrGenerateNewGraph(lli n, lli m, lli nPrime, std::string dir) {
    this->n = n;
    bool isGraphLoaded = loadGraph(graphName(m, nPrime), dir);

    if (!isGraphLoaded) {
        initializeGraph(n, graph);
        generateGraph(m, nPrime);
        validateGraph();
        saveGraph(dir);
    }
}