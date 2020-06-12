#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>

#include "minas.h"
#include "loadenv.h"

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
    char *matchesCsv, *examplesCsv, *evaluateLog, *timingLog;
    FILE *matches, *examples, *evaluate, *timing;
    #define VARS_SIZE 4
    loadEnv(argc, argv, envp, VARS_SIZE,
        (char*[]) {"MATCHES_CSV", "EXAMPLES_CSV", "EVALUATE_LOG", "TIMING_LOG"},
        (char**[]) { &matchesCsv, &examplesCsv, &evaluateLog, &timingLog },
        (FILE**[]) { &matches, &examples, &evaluate, &timing },
        (char*[]) { "r", "r", "a", "a" }
    );
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
    char l;
    int i, j, hits = 0, mathcesBufferSize = 0, examplesBufferSize = 0;
    Match *mathcesBuffer = malloc(1 * sizeof(Match));
    Point *examplesBuffer = malloc(1 * sizeof(Point));
    while (!(feof(examples) || feof(matches))) {
        for (int i = 0; i < dimension; i++) {
            fscanf(examples, "%lf,", &(example.value[i]));
        }
        fscanf(examples, "%c\n", &l);
        example.id++;
        //
        fscanf(matches, "%d,%c,%d,%c,%lf,%lf\n",
            &(match.pointId), &(match.isMatch), &(match.clusterId),
            &(match.label), &(match.distance), &(match.radius)
        );
        if (match.pointId != example.id) {
            int i = 0;
            // after buffer is too big, reuse addresses
            if (mathcesBufferSize > 100) {
                // try to replace an deleted buffer entry
                for (i = 0; i < mathcesBufferSize; i++) {
                    if (mathcesBuffer[i].pointId == -1) {
                        mathcesBuffer[i] = match;
                        break;
                    }
                }
            }
            // if no deleted entry has been found, realloc
            if (i >= mathcesBufferSize) {
                mathcesBuffer = realloc(mathcesBuffer, ++mathcesBufferSize * sizeof(Match));
                mathcesBuffer[mathcesBufferSize -1] = match;
            }
            if (mathcesBufferSize > 100) {
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
                        // delete
                        mathcesBuffer[i].pointId = -1;
                        examplesBuffer[j].id = -1;
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
    fclose(examples);
    fclose(matches);
    //
    double hitPC = ((double) hits) / ((double) example.id);
    
    // printMatrix(&confusionSize, &labels, &confusionMatrix);
    fprintf(evaluate,
        "      \t|\tClasses\n"
        "Labels\t|\t");
    for (i = 1; i < (confusionSize); i++) {
        fprintf(evaluate, "%10c\t", labels[i]);
    }
    fprintf(evaluate, "\n");
    for (i = 0; i < (confusionSize); i++) {
        fprintf(evaluate, "%6c\t|\t", labels[i]);
        for (j = 1; j < confusionSize; j++) {
            fprintf(evaluate, "%10d\t", confusionMatrix[i][j]);
        }
        fprintf(evaluate, "\n");
    }
    fprintf(evaluate, "Total \t%d\n", example.id);
    fprintf(evaluate, "Hits \t%d (%f%%)\n", hits, hitPC * 100.0);
    fprintf(evaluate, "Misses \t%d (%f%%)\n", example.id - hits, (1 - hitPC) * 100.0);
    //
    for (i = 0; i < confusionSize; i++) {
        free(confusionMatrix[i]);
    }
    free(confusionMatrix);
    free(labels);
    // # source, executable, build_date-time, wall-clock, function, elapsed, cores
    double elapsed = ((double)(clock() - start)) / 1000000.0;
    fprintf(timing, "%s,%s,%s %s,%ld,%s,%e,%d\n",
            __FILE__, argv[0], __DATE__, __TIME__, time(NULL), __FUNCTION__, elapsed, 1);
    fclose(timing);
    fclose(evaluate);
    exit(EXIT_SUCCESS);
}
