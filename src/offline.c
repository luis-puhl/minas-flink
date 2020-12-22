#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "./base.h"

int main(int argc, char const *argv[]) {
    clock_t start = clock();
    MinasParams minasParams = {
        .k=100, .dim=22, .precision=1.0e-08,
        .radiusF=0.25, .minExamplesPerCluster=20, .noveltyF=1.4,
        .thresholdForgettingPast = 10000,
    };
    minasParams.noveltyDetectionTrigger = minasParams.minExamplesPerCluster * minasParams.k;
    minasParams.unknownsMaxSize = minasParams.noveltyDetectionTrigger * 2;
    MinasState minasState = MINAS_STATE_EMPTY;
    minasState.unknowns = calloc(minasParams.unknownsMaxSize + 1, sizeof(Example));
    for (unsigned long int i = 0; i < minasParams.unknownsMaxSize + 1; i++) {
        minasState.unknowns[i].val = calloc(minasParams.dim, sizeof(double));
    }
    printArgs(minasParams, 2, 0);
    //
    Model *model = training(&minasParams);
    //
    for (ModelLink *curr = model->head; curr != NULL; ) {
        Cluster *cl = &curr->cluster;
        fprintf(stdout, "Cluster: %10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
        for (unsigned int d = 0; d < minasParams.dim; d++)
            fprintf(stdout, ", %le", cl->center[d]);
        fprintf(stdout, "\n");
        curr = curr->next;
    }
    free(model);
    fprintf(stderr, "[%s] %le seconds. At %s:%d\n", argv[0], ((double)clock() - start) / 1000000.0, __FILE__, __LINE__);
    return EXIT_SUCCESS;
}
