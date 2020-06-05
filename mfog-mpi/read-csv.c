#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    char *filename = "datasets/model-clean.csv";
    FILE *file = fopen(filename, "r");
    int lineLen = 10 * 1024, fieldSize = 27;
    char line[lineLen], *fields[fieldSize];
    int fieldPtr;
    // id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
    // %d,%c,%c,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n
    // 0, 1,    2,       3,         4,      5,      6
    int id, matches,time;
    char label,category;
    float meanDistance,radius, center[22];
    float c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21;
    for (int i = 0; i < lineLen; i++) line[i] = '\0';
    char *cur, *i;
    while(fgets(line, lineLen, file)) {
        i = line;
        if (*i == '#') {
            printf("skip=%s", line);
            continue;
        }
        fprintf(stderr, "line='%s'\n", line);
        // sscanf(line, "%d,%c,%c,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n",
        sscanf(line, "%d,%c,%c,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            &id, &label, &category, &matches, &time, &meanDistance, &radius,
            &c0, &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9,
            &c10, &c11, &c12, &c13, &c14, &c15, &c16, &c17, &c18, &c19,
            &c20, &c21
        );
        fprintf(stderr, "Cluster(id=%d, lbl=%c, cat=%c, match=%d, time=%d, mean=%f, r=%f, c=",
            id, label, category, matches, time, meanDistance, radius
        );
        printf(
            "%d,%c,%c,%d,%d,%1.17e,%1.17e,"
            "%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,"
            "%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,%1.17e,"
            "%1.17e,%1.17e,%1.17e,%1.17e\n",
            id, label, category, matches, time, meanDistance, radius,
            c0, c1, c2, c3, c4, c5, c6, c7, c8, c9,
            c10, c11, c12, c13, c14, c15, c16, c17, c18, c19,
            c20, c21
        );
        for (size_t i = 0; i < 22; i++) {
            fprintf(stderr, "%f, ", center[i]);
            // printf("%f, ", center[i]);
        }
        fprintf(stderr, ")\n");
    }
    fclose(file);

    return 0;
}
