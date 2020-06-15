#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "./minas.h"

// #define SQR_DISTANCE 1

int MNS_classifier() {
    #ifdef SQR_DISTANCE
        fprintf(stderr, "Using Square distance (d²)\n");
    #endif // SQR_DISTANCE
    int dimension = 22;
    //
    Model model;
    char *modelName = "datasets/model-clean.csv";
    readModel(dimension, modelName, &model);

    char *testName = "datasets/test.csv";
    Point *examples;
    examples = readExamples(dimension, testName);
    
    printf("#id,isMach,clusterId,label,distance,radius\n");
    int exampleCounter = 0;
    clock_t start = clock();
    Match match;
    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        classify(dimension, &model, &(examples[exampleCounter]), &match);
    }
    fprintf(stderr, "Done %s in \t%fs\n", __FUNCTION__, ((double)(clock() - start)) / ((double)1000000));

    for (int i = 0; i < model.size; i++) {
        free(model.vals[i].center);
    }
    free(model.vals);
    free(examples);
    return 0;
}

#ifndef MAIN
#define MAIN
int main(int argc, char **argv) {
    clock_t start = clock();
    MNS_classifier();
    fprintf(stderr, "Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    return 0;
}
#endif
