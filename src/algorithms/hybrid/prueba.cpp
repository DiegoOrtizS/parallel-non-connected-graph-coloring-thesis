#include <iostream>
#include <mpi.h>
#include <vector>
#include <algorithm>

// Struct to hold the label and data information
struct LabelData {
    int label;
    int data;
};

// Custom comparison function for sorting based on labels
bool compareLabels(const LabelData& a, const LabelData& b) {
    return a.label < b.label;
}

int main(int argc, char** argv) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Datatype mpi_label_data_type;  // MPI data type for LabelData
    MPI_Type_contiguous(2, MPI_INT, &mpi_label_data_type);
    MPI_Type_commit(&mpi_label_data_type);

    std::vector<LabelData> my_label_data;  // Array of labels and data for the current process

    if (rank == 1) {
        my_label_data = {{3, 1}, {4, 1}, {5, 2}};
    } else if (rank == 2) {
        my_label_data = {{0, 1}, {1, 2}, {2, 3}};
    } else if (rank == 0) {
        my_label_data = {{6, 1}, {7, 2}, {8, 2}};
    }

    // Determine the size of data to be received from each process
    std::vector<int> recvcounts(size);
    int my_label_data_size = my_label_data.size();
    MPI_Gather(&my_label_data_size, 1, MPI_INT, recvcounts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate displacements for received data
    std::vector<int> displs(size, 0);
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + recvcounts[i - 1];
        }
    }

    // Gather the label_data array onto the master process
    std::vector<LabelData> gathered_label_data;

    int total_elements = displs[size - 1] + recvcounts[size - 1];
    gathered_label_data.resize(total_elements);

    // MPI_Gatherv(my_label_data.data(), my_label_data_size, mpi_label_data_type,
    //             gathered_label_data.data(), recvcounts.data(), displs.data(), mpi_label_data_type,
    //             0, MPI_COMM_WORLD);
    // this but with MPI_Gather
    MPI_Gather(my_label_data.data(), my_label_data_size, mpi_label_data_type,
                gathered_label_data.data(), my_label_data_size, mpi_label_data_type,
                0, MPI_COMM_WORLD);

    // Sort the gathered data by labels on the master process
    if (rank == 0) {
        std::sort(gathered_label_data.begin(), gathered_label_data.end(), compareLabels);

        std::cout << "Gathered labels: ";
        for (const auto& label_data : gathered_label_data) {
            std::cout << label_data.label << " ";
        }
        std::cout << std::endl;

        std::cout << "Gathered data: ";
        for (const auto& label_data : gathered_label_data) {
            std::cout << label_data.data << " ";
        }
        std::cout << std::endl;
    }

    MPI_Type_free(&mpi_label_data_type);
    MPI_Finalize();
    return 0;
}
