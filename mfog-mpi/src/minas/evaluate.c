#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>

#include "minas.h"
#include "../util/loadenv.h"

int findLabelIndex(int *confusionSize, char **labels, int ***confusionMatrix, char newLabel) {
    int i;
    for (i = 0; i < (*confusionSize); i++) {
        if ((*labels)[i] == newLabel) {
            break;
        }
    }
    if (i == (*confusionSize)) {
        // printf("new label '%c', realloc\n", newLabel);
        for (i = 0; i < (*confusionSize); i++) {
            (*confusionMatrix)[i] = realloc((*confusionMatrix)[i], ((*confusionSize) +1) * sizeof(int));
        }
        (*confusionSize)++;
        (*labels) = realloc((*labels), (*confusionSize) * sizeof(char));
        (*labels)[i] = newLabel;
        (*confusionMatrix)[((*confusionSize) - 1)] = malloc((*confusionSize) * sizeof(int));
    }
    return i;
}

int main(int argc, char *argv[], char **envp) {
    char *executable = argv[0];
    char *matchesCsv, *examplesCsv, *evaluateLog, *timingLog;
    FILE *matches, *examples, *evaluate, *timing;
    #define VARS_SIZE 4
    char *varNames[] = {"MATCHES_CSV", "EXAMPLES_CSV", "EVALUATE_LOG", "TIMING_LOG"};
    char **fileNames[] = { &matchesCsv, &examplesCsv, &evaluateLog, &timingLog };
    FILE **files[] = { &matches, &examples, &evaluate, &timing };
    char *fileModes[] = { "r", "r", "a", "a" };
    loadEnv(argc, argv, envp, VARS_SIZE, varNames, fileNames, files, fileModes);
    printf(
        "Reading examples from   '%s'\n"
        "Reading matches from    '%s'\n"
        "Writing evaluation to   '%s'\n"
        "Writing timing to       '%s'\n",
        examplesCsv, matchesCsv, evaluateLog, timingLog
    );
    //
    int dimension = 22;
    clock_t start = clock();
    int confusionSize = 1;
    char *labels = malloc(confusionSize * sizeof(char));
    labels[0] = '-';
    int **confusionMatrix = malloc(confusionSize * sizeof(int*));
    confusionMatrix[0] = malloc(confusionSize * sizeof(int));
    //
    Point example;
    example.value = malloc(dimension * sizeof(double));
    example.id = 0;
    Match match;
    //
    char header[1024];
    if (!fscanf(matches, "%s\n", header)) {
        errx(EXIT_FAILURE, "bad file format '%s'", matchesCsv);
    }
    int matchesBufferSize = 0, examplesBufferSize = 0;
    int totalExamples = 0, totalMatches = 0, totalHits = 0;
    Match *matchesBuffer = malloc(1 * sizeof(Match));
    Point *examplesBuffer = malloc(1 * sizeof(Point));
    while (!feof(examples)) {
        for (int i = 0; i < dimension; i++) {
            fscanf(examples, "%lf,", &(example.value[i]));
        }
        fscanf(examples, "%c\n", &example.label);
        totalExamples++;
        example.id++;
        //
        if (!feof(matches)) {
            fscanf(matches, "%d,%c,%d,%c,%lf,%lf\n",
                &(match.pointId), &(match.isMatch), &(match.clusterId),
                &(match.label), &(match.distance), &(match.radius)
            );
            totalMatches++;
        } else {
            match.pointId = -1;
            match.isMatch = 'n';
        }
        if (match.pointId != example.id) {
            int i = 0;
            // after buffer is too big, reuse addresses
            if (matchesBufferSize > 100) {
                // try to replace an deleted buffer entry
                for (i = 0; i < matchesBufferSize; i++) {
                    if (matchesBuffer[i].pointId == -1) {
                        matchesBuffer[i] = match;
                        break;
                    }
                }
            }
            // if no deleted entry has been found, realloc
            if (i >= matchesBufferSize) {
                matchesBuffer = realloc(matchesBuffer, ++matchesBufferSize * sizeof(Match));
                matchesBuffer[matchesBufferSize -1] = match;
            }
            if (matchesBufferSize > 100) {
                // try to replace an deleted buffer entry
                for (i = 0; i < examplesBufferSize; i++) {
                    if (examplesBuffer[i].id == -1) {
                        examplesBuffer[i] = example;
                        break;
                    }
                }
            }
            // if no deleted entry has been found, realloc
            if (i >= matchesBufferSize) {
                examplesBuffer = realloc(examplesBuffer, ++examplesBufferSize * sizeof(Point));
                examplesBuffer[examplesBufferSize - 1] = example;
            }
            for (int i = 0; i < matchesBufferSize; i++) {
                for (int j = 0; j < examplesBufferSize; j++) {
                    if (matchesBuffer[i].pointId == examplesBuffer[j].id) {
                        match = matchesBuffer[i];
                        example = examplesBuffer[j];
                        // delete
                        matchesBuffer[i].pointId = -1;
                        examplesBuffer[j].id = -1;
                        // break outer
                        i = matchesBufferSize;
                        break;
                    }
                }
            }
        }
        int exampleIndex = findLabelIndex(&confusionSize, &labels, &confusionMatrix, example.label);
        char matchLabel = match.isMatch == 'y' ? match.label : '-';
        int matchIndex = findLabelIndex(&confusionSize, &labels, &confusionMatrix, matchLabel);
        //
        confusionMatrix[matchIndex][exampleIndex]++;
        if (match.isMatch == 'y' && example.label == match.label) totalHits++;
    }
    //

    // printMatrix(&confusionSize, &labels, &confusionMatrix);
    fprintf(evaluate,
        "Confusion Matrix\n"
        "      \t|\tClasses\n"
        "Labels\t|\t");
    for (int i = 1; i < (confusionSize); i++) {
        fprintf(evaluate, "%10c\t", labels[i]);
    }
    fprintf(evaluate, "\n");
    for (int i = 0; i < (confusionSize); i++) {
        fprintf(evaluate, "%6c\t|\t", labels[i]);
        for (int j = 1; j < confusionSize; j++) {
            fprintf(evaluate, "%10d\t", confusionMatrix[i][j]);
        }
        fprintf(evaluate, "\n");
    }
    //
    double hitPC = ((double)totalHits) / ((double)totalExamples);
    fprintf(evaluate,
        "Total examples   %8d\n"
        "Total matches    %8d\n"
        "Hits             %8d (%10f%%)\n"
        "Misses           %8d (%10f%%)\n",
        totalExamples, totalMatches,
        totalHits, hitPC * 100.0,
        totalExamples - totalHits, (1 - hitPC) * 100.0
    );
    //
    for (int i = 0; i < confusionSize; i++) {
        free(confusionMatrix[i]);
    }
    free(confusionMatrix);
    free(labels);
    PRINT_TIMING(timing, executable, 1, start, totalExamples);
    closeEnv(VARS_SIZE, varNames, fileNames, files, fileModes);
    exit(EXIT_SUCCESS);
}
