#include <mpi.h>
#include <omp.h>
#include <vector>
#include <unordered_map>

using namespace std;

const int V = 6, E = 6;
int size, processId;

int find(int parent[], int x) {
    if (parent[x] == x) {
        return x;
    }
    return parent[x] = find(parent, parent[x]);
}

void union_set(int parent[], int rank[], int x, int y) {
    int px = find(parent, x);
    int py = find(parent, y);

    if (rank[px] > rank[py]) {
        parent[py] = px;
    } else {
        parent[px] = py;
        if (rank[px] == rank[py]) {
            rank[py]++;
        }
    }
}

vector<vector<int>> dsu(int matrix[V][E]) {
    vector<vector<int>> components;
    int parent[V];
    int rank[V];

    for (int i = 0; i < V; i++) {
        parent[i] = i;
        rank[i] = 0;
    }

    for (int i = 0; i < V; i++) {
        for (int j = 0; j < E; j++) {
            if (matrix[i][j]) {
                union_set(parent, rank, i, j);
            }
        }
    }

    unordered_map<int, vector<int>> comp_map;

    for (int i = 0; i < V; i++) {
        comp_map[find(parent, i)].push_back(i);
    }

    for (auto& [p, c] : comp_map) {
        components.push_back(c);
    }

    return components;
}

int main() {
    MPI_Init(NULL, NULL);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    omp_set_num_threads(1);

    if (processId == 0) {
        int matrix[V][E] = {
            {0, 0, 1, 0, 0, 0},
            {0, 0, 0, 1, 1, 0},
            {1, 0, 0, 0, 0, 1},
            {0, 1, 0, 0, 1, 0},
            {0, 1, 0, 1, 0, 0},
            {0, 0, 1, 0, 0, 0}
        };

        auto components = dsu(matrix);
        
        int nextProcessId = 1;
        for (auto& c : components) {
            int submatrixSize = c.size();
            int **submatrix = new int*[submatrixSize];
            for (int i = 0; i < submatrixSize; i++) {
                submatrix[i] = new int[submatrixSize];
                for (int j = 0; j < submatrixSize; j++) {
                    submatrix[i][j] = matrix[c[i]][c[j]];
                }
            }

            // also send the size of the submatrix
            // MPI_Send(&submatrixSize, 1, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD);
            // print submatrix
            for (int i = 0; i < submatrixSize; i++) {
                for (int j = 0; j < submatrixSize; j++) {
                    cout << submatrix[i][j] << " ";
                }
                cout << endl;
            }
            MPI_Send(&submatrix, submatrixSize * submatrixSize, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD);
            nextProcessId++;
            break;
        }
    }
    else {
        // receive the submatrix and print it
        int submatrixSize = 3;
        // MPI_Recv(&submatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cout << submatrixSize << endl;
        int **submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
        }
        // int submatrix[submatrixSize][submatrixSize];
        MPI_Recv(&submatrix, submatrixSize * submatrixSize, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int i = 0; i < submatrixSize; i++) {
            for (int j = 0; j < submatrixSize; j++) {
                cout << submatrix[i][j] << " ";
            }
            cout << endl;
        }
    }

    MPI_Finalize();
}