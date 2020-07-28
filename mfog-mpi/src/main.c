#include <stdlib.h>
#include <mpi.h>
#include <err.h>
#include <time.h>

#include "./minas/minas.h"
#include "./mpi/minas-mpi.h"
#include "./util/loadenv.h"

/**
 * Experiments are based in this Minas config
 *      threshold = 2.0
 *      flagEvaluationType = 1
 *      thresholdForgettingPast = 10000
 *      numMicro = 100   // aka K in k-means
 *      flagMicroClusters = true
 *      
 *      minExCluster = 20
 *      validationCriterion = dec
*/

#ifndef MAIN
#define MAIN

int mainClassify(int mpiRank, int mpiSize, Point examples[], Model *model, int *nMatches, Match *memMatches, FILE *matches, FILE *timing, char *executable) {
    clock_t start = clock();
    double noveltyThreshold = 2;
    int k = 100;
    int minExCluster = 20;
    int maxUnkSize = k * minExCluster;
    int exampleCounter = 0;
    *nMatches = 0;
    int thresholdForgettingPast = 10000;
    int lastCheck = 0;
    if (mpiSize == 1) {
        Point **unknowns = malloc(maxUnkSize * sizeof(Point *));
        int unknownsSize = 0;
        for (exampleCounter = 0; examples[exampleCounter].value != NULL; exampleCounter++) {
            Point *example = &(examples[exampleCounter]);
            Match *match = &(memMatches[*nMatches]);
            (*nMatches)++;
            classify(model->dimension, model, example, match);
            if (match->label == '-') {
                unknowns[unknownsSize] = example;
                unknownsSize++;
                if (unknownsSize >= maxUnkSize && (lastCheck + (k * minExCluster) < exampleCounter)) {
                    lastCheck = exampleCounter;
                    // ND
                    Point *linearGroup = malloc(unknownsSize * sizeof(Point));
                    printf("clustering unknowns with %5d examples\n", unknownsSize);
                    for (int g = 0; g < unknownsSize; g++) {
                        linearGroup[g] = *unknowns[g];
                    }
                    model = noveltyDetection(k, model, unknownsSize, linearGroup, minExCluster, noveltyThreshold, timing, executable);
                    char outputModelFileName[200];
                    sprintf(outputModelFileName, "out/models/%d.csv", exampleCounter);
                    FILE *outputModelFile = fopen(outputModelFileName, "w");
                    if (outputModelFile != NULL) {
                        writeModel(model->dimension, outputModelFile, model, timing, executable);
                    }
                    fclose(outputModelFile);
                    //
                    // Classify after model update
                    int prevUnknownsSize = unknownsSize;
                    unknownsSize = 0;
                    int currentForgetUnkThreshold = exampleCounter - thresholdForgettingPast;
                    int forgotten = 0;
                    for (int unk = 0; unk < prevUnknownsSize; unk++) {
                        match = &(memMatches[*nMatches]);
                        classify(model->dimension, model, unknowns[unk], match);
                        if (match->label != '-') {
                            (*nMatches)++;
                            // printf("late classify %d %c\n", unkMatch.pointId, unkMatch.label);
                        } else if (unknowns[unk]->id > currentForgetUnkThreshold) {
                            // compact unknowns
                            unknowns[unknownsSize] = unknowns[unk];
                            unknownsSize++;
                        } else {
                            forgotten++;
                        }
                    }
                    printf("late classify of %d -> %d unknowns, forgotten %d\n", prevUnknownsSize, unknownsSize, forgotten);
                    fflush(stdout);
                    free(linearGroup);
                }
            }
            if (exampleCounter % thresholdForgettingPast == 0) {
                // put old clusters in model to sleep
            }
        }
        if (timing) {
            PRINT_TIMING(timing, executable, mpiSize, start, exampleCounter);
        }
        fprintf(matches, MATCH_CSV_HEADER);
        for (int i = 0; i < (*nMatches); i++) {
            fprintf(matches, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
    } else if (mpiRank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);
        sendModel(model->dimension, model, mpiRank, mpiSize, timing, executable);
        
        MPI_Barrier(MPI_COMM_WORLD);
        int exampleCounter = sendExamples(model->dimension, examples, memMatches, mpiSize, timing, executable);

        MPI_Barrier(MPI_COMM_WORLD);
        if (timing) {
            PRINT_TIMING(timing, executable, mpiSize, start, exampleCounter);
        }
        fprintf(matches, MATCH_CSV_HEADER);
        for (int i = 0; i < exampleCounter; i++) {
            fprintf(matches, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(memMatches[i]));
        }
        // closeEnv(envSize, varNames, fileNames, files, fileModes);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        model = malloc(sizeof(Model));
        receiveModel(0, model, mpiRank);
        MPI_Barrier(MPI_COMM_WORLD);

        receiveExamples(model->dimension, model, mpiRank);

        MPI_Barrier(MPI_COMM_WORLD);
    }
    return exampleCounter;
}

int main(int argc, char *argv[], char **envp) {
    int mpiReturn;
    mpiReturn = MPI_Init(&argc, &argv);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    int mpiRank, mpiSize;
    mpiReturn = MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
    mpiReturn = MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    if (mpiReturn != MPI_SUCCESS) {
        MPI_Abort(MPI_COMM_WORLD, mpiReturn);
        errx(EXIT_FAILURE, "MPI Abort %d\n", mpiReturn);
    }
    printf("MPI rank / size => %d/%d\n", mpiRank, mpiSize);
    // printEnvs(argc, argv, envp);
    char *executable = argv[0];
    int envSize =           1 +             1 +             /* 1 +                 */ 1 +             1 +             1;
    char                    *trainingCsv,   *modelCsv,      /* *modelOutCsv,       */ *examplesCsv,   *matchesCsv,    *timingLog;
    FILE                    *trainingFile,  *modelFile,     /* *modelOutFile,      */ *examplesFile,  *matches,       *timing;
    char *varNames[] = {    "TRAINING_CSV", "MODEL_CSV",    /* "MODEL_OUT_CSV",    */ "EXAMPLES_CSV", "MATCHES_CSV",  "TIMING_LOG" };
    char **fileNames[] = {  &trainingCsv,   &modelCsv,      /* &modelOutCsv,       */ &examplesCsv,   &matchesCsv,    &timingLog };
    FILE **files[] = {      &trainingFile,  &modelFile,     /* &modelOutFile,      */ &examplesFile,  &matches,       &timing };
    char *fileModes[] = {   "r",            "r",            /* "w",                */ "r",            "w",            "a" };
    if (mpiRank == 0) {
        loadEnv(argc, argv, envp, envSize, varNames, fileNames, files, fileModes);
        printf(
            "Reading training from  '%s'\n"
            "Reading model from     '%s'\n"
            // "Writing model out to   '%s'\n"
            "Reading examples from  '%s'\n"
            "Writing matches to     '%s'\n"
            "Writing timing to      '%s'\n",
            trainingCsv, modelCsv, /* modelOutCsv, */ examplesCsv, matchesCsv, timingLog
        );
        fflush(stdout);
    }
    //
    Model *model;
    if (mpiRank == 0) {
        if (trainingCsv != NULL && trainingFile != NULL) {
            // printf("will training\n");
            int nExamples;
            Point *examples = readExamples(22, trainingFile, &nExamples, timing, executable);
            model = MNS_offline(nExamples, examples, 100, 22, timing, executable);
            fflush(stdout);
            FILE *outputModelFile = fopen("out/models/0-initial.csv", "w");
            if (outputModelFile != NULL) {
                writeModel(model->dimension, outputModelFile, model, timing, executable);
            }
            fclose(outputModelFile);
        } else if (modelCsv != NULL && modelFile != NULL) {
            model = readModel(22, modelFile, timing, executable);
        }
    }
    Point *examples;
    Match *memMatches;
    int nExamples;
    if (examplesCsv != NULL && examplesFile != NULL) {
        examples = readExamples(model->dimension, examplesFile, &nExamples, timing, executable);
        // max 2 matches per example
        memMatches = malloc(2 * nExamples * sizeof(Match));
    }
    int nMatches;
    mainClassify(mpiRank, mpiSize, examples, model, &nMatches, memMatches, matches, timing, executable);

    if (mpiRank == 0) {
        char outputModelFileName[200];
        sprintf(outputModelFileName, "out/models/%d-final.csv", nExamples);
        FILE *outputModelFile = fopen(outputModelFileName, "w");
        if (outputModelFile != NULL) {
            writeModel(model->dimension, outputModelFile, model, timing, executable);
        }
        fclose(outputModelFile);
        closeEnv(envSize, varNames, fileNames, files, fileModes);
    }
    
    MPI_Finalize();
    free(model);
    free(examples);
    free(memMatches);
    return 0;
}

#endif // MAIN
