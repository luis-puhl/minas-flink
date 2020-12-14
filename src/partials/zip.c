#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char const *argv[]) {
    FILE **files = calloc(argc, sizeof(FILE*));
    //
    char **buffer = calloc(argc, sizeof(char *));
    unsigned long *buffersSize = calloc(argc, sizeof(unsigned long));
    unsigned long *buffersMaxSize = calloc(argc, sizeof(unsigned long));
    //
    int *fileDescriptors = calloc(argc, sizeof(int)), maxFD;
    //
    fd_set fds;
    FD_ZERO(&fds);
    //
    files[0] = stdin;
    for (size_t i = 0; i < argc; i++) {
        if (i > 0) {
            files[i] = fopen(argv[i], "r");
            // fprintf(stderr, "fopen('%s', 'r');\n", argv[i]);
        }
        fileDescriptors[i] = fileno(files[i]);
        FD_SET(fileDescriptors[i], &fds);
        if (fileDescriptors[i] > maxFD) {
            maxFD = fileDescriptors[i];
        }
        buffersSize[i] = 0;
        buffersMaxSize[i] = BUFSIZ;
        buffer[i] = calloc(buffersMaxSize[i], sizeof(char));
        memset(buffer[i], '\0', BUFSIZ);
    }
    //
    int stdoutFD = fileno(stdout);
    int streams = argc;
    while (streams) {
        streams = argc;
        int retval = select(maxFD + 1, &fds, NULL, NULL, NULL);
        if (retval <= 0) {
            continue;
        }
        for (size_t i = 0; i < argc; i++) {
            // fprintf(stderr, "[status %lu] FD_ISSET %d, buffer: %lu / %lu (%2.3f)\n",
            //         i, FD_ISSET(fileDescriptors[i], &fds),
            //         buffersSize[i], buffersMaxSize[i], ((float)buffersSize[i]) / ((float)buffersMaxSize[i]));
            if (feof(files[i]) || ferror(files[i])) {
                // fprintf(stderr, "eof %lu\n", i);
                streams--;
            } else if (FD_ISSET(fileDescriptors[i], &fds)) {
                unsigned long n = read(fileDescriptors[i], &buffer[i][buffersSize[i]], sizeof(char) * (buffersMaxSize[i] - buffersSize[i]));
                if (n == 0) {
                    // fprintf(stderr, "eof %lu\n", i);
                    streams--;
                    continue;
                }
                buffersSize[i] += n;
                size_t lineStart = 0;
                for (size_t lineEnd = 0; lineEnd < buffersSize[i]; lineEnd++) {
                    if (buffer[i][lineEnd] == '\n') {
                        write(stdoutFD, & buffer[i][lineStart], sizeof(char) * (lineEnd - lineStart + 1));
                        lineStart = lineEnd + 1;
                        lineEnd += 2;
                    }
                }
                // compress
                for (size_t k = 0; k < lineStart; k++) {
                    buffer[i][k] = buffer[i][k+lineStart];
                }
                buffersSize[i] -= lineStart;
                //
                if (buffersSize[i] == buffersMaxSize[i]) {
                    // size_t prevMaxSize = buffersMaxSize[i];
                    buffersMaxSize[i] += BUFSIZ;
                    // fprintf(stderr, "Realloc from %lu to %lu\n", prevMaxSize, buffersMaxSize[i]);
                    buffer[i] = realloc(buffer[i], buffersMaxSize[i] * sizeof(char));
                    // memset(&buffer[i][prevMaxSize], '\0', BUFSIZ);
                }
            } else {
                streams--;
            }
        }
    }
    //
    for (size_t i = 0; i < argc; i++) {
        if (i > 0) fclose(files[i]);
        free(buffer[i]);
    }
    return 0;
}
