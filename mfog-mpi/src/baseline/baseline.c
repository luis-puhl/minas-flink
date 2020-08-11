#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

// #define SQR_DIST 1

#include "./base.h"
#include "./minas.h"
// #include "./kmeans.h"
// #include "./clustream.h"

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        fprintf(stderr, "reading from file %s\n", argv[1]);
        stdin = fopen(argv[1], "r");
    }
    Params params;
    params.executable = argv[0];
    fprintf(stderr, "%s\n", params.executable);
    getParams(params);
    #ifdef SQR_DIST
    fprintf(stderr, "\tSQR_DIST = 1\n");
    #endif

    Model *model = training(&params);

    minasOnline(&params, model);

    return EXIT_SUCCESS;
}
