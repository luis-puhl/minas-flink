#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "minas.h"

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

int printMatrix(int *confusionSize, char **labels, int ***confusionMatrix) {
    printf(
        "      \t|\tClasses\n"
        "Labels\t|\t");
    int i, j;
    for (i = 1; i < (*confusionSize); i++) {
        printf("%10c\t", (*labels)[i]);
    }
    printf("\n");
    for (i = 0; i < (*confusionSize); i++) {
        printf("%6c\t|\t", (*labels)[i]);
        for (j = 1; j < (*confusionSize); j++) {
            printf("%10d\t", (*confusionMatrix)[i][j]);
        }
        printf("\n");
    }
    return i;
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        errx(EXIT_FAILURE, "Missing arguments, expected 2, got %d\n", argc - 1);
    }
    printf("\n"
        "Reading test from      '%s'\n"
        "Reading output from    '%s'\n",
        argv[1], argv[2]
    );
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
    FILE *test = fopen(argv[1], "r");
    if (test == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[1]);
    // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
    FILE *class = fopen(argv[2], "r");
    if (class == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[2]);
    // id,isMach,clusterId,label,distance,radius
    char header[1024];
    if (!fscanf(class, "%s\n", header)) {
        errx(EXIT_FAILURE, "bad file format '%s'", argv[2]);
    }// else { printf("%s\n", header); }
    char l;
    int i, j, hits = 0, mathcesBufferSize = 0, examplesBufferSize = 0;
    Match *mathcesBuffer = malloc(1 * sizeof(Match));
    Point *examplesBuffer = malloc(1 * sizeof(Point));
    while (!(feof(test) || feof(class))) {
        for (int i = 0; i < dimension; i++) {
            fscanf(test, "%lf,", &(example.value[i]));
        }
        fscanf(test, "%c\n", &l);
        example.id++;
        //
        fscanf(class, "%d,%c,%d,%c,%lf,%lf\n",
            &(match.pointId), &(match.isMatch), &(match.clusterId),
            &(match.label), &(match.distance), &(match.radius)
        );
        if (match.pointId != example.id) {
            int i = 0;
            // after buffer is too big, reuse addresses
            if (mathcesBufferSize > 1000) {
                // try to replace an deleted buffer entry
                for (i = 0; i < mathcesBufferSize; i++) {
                    if (mathcesBuffer[i].pointId == -1) {
                        mathcesBuffer[i] = match;
                        break;
                    }
                }
                // if no deleted entry has been found, realloc
                if (i >= mathcesBufferSize) {
                    mathcesBuffer = realloc(mathcesBuffer, ++mathcesBufferSize * sizeof(Match));
                    mathcesBuffer[mathcesBufferSize -1] = match;
                }
                // try to replace an deleted buffer entry
                for (i = 0; i < examplesBufferSize; i++) {
                    if (examplesBuffer[i].id == -1) {
                        examplesBuffer[i] = example;
                        break;
                    }
                }
            }
            // if no deleted entry has been found, realloc
            if (i >= mathcesBufferSize) {
                examplesBuffer = realloc(examplesBuffer, ++examplesBufferSize * sizeof(Point));
                examplesBuffer[examplesBufferSize - 1] = example;
            }
            for (int i = 0; i < mathcesBufferSize; i++) {
                for (int j = 0; j < examplesBufferSize; j++) {
                    if (mathcesBuffer[i].pointId == examplesBuffer[j].id) {
                        match = mathcesBuffer[i];
                        example = examplesBuffer[j];
                        // break outer
                        i = mathcesBufferSize;
                        break;
                    }
                }
            }
        }
        //
        i = findLabelIndex(&confusionSize, &labels, &confusionMatrix, l);
        j = findLabelIndex(&confusionSize, &labels, &confusionMatrix, match.isMatch == 'y' ? match.label : '-');
        //
        confusionMatrix[j][i]++;
        if (match.isMatch == 'y' && l == match.label) hits++;
    }
    fclose(test);
    fclose(class);
    //
    double hitPC = ((double) hits) / ((double) example.id);
    
    printMatrix(&confusionSize, &labels, &confusionMatrix);
    printf("Total \t%d\n", example.id);
    printf("Hits \t%d (%f%%)\n", hits, hitPC * 100.0);
    printf("Misses \t%d (%f%%)\n", example.id - hits, (1 - hitPC) * 100.0);
    //
    for (i = 0; i < confusionSize; i++) {
        free(confusionMatrix[i]);
    }
    free(confusionMatrix);
    free(labels);
    printf("Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    exit(EXIT_SUCCESS);
}
