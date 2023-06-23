#include "GraphGenerator.h"
#include "../utils/dfs.h"
#include <GL/glut.h>
#include <random>
#include <cmath>
#include <stdexcept>

GraphGenerator::GraphGenerator(int n, int m, int nPrime, float maxDensity, int seed) : Graph(n, m)
{
    this->nPrime = nPrime;
    this->maxDensity = maxDensity;
    this->seed = seed;
}

GraphGenerator::~GraphGenerator()
{
    for (int i = 0; i < n; ++i) {
        delete[] graph[i];
    }
    delete[] graph;
}

void GraphGenerator::generateGraph() {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0, 1.0);

    int verticesPerComponent = n / nPrime;
    int extraVertices = n % nPrime;

    int currentVertex = 0;
    int mAux = m;
    for (int i = 0; i < nPrime; ++i) {
        int componentSize = verticesPerComponent + (i < extraVertices ? 1 : 0);

        for (int u = currentVertex; u < currentVertex + componentSize; ++u) {
            for (int v = u + 1; v < currentVertex + componentSize; ++v) {
                if (dist(rng) < maxDensity) {
                    graph[u][v] = graph[v][u] = 1;
                    --mAux;
                }
            }
        }

        currentVertex += componentSize;
    }

    while (mAux > 0) {
        int u = rng() % n;
        int v = rng() % n;
        if (u != v && graph[u][v] == 0) {
            graph[u][v] = graph[v][u] = 1;
            --mAux;
        }
    }
}

void GraphGenerator::drawGraph() {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0, 1.0, 1.0);

    const float radius = 0.1;

    float *precalcXpos = new float[n];
    float *precalcYpos = new float[n];

    for (int i = 0; i < n; ++i) {
        precalcXpos[i] = cos(i * 2 * M_PI / n);
        precalcYpos[i] = sin(i * 2 * M_PI / n);
    }

    for (int i = 0; i < n; ++i) {
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
        throw std::length_error("Error: Number of edges (m) does not match the actual number of edges in the graph.");
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

            float density = (2.0 * componentEdges) / (componentSize * (componentSize - 1));
            if (density > maxDensity) {
                throw std::length_error("Error: Density of a connected component is greater than the maximum density.");
                delete[] visited;
                return false;
            }
        }
    }

    if (connectedComponents != nPrime) {
        throw std::length_error("Error: Number of connected components does not match the actual number of connected components in the graph.");
        return false;
    }

    delete[] visited;


    return true;
}