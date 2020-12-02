#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <time.h>

#define MAIN

#include "../base/base.h"
#include "../base/minas.h"
#include "../base/kmeans.h"
#include "../base/clustream.h"

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);

    // writes to file "out/baseline-trainign.csv"
    Model *model = training(params);

    size_t modelFileSize = 0;
    modelFileSize += printf("# Model(dimension=%d, nextLabel=%s, size=%d)\n",
                            params->dim, printableLabel(model->nextLabel), model->size);
    modelFileSize += printf("#id, label, n_matches, distanceAvg, distanceStdDev, radius");
    //, ls_valLinearSum, ss_valSquareSum, distanceLinearSum, distanceSquareSum\n");
    // ,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
    for (unsigned int d = 0; d < params->dim; d++)
        modelFileSize += printf(", c%u", d);
    modelFileSize += printf("\n");
    //
    for (int i = 0; i < model->size; i++) {
        Cluster *cl = &(model->clusters[i]);
        modelFileSize += printf("%10u, %s, %10u, %le, %le, %le",
                cl->id, printableLabel(cl->label), cl->n_matches,
                cl->distanceAvg, cl->distanceStdDev, cl->radius);
        for (unsigned int d = 0; d < params->dim; d++)
            modelFileSize += printf(", %le", cl->center[d]);
        modelFileSize += printf("\n");
    }

    fprintf(stderr, "Model size = %10lu\n", modelFileSize);
    //
    printTiming(main, model->size);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
