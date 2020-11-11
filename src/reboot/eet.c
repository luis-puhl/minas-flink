/** eet - read from standard input and files, write to standard output.
 * 
 * Takes file names as arguments and `stdin` making a set.
 * Reads one line from each ready file at a time and writes to `stdout`.
 * Exits when the entire set is at EOF.
 *  Copyright 2019 Luis Puhl

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    const char *fileName;
    pthread_mutex_t *mutex;
} threadArgs;

void *thread(void * voidThreadArgs) {
    threadArgs *args = (threadArgs *) voidThreadArgs;
    FILE *stream;
    char isStdin = args->fileName[0] == '-' && args->fileName[1] == '\0';
    if (isStdin) {
        stream = stdin;
    } else {
        stream = fopen(args->fileName, "r");
    }
    size_t bufferSize = 400, lineLen = 0, nread = 0;
    char *lineptr = calloc(bufferSize, sizeof(char));
    while (!feof(stream)) {
        lineLen = getline(&lineptr, &bufferSize, stream);
        nread += lineLen;
        if (lineLen > 0 && !feof(stream)) {
            pthread_mutex_lock(args->mutex);
            // fwrite(lineptr, sizeof(char), lineLen, stdout);
            printf("%s", lineptr);
            pthread_mutex_unlock(args->mutex);
        }
    }
    free(lineptr);
    fprintf(stderr, "%s EOF at char %ld\n", args->fileName, nread);
    if (!isStdin) {
        fclose(stream);
    }
    return args;
}

int main(int argc, char const *argv[]) {
    threadArgs *args = calloc(argc, sizeof(threadArgs));
    pthread_t *pThreads = calloc(argc, sizeof(pthread_t));
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    //
    for (size_t i = 0; i < argc; i++) {
        if (i == 0) {
            args[i].fileName = "-";
        } else {
            args[i].fileName = argv[i];
        }
        args[i].mutex = &mutex;
        pthread_create(&(pThreads[i]), NULL, thread, &(args[i]));
    }
    for (size_t i = 0; i < argc; i++) {
        pthread_join(pThreads[i], NULL);
    }
    pthread_exit(NULL);
    free(pThreads);
    free(args);
    return 0;
}
