#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "./minas.h"
#include "./loadenv.h"

// #define SQR_DISTANCE 1

int MNS_classifier(int argc, char *argv[], char **envp) {
    char *executable = argv[0];
    char *modelCsv, *examplesCsv, *matchesCsv, *timingLog;
    FILE *modelFile, *examplesFile, *matches, *timing;
    #define VARS_SIZE 4
    char *varNames[] = { "MODEL_CSV", "EXAMPLES_CSV", "MATCHES_CSV", "TIMING_LOG"};
    char **fileNames[] = { &modelCsv, &examplesCsv, &matchesCsv, &timingLog };
    FILE **files[] = { &modelFile, &examplesFile, &matches, &timing };
    char *fileModes[] = { "r", "r", "w", "a" };
    loadEnv(argc, argv, envp, VARS_SIZE, varNames, fileNames, files, fileModes);
    printf(
        "Reading examples from  '%s'\n"
        "Reading model from     '%s'\n"
        "Writing matches to     '%s'\n"
        "Writing timing to      '%s'\n",
        examplesCsv, modelCsv, matchesCsv, timingLog
    );
    //
    
    #ifdef SQR_DISTANCE
        printf(stderr, "Using Square distance (d²)\n");
    #endif // SQR_DISTANCE
    int dimension = 22;
    //
    Model model;
    readModel(dimension, modelFile, &model, timing, executable);
    Point *examples;
    int nExamples;
    examples = readExamples(dimension, examplesFile, &nExamples, timing, executable);

    fprintf(matches, "#id,isMach,clusterId,label,distance,radius\n");
    int exampleCounter = 0;
    clock_t start = clock();
    Match match;
    for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
        classify(dimension, &model, &(examples[exampleCounter]), &match);
        fprintf(matches, "%d,%c,%d,%c,%e,%e\n",
            match.pointId, match.isMatch, match.clusterId,
            match.label, match.distance, match.radius
        );
    }
    PRINT_TIMING(timing, executable, 1, start);
    closeEnv(VARS_SIZE, varNames, fileNames, files, fileModes);

    for (int i = 0; i < model.size; i++) {
        free(model.vals[i].center);
    }
    free(model.vals);
    free(examples);
    return 0;
}

#ifndef MAIN
#define MAIN
int main(int argc, char *argv[], char **envp) {
    return MNS_classifier(argc, argv, envp);
}
#endif // MAIN
