#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>
using lli = long long int;

namespace mesquo {
    class Graph
    {
        protected:
            lli n;
            lli **graph;
        
        public:
            Graph();
            Graph(lli n);
            Graph(lli n, lli **graph);
            ~Graph();
            void setN(lli n);
            lli getN();
            void setGraph(lli **graph);
            lli** getGraph();
            friend std::ostream &operator<<(std::ostream &os, const Graph &graph) {
                for (lli i = 0; i < graph.n; i++) {
                    for (lli j = 0; j < graph.n; j++) {
                        os << graph.graph[i][j] << " ";
                    }
                    os << std::endl;
                }
                return os;
            };
    };
}

#endif // GRAPH_H