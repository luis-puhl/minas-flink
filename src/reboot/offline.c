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
    int kParam = 100, dim = 22, minExamplesPerCluster = 20;
    double precision = 1.0e-08, radiusF = 0.10, noveltyF = 2.0;
    //
    kParam=100; dim=22; precision=1.0e-08; radiusF=0.25; minExamplesPerCluster=20; noveltyF=1.4;
    // kParam=100; dim=22; precision=1.0e-08; radiusF=0.10; minExamplesPerCluster=20; noveltyF=2.0;
    //
    fprintf(stderr, "kParam=%d; dim=%d; precision=%le; radiusF=%le; minExamplesPerCluster=%d; noveltyF=%le\n", PARAMS);
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
    return EXIT_SUCCESS;
}
