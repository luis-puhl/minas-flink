#include <poll.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../baseline/base.h"
#include "../baseline/minas.h"

#include "../util/net.h"

#include "./modules.h"

int appendClusterFromStore(Params *params, SOCKET modelStore, char *buffer, size_t buffSize, Model *model) {
    // printf("cluster line (%ld) \t'%s'\n", buffSize, buffer);
    int assigned = 0, consumed = 0, consumedStep = 0;
    if (model->size > 0 && model->size % params->k == 0) {
        model->clusters = realloc(model->clusters, model->size + params->k * sizeof(Cluster));
    }
    //
    Cluster *currCluster = &(model->clusters[model->size]);
    char *labelString;
    assigned += sscanf(buffer, "%10u, %m[a-zA-z0-9], %10u, %le, %le, %le%n",
                       &currCluster->id, &labelString, &currCluster->n_matches,
                       &currCluster->distanceAvg, &currCluster->distanceStdDev,
                       &currCluster->radius, &consumedStep
    );
    consumed += consumedStep;
    if (consumed >= buffSize) return 0;
    // "         1, N,        311, 9.625003e-02, 1.105641e+00, 1.105641e-01" // 67
    // fprintf(stderr, "id=%10u, l=%s, n=%10u, dAvg=%le, dDev=%le, r=%le, assingned=%d, consumed=%d\n",
    //         currCluster->id, labelString, currCluster->n_matches,
    //         currCluster->distanceAvg, currCluster->distanceStdDev, currCluster->radius,
    //         assigned, consumed);
    // assertEquals(assigned, 7);
    if (assigned != 6)
        // errx(EXIT_FAILURE, "Assert error, expected '%d' and got '%d'. At "__FILE__":%d\n", 6, assigned, __LINE__);
        return 0;
    if (labelString[1] == '\0' && isalpha(labelString[0])) {
        // single char
        currCluster->label = labelString[0];
    } else {
        currCluster->label = atoi(labelString);
    }
    free(labelString);
    //
    // ", 1.574879e-03, 2.128046e-02, 4.975385e-02, 5.661461e-02, 1.000000e+00, 5.723473e-03, 1.565916e-02, 3.270096e-02, 4.614469e-01, 0.000000e+00, 2.893891e-04, 4.469453e-03, 0.000000e+00, 0.000000e+00, 1.000000e+00, 0.000000e+00, 0.000000e+00, 0.000000e+00, 0.000000e+00, 1.000000e+00, 0.000000e+00, 0.000000e+00" 308
    // ", 1.574879e-03" // 14
    if (currCluster->center == NULL) {
        currCluster->center = calloc(params->dim, sizeof(double));
    }
    for (unsigned int d = 0; d < params->dim; d++) {
        if (consumed >= buffSize) return 0;
        assigned += sscanf(&buffer[consumed], ", %le%n", &currCluster->center[d], &consumedStep);
        consumed += consumedStep;
        // fprintf(stderr, "d=%d, v=%le, assingned=%d, consumedStep=%d, consumed=%d\n", d, currCluster->center[d], assigned, consumedStep, consumed);
    }
    // assertEquals(consumed, buffSize);
    if (consumed != buffSize)
        errx(EXIT_FAILURE, "Assert error, expected 'buffSize' '%lu' and got 'consumed' '%d'. At "__FILE__":%d\n",
             buffSize, consumed, __LINE__);
    // assigned += sscanf(buffer, "\0%n", &consumedStep);
    // consumed += consumedStep;
    // fprintf(stderr, "Cluster(id=%u, l=%s, n=%u, dAvg=%le, dDev=%le, r=%le, assingned=%d)\n",
    //         currCluster->id, printableLabel(currCluster->label), currCluster->n_matches,
    //         currCluster->distanceAvg, currCluster->distanceStdDev, currCluster->radius,
    //         assigned);
    // fprintf(stderr, "Cluster(id=%u)\n", currCluster->id);
    model->size++;
    return consumed;
}

int modelStoreComm(Params *params, int timeout, Model *model, SOCKET modelStore, struct pollfd *modelStorePoll, char *buffer, size_t maxBuffSize) {
    size_t prevSize = model->size;
    while (poll(modelStorePoll, 1, timeout) != 0 && modelStorePoll->revents != 0 && modelStorePoll->revents & POLLIN) {
        bzero(buffer, maxBuffSize);
        ssize_t buffRead = read(modelStore, buffer, maxBuffSize - 1);
        if (buffRead < 0)
            errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
        if (buffRead == 0)
            fprintf(stderr, "EOF model\n");
        // fprintf(stderr, "buffer \t'%s'\n", buffer);
        int consumed = 0, lineSize = 0;
        char *line;
        while (consumed != buffRead) {
            line = &buffer[lineSize];
            while (buffer[lineSize] != '\n' && lineSize < buffRead) lineSize++;
            // printf("line (%d) \t'%s' /%s\n", lineSize, line, &line[lineSize - 1]);
            if (lineSize == 0 || line[0] == '\0' || line[lineSize] == '\0') {
                break;
            }
            if (buffer[lineSize] == '\n') {
                buffer[lineSize] = '\0';
            } else {
                fprintf(stderr, "incomplete line '%s'\n", line);
                // if (lineSize >= buffRead) {
                // compact buffer and read;
                fprintf(stderr, "compact buffer lineSize=%d, buffRead=%ld\n", lineSize, buffRead);
                for (size_t i = 0; i < lineSize && (lineSize + i) < buffRead; i++) {
                    buffer[i] = buffer[i + lineSize];
                }
                int locBuffRead = read(modelStore, &buffer[lineSize - 1], maxBuffSize - lineSize - 1);
                fprintf(stderr, "compact buffer locBuffRead=%d\n", locBuffRead);
                if (locBuffRead < 0)
                    errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
                if (locBuffRead == 0) {
                    buffRead = 0;
                    break;
                }
                buffRead = locBuffRead + lineSize;
                lineSize = 0;
                continue;
            }
            if (line[0] == '#' || lineSize == 0) {
                lineSize++;
                continue;
            }
            consumed += appendClusterFromStore(params, modelStore, line, lineSize, model);
        }
        // fprintf(stderr, "buffRead %d\n", buffRead);
    }
    if (prevSize < model->size)
        fprintf(stderr, "Model(size=%d)\n", model->size);
    return model->size;
}
