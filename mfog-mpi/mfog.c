#include <stdio.h>
#include "mpi.h"

int main(int argc, char** argv) {
    int rank, size;
    int tag, count;
    MPI_Status status;
    MPI_Request request = MPI_REQUEST_NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size); //number of processes
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); //rank of current process

    tag = 1234;
    count = 1; //number of elements in buffer
    int buffer; //value to send
    if (rank == 0) {
        // non blocking send to destination process
        printf("Enter a value to send to ALL:\n");
        scanf("%d", &buffer);
        for (int destination = 1; destination < size; destination++)
        {
            MPI_Isend(&buffer, count, MPI_INT, destination, tag, MPI_COMM_WORLD, &request);
        }
    } else {
        // destination process receives
        MPI_Irecv(&buffer, count, MPI_INT, 0, tag, MPI_COMM_WORLD, &request);
    }

    // blocks and waits for destination process to receive data
    MPI_Wait(&request, &status); 

    if (rank == 0) {
        printf("processor %d sent %d\n", rank, buffer);
    } else {
        printf("processor %d got %d\n", rank, buffer);
    }

    MPI_Finalize();

    return 0;
}
