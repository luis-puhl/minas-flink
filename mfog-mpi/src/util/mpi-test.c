#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "mpi.h"

#define LEN 128
#define root 0

int main(int argc, char *argv[]) {
    printf("Hello World, from %s with <3\n", argv[0]);
    //
    //
    int i, rank, result, numtasks, namelen, msgtag, pid, pid_0;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    char buf[LEN];

    result = MPI_Init(&argc, &argv);
    if (result != MPI_SUCCESS) {
        printf("Erro iniciando programa MPI.\n");
        MPI_Abort(MPI_COMM_WORLD, result);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);

    if (rank == root) {
        char processor_name[MPI_MAX_PROCESSOR_NAME];
        int name_len, mpiReturn;
        mpiReturn = MPI_Get_processor_name(processor_name, &name_len);
        if (mpiReturn != MPI_SUCCESS) {
            MPI_Abort(MPI_COMM_WORLD, mpiReturn);
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
        // printEnvs(argc, argv, envp);
        fprintf(stderr, "Processor %s, Rank %d out of %d processors\n", processor_name, rank, numtasks);
        //
        #ifdef MPI_VERSION
        fprintf(stderr, "MPI %d %d\n", MPI_VERSION, MPI_SUBVERSION);
        #endif // MPI_VERSION
        //
        char value[140]; int flag;
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "arch", 140, value, &flag);
        if (mpiReturn != MPI_SUCCESS) {
            MPI_Abort(MPI_COMM_WORLD, mpiReturn);
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
        if (flag) fprintf(stderr, "MPI arch = %s\n", value);
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "host", 140, value, &flag);
        if (mpiReturn != MPI_SUCCESS) {
            MPI_Abort(MPI_COMM_WORLD, mpiReturn);
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
        if (flag) fprintf(stderr, "MPI host = %s\n", value);
        //
        mpiReturn = MPI_Info_get(MPI_INFO_ENV, "thread_level", 140, value, &flag);
        if (mpiReturn != MPI_SUCCESS) {
            MPI_Abort(MPI_COMM_WORLD, mpiReturn);
            errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
        }
        if (flag) fprintf(stderr, "MPI thread_level = %s\n", value);
    }
        sprintf(buf, "%s", processor_name);

    // mesmo codigo é executado em todos os nós. Rank e root do bcast definem papeis
    // como todos usam o mesmo códo e só root sabe o tamanho da string, não dá para usar strlen...
    // int MPI_Bcast (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
    // MPI_Bcast( buf, strlen(buf+1), MPI_CHAR, root, MPI_COMM_WORLD);
    MPI_Bcast(buf, LEN, MPI_CHAR, root, MPI_COMM_WORLD);

    if (rank == root)
    {
        printf("%s enviou: %s\n", processor_name, buf);
    }
    else
    {
        // todos recebem de rank 0
        printf("%s (%d) recebeu: %s\n", processor_name, rank, buf);
    }

    MPI_Finalize();

    return (0);
}
