#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>

#include "./base.h"

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    unsigned int kParam = 100, dim = 22, minExamplesPerCluster = 20, thresholdForgettingPast = 10000;
    double precision = 1.0e-08, radiusF = 0.25, noveltyF = 1.4;
    fprintf(stderr, "%s; kParam=%u; dim=%u; precision=%le; radiusF=%le; minExamplesPerCluster=%u; noveltyF=%le, thresholdForgettingPast=%u\n", argv[0], PARAMS);
    //
    Model *model = training(kParam, dim, precision, radiusF);
    //
    for (size_t k = 0; k < model->size; k++) {
        Cluster *cl = &model->clusters[k];
        fprintf(stdout, "Cluster: %10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
        for (unsigned int d = 0; d < dim; d++)
            fprintf(stdout, ", %le", cl->center[d]);
        fprintf(stdout, "\n");
    }
    free(model);
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
