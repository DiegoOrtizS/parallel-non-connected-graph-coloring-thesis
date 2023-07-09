#include "GraphGenerator.h"
#include "../utils/functions/dfs.h"
#include "../utils/functions/distributeIntegers.h"
#include "../utils/functions/adjacencyListToMatrix.h"
#include "../utils/functions/combineComponentsToAdjacencyMatrix.h"
#include <GL/glut.h>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <random>


void GraphGenerator::intializeColors() {
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
    colorIndex = new int[n];
    intializeColors();
}

GraphGenerator::GraphGenerator(int n) : Graph (n) {
    colorIndex = new int[n];
    intializeColors();
}

GraphGenerator::GraphGenerator(int n, int **graph) : Graph (n, graph) {
    colorIndex = new int[n];
    intializeColors();
}

GraphGenerator::~GraphGenerator() {}

void GraphGenerator::setColorIndex(int *colorIndex) {
    this->colorIndex = colorIndex;
}

void GraphGenerator::setChromaticNumber(int chromaticNumber) {
    this->chromaticNumber = chromaticNumber;
}

void GraphGenerator::generateGraph(int m, int nPrime) {
    this->m = m;
    this->nPrime = nPrime;
    std::vector<int> verticesPerComponent = distributeIntegers(n, nPrime);
    std::vector<int> edgesPerComponent = distributeIntegers(m, nPrime);
    std::vector<int**> components;
    for (int i = 0; i < nPrime; ++i) {
        jngen::Graph component = jngen::Graph::random(verticesPerComponent[i], edgesPerComponent[i]).connected();
        components.push_back(adjacencyListToMatrix(component));
    }
    combineComponentsToAdjacencyMatrix(components, verticesPerComponent, graph);
}

void GraphGenerator::drawGraph() {
    glClear(GL_COLOR_BUFFER_BIT);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0, 1.0);

    for (int i = colors.size(); i < chromaticNumber; ++i) {
        colors.push_back({dis(gen), dis(gen), dis(gen)});
    }

    const float radius = 0.1;

    float *precalcXpos = new float[n];
    float *precalcYpos = new float[n];

    for (int i = 0; i < n; ++i) {
        precalcXpos[i] = cos(i * 2 * M_PI / n);
        precalcYpos[i] = sin(i * 2 * M_PI / n);
    }

    for (int i = 0; i < n; ++i) {
        auto color = colors[colorIndex[i]];
        glColor3f(color[0], color[1], color[2]);

        glPushMatrix();
        glTranslatef(precalcXpos[i], precalcYpos[i], 0.0);

        glutSolidSphere(radius, 10, 10);

        glPopMatrix();
    }

    glColor3f(0.0, 0.0, 0.0);
    for (int i = 0; i < n; ++i) {
        
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

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (graph[i][j] == 1) {
                glBegin(GL_LINES);
                glVertex3f(precalcXpos[i], precalcYpos[i], 0.0);
                glVertex3f(cos(j * 2 * M_PI / n), sin(j * 2 * M_PI / n), 0.0);
                glEnd();
            }
        }
    }

    glFlush();
}

bool GraphGenerator::validateGraph() {
    int edgeCount = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            edgeCount += graph[i][j];
        }
    }
    if (edgeCount != m) {
        throw std::length_error("Error: Number of edges " + std::to_string(m) + " does not match the actual number of " + std::to_string(edgeCount) + " edges in the graph.");
        return false;
    }

    int connectedComponents = 0;
    bool* visited = new bool[n];
    for (int i = 0; i < n; ++i) {
        visited[i] = false;
    }

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            int componentSize = 0;
            int componentEdges = 0;

            DFS(graph, n, i, visited, componentSize, componentEdges);
            ++connectedComponents;
        }
    }

    if (connectedComponents != nPrime) {
        throw std::length_error("Error: Number of connected components " + std::to_string(nPrime) + " does not match the actual number of " + std::to_string(connectedComponents) + " connected components in the graph.");
        return false;
    }

    delete[] visited;

    return true;
}

void GraphGenerator::saveGraph(std::string dir) {
    std::ofstream file;
    std::string name = std::to_string(n) + " " + std::to_string(m) + " " + std::to_string(nPrime);
    file.open(dir + "/" + name + ".txt");
    file << name << std::endl;
    for (int i = 0; i < n; ++i) {
        file << graph[i][0];
        for (int j = 1; j < n; ++j) {
            file << " " << graph[i][j];
        }
        file << std::endl;
    }
    file.close();
}

void GraphGenerator::loadGraph(std::string name, std::string dir) {
    std::ifstream file;
    file.open(dir + "/" + name + ".txt");
    if (!file.is_open()) {
        throw std::invalid_argument("Error: File " + name + ".txt does not exist.");
    }
    std::string line;
    std::getline(file, line);
    std::istringstream iss(line);
    iss >> n >> m >> nPrime;
    graph = new int*[n];
    for (int i = 0; i < n; ++i) {
        graph[i] = new int[n];
    }
    int i = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int j = 0;
        while (iss >> graph[i][j]) {
            ++j;
        }
        ++i;
    }
    file.close();
}