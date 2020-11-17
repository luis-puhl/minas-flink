#include <stdio.h>
#include <time.h>
#include <mpi.h>

int main(int argc, char** argv) {
    clock_t start = clock();
    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Get the name of the processor
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    // Print off a hello world message
    printf("Hello world from processor %.10s, rank %.3d out of %.3d processors\n",
           processor_name, world_rank, world_size);

    // loop the ring
    int i;
    MPI_Status st;
    if (world_rank == 0) {
        MPI_Send(&world_rank, 1, MPI_INT, world_rank + 1, 200, MPI_COMM_WORLD);
        MPI_Recv(&i, 1, MPI_INT, world_size - 1, 200, MPI_COMM_WORLD, &st);
        printf("ring got %d\n", i);
    } else {
        MPI_Recv(&i, 1, MPI_INT, world_rank - 1, 200, MPI_COMM_WORLD, &st);
        i++;
        MPI_Send(&world_rank, 1, MPI_INT, (world_rank + 1) % world_size, 200, MPI_COMM_WORLD);
    }

    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    // Finalize the MPI environment.
    return MPI_Finalize();
}
