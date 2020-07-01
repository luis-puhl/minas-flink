#include "./minas/minas.h"
#include "./mpi/minas-mpi.h"
#include "./util/loadenv.h"

#ifndef MAIN
#define MAIN
int main(int argc, char *argv[], char **envp) {
    // printEnvs(argc, argv, envp);
    MNS_mfog_main(argc, argv, envp);
    return 0;
}
#endif // MAIN
