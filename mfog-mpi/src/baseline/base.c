#ifndef _BASE_C
#define _BASE_C

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <err.h>
#include <math.h>
// #include <time.h>
#include <ctype.h>

char *printableLabel(char label) {
    if (isalnum(label) || label == '-') {
        char *ret = calloc(2, sizeof(char));
        ret[0] = label;
        return ret;
    }
    char *ret = calloc(20, sizeof(char));
    sprintf(ret, "%d", label);
    return ret;
}

double euclideanSqrDistance(unsigned int dim, double a[], double b[]) {
    double distance = 0;
    for (size_t d = 0; d < dim; d++) {
        distance += (a[d] - b[d]) * (a[d] - b[d]);
    }
    return sqrt(distance);
}

double euclideanDistance(unsigned int dim, double a[], double b[]) {
    return sqrt(euclideanSqrDistance(dim, a, b));
}

#endif // !_BASE_C
