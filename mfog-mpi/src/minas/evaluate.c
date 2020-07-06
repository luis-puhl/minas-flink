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

// void checkLabel(char label, char *csvSource, int id, char *sourceCode, int sourceLine) {
//     if (label < 'A' || label > 'Z') {
//         errx(EXIT_FAILURE, "%s:%d Bad file format '%s'. Unexpected label '%c' on id %d.\n", sourceCode, sourceLine, csvSource, label, id);
//     }
// }

int main(int argc, char *argv[], char **envp) {
    char *executable = argv[0];
    char *matchesCsv, *examplesCsv, *evaluateLog, *timingLog;
    FILE *matchesFile, *examplesFile, *evaluate, *timing;
    #define VARS_SIZE 4
    char *varNames[] = {"MATCHES_CSV", "EXAMPLES_CSV", "EVALUATE_LOG", "TIMING_LOG"};
    char **fileNames[] = { &matchesCsv, &examplesCsv, &evaluateLog, &timingLog };
    FILE **files[] = {&matchesFile, &examplesFile, &evaluate, &timing};
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
    int totalExamples;
    Point *examples = readExamples(dimension, examplesFile, &totalExamples, timing, argv[0]);
    /**
     * #id,isMach,clusterId,label,distance,radius
     * 1,y,18,N,6.003312e-01,1.826296e+00
     * 2,y,19,N,4.860992e-01,1.026340e+00
     * 3,y,18,N,6.003312e-01,1.826296e+00
     * 4,y,18,N,6.003312e-01,1.826296e+00
    **/
    // Match *matches = malloc(nExamples * sizeof(Match));
    Match match;
    char header[1024];
    if (!fscanf(matchesFile, "%s", header)) {
        errx(EXIT_FAILURE, "%s:%d Bad file format '%s'", __FILE__, __LINE__, matchesCsv);
    } else {
        printf("header = %s\n", header);
    }
    int totalMatches = 0;
    //
    clock_t start = clock();
    int confusionSize = 1;
    char *labels = malloc(confusionSize * sizeof(char));
    labels[0] = '-';
    int **confusionMatrix = malloc(confusionSize * sizeof(int*));
    confusionMatrix[0] = malloc(confusionSize * sizeof(int));
    Point example;
    // typedef struct s_classified{
    //     char class;
    //     int examples;
    //     int *as;
    // } Classified;
    // Classified *classes = malloc(confusionSize * sizeof(Classified));
    // int labelsSize = 1;
    // classes[0].class = '\0';
    // classes[0].examples = 0;
    // classes[0].as = malloc(labelsSize * sizeof(int));
    int lblSize = 1;
    char *lbl = malloc(lblSize * sizeof(char));
    int classesSize = 1;
    char *classes = malloc(classesSize * sizeof(char));
    int matrixSize = 1;
    int **matrix = malloc(matrixSize * sizeof(int *));
    matrix[0] = malloc(matrixSize * sizeof(int));
    matrix[0][0] = 0;
    int totalHits = 0;
    while (!feof(matchesFile)) {
        fscanf(matchesFile, MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_SCAN_ARGS(match));
        example = examples[match.pointId - 1];
        totalMatches++;
        // printf(
        //     "%s:%d ready %d,%c -> \t "MATCH_CSV_LINE_FORMAT,
        //     __FILE__, __LINE__, example.id, example.label, MATCH_CSV_LINE_PRINT_ARGS(match)
        // );
        //
        if (match.pointId != example.id) {
            printf("mismatch example and match " MATCH_CSV_LINE_FORMAT, MATCH_CSV_LINE_PRINT_ARGS(match));
        }
        int i = 0;
        for (; i < lblSize; i++) {
            if (lbl[i] == '\0') {
                lbl[i] = match.label;
                //
                lblSize++;
                lbl = realloc(lbl, lblSize * sizeof(char));
                lbl[lblSize - 1] = '\0';
                //
                for (int k = 0; k < matrixSize; k++) {
                    matrix[k] = realloc(matrix[k], matrixSize * sizeof(int));
                    matrix[k][matrixSize - 1] = 0;
                }
                matrixSize++;
                matrix = realloc(matrix, matrixSize * sizeof(int *));
                matrix[matrixSize - 1] = malloc(matrixSize * sizeof(int));
            }
            if (lbl[i] == match.label) break;
        }
        int j = 0;
        for (; j < classesSize; j++) {
            if (classes[j] == '\0') {
                classes[j] = example.label;
                classesSize++;
                classes = realloc(classes, classesSize * sizeof(char));
                //
                for (int k = 0; k < matrixSize; k++) {
                    matrix[k] = realloc(matrix[k], matrixSize * sizeof(int));
                    matrix[k][matrixSize - 1] = 0;
                }
                matrixSize++;
                matrix = realloc(matrix, matrixSize * sizeof(int *));
                matrix[matrixSize - 1] = malloc(matrixSize * sizeof(int));
                for (int k = 0; k < matrixSize; k++) {
                    matrix[matrixSize - 1][k] = 0;
                }
            }
            if (classes[j] == example.label) break;
        }
        // matrix[i][j]++;
        //     if (classes[i].class == '\0') {
        //         // first case
        //         printf("%s:%d first class '%c'\n", __FILE__, __LINE__, example.label);
        //         classes[i].class = example.label;
        //         // one over existing classes, realloc
        //         printf("%s:%d realloc new class '%c'\n", __FILE__, __LINE__, example.label);
        //         confusionSize++;
        //         classes = realloc(classes, confusionSize * sizeof(Classified));
        //         classes[confusionSize - 1].class = '\0';
        //         classes[confusionSize - 1].examples = 0;
        //         classes[confusionSize - 1].as = malloc(labelsSize * sizeof(int));
        //     }
        //     if (classes[i].class == example.label) {
        //         classes[i].examples++;
        //         for (int j = 0; j < labelsSize; j++) {
        //             if (labels[j] == '\0') {
        //                 // first case
        //                 printf("%s:%d first label '%c'\n", __FILE__, __LINE__, match.label);
        //                 labels[j] = match.label;
        //                 // realloc
        //                 printf("%s:%d realloc new label '%c'\n", __FILE__, __LINE__, match.label);
        //                 labelsSize++;
        //                 for (int k = 0; k < confusionSize; k++) {
        //                     classes[k].as = realloc(classes[k].as, labelsSize * sizeof(int));
        //                     classes[k].as[labelsSize - 1] = 0;
        //                 }
        //                 labels = realloc(labels, labelsSize * sizeof(char));
        //                 labels[labelsSize - 1] = match.label;
        //             }
        //             if (labels[j] == match.label) {
        //                 classes[i].as[j]++;
        //                 break;
        //             }
        //         }
        //         break;
        //     }
        // }
        //
        int exampleIndex = findLabelIndex(&confusionSize, &labels, &confusionMatrix, example.label);
        int matchIndex = findLabelIndex(&confusionSize, &labels, &confusionMatrix, match.label);
        //
        confusionMatrix[matchIndex][exampleIndex]++;
        if (example.label == match.label) totalHits++;
    }
    printf("done\n");
    if (totalMatches != totalExamples) {
        fprintf(evaluate, "%s:%d Not enough mathces, expected %d but got %d.\n", __FILE__, __LINE__, totalExamples, totalMatches);
    }
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
    fprintf(evaluate, "\n\nanoter matrix\n");
    fprintf(evaluate, "lbl %s\n", lbl);
    fprintf(evaluate, "classes %s\n", classes);
    for (int i = 0; i < classesSize; i++) {
        for (int j = 0; j < lblSize; j++) {
            // fprintf(evaluate, "%20d", matrix[i][j]);
            fprintf(evaluate, "%20d", 0);
        }
        fprintf(evaluate, "\n");
    }
    fprintf(evaluate, "\n\n");
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
