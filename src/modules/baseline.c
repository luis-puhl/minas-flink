#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#define USE_MPI 0

#include "../base/base.h"
#include "../base/minas.h"

int main(int argc, char const *argv[], char *env[]) {
    clock_t start = clock();
    Params *params = setup(argc, argv, env);

    Model *model = training(params);

    minasOnline(params, model);

    tearDown(argc, argv, env, params);
    printTiming(main, 1);
    printTimeLog(params);
    return EXIT_SUCCESS;
}
