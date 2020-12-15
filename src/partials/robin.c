#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char const *argv[]) {
    fd_set inFds;
    FD_ZERO(&inFds);
    int stdinFD = fileno(stdin);
    int maxFD = stdinFD;
    FD_SET(stdinFD, &inFds);
    //
    int outputs = argc -1;
    fd_set fds;
    FD_ZERO(&fds);
    FILE **files = calloc(outputs, sizeof(FILE*));
    int *fileDescriptors = calloc(outputs, sizeof(int));
    for (size_t i = 0; i < outputs; i++) {
        fprintf(stderr, "fopen(%s)\n", argv[i + 1]);
        files[i] = fopen(argv[i + 1], "w");
        fileDescriptors[i] = fileno(files[i]);
        FD_SET(fileDescriptors[i], &fds);
        if (fileDescriptors[i] > maxFD) {
            maxFD = fileDescriptors[i];
        }
    }
    //
    char *line = NULL;
    unsigned long n = 0;
    unsigned int dest = 0;
    int streams = outputs;
    while (streams > 0) {
        streams = outputs;
        int retval = select(maxFD + 1, &inFds, &fds, NULL, NULL);
        if (retval <= 0 || !FD_ISSET(stdinFD, &inFds)) {
            continue;
        }
        int lineLen = getline(&line, &n, stdin);
        if (lineLen == -1 || feof(stdin) || ferror(stdin)) {
            fprintf(stderr, "EOF\n");
            break;
        }
        // printf("%p %s", line, line);
        if (line[0] == 'C') {
            // broadcast
            for (size_t i = 0; i < outputs; i++) {
                if (feof(files[i]) || ferror(files[i])) {
                    streams--;
                    continue;
                }
                write(fileDescriptors[i], line, sizeof(char) * lineLen);
            }
            fprintf(stderr, "b");
            continue;
        }
        fprintf(stderr, ".");
        unsigned int prevDest = dest;
        dest = (dest + 1) % outputs;
        do {
            for (; prevDest != dest; dest = (dest + 1) % outputs) {
                if (feof(files[dest]) || ferror(files[dest])) {
                    // fprintf(stderr, "eof %lu\n", i);
                    streams--;
                    continue;
                }
                if (FD_ISSET(fileDescriptors[dest], &fds)) {
                    write(fileDescriptors[dest], line, sizeof(char) * lineLen);
                    break;
                }
            }
            if (prevDest == dest) {
                fprintf(stderr, "s\n");
                int retval = select(maxFD + 1, NULL, &fds, NULL, NULL);
                if (retval <= 0) {
                    continue;
                }
            }
        } while(prevDest == dest);
        // fprintf(stderr, "\n");
    }
    //
    free(line);
    return 0;
}
