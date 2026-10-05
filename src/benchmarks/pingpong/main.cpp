#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

// Ping-pong between ranks 0 and 1 to fit the alpha-beta model t(w) = alpha + beta * w (thesis Eq. hockney).
// Prints one line per message size: PINGPONG,words,bytes,one_way_seconds (median of the repetitions).
// Usage: mpirun -np 2 ./a.out [max_log2_words=22] [repetitions=50]
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size < 2) {
        if (rank == 0) {
            std::fprintf(stderr, "ping-pong needs at least 2 processes\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int maxLog = argc > 1 ? std::atoi(argv[1]) : 22;
    int repetitions = argc > 2 ? std::atoi(argv[2]) : 50;

    std::vector<long long> buffer(1LL << maxLog, 1);
    for (int j = 0; j <= maxLog; j++) {
        int words = 1 << j;
        std::vector<double> times;
        // One warm-up exchange, then the timed repetitions.
        for (int r = -1; r < repetitions; r++) {
            MPI_Barrier(MPI_COMM_WORLD);
            double start = MPI_Wtime();
            if (rank == 0) {
                MPI_Send(buffer.data(), words, MPI_LONG_LONG_INT, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buffer.data(), words, MPI_LONG_LONG_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else if (rank == 1) {
                MPI_Recv(buffer.data(), words, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buffer.data(), words, MPI_LONG_LONG_INT, 0, 0, MPI_COMM_WORLD);
            }
            double elapsed = MPI_Wtime() - start;
            if (r >= 0) {
                times.push_back(elapsed / 2);
            }
        }
        if (rank == 0) {
            std::sort(times.begin(), times.end());
            std::printf("PINGPONG,%d,%lld,%.9e\n", words, 8LL * words, times[times.size() / 2]);
        }
    }
    MPI_Finalize();
    return 0;
}
