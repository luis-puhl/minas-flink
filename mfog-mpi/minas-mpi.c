#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "./minas.h"

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int clRank, clSize;
    MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    //
    char *modelName = "datasets/model-clean.csv";
    char *testName = "datasets/test.csv";
    #define line_len 10 * 1024
    char line[line_len + 1];
    int dimension = 22;
    clock_t start = clock();
    FILE *file;

    Model model;
    model.size = 0;
    model.vals = malloc(1 * sizeof(Cluster));
    if (clRank == 0) {
        fprintf(stderr, "Reading model from \t%-30s\n", modelName);
        file = fopen(modelName, "r");
        if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", modelName);
        while (fgets(line, line_len, file)) {
            if (line[0] == '#') continue;
            model.vals = realloc(model.vals, (++model.size) * sizeof(Cluster));
            Cluster *cl = &(model.vals[model.size - 1]);
            cl->center = malloc(dimension * sizeof(float));
            // #id,label,category,matches,time,meanDistance,radius,c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,c20,c21
            int assigned = sscanf(line,
                "%d,%c,%c,"
                "%d,%d,%f,%f,"
                "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
                &cl->id, &cl->label, &cl->category,
                &cl->matches, &cl->time, &cl->meanDistance, &cl->radius,
                &cl->center[0], &cl->center[1], &cl->center[2], &cl->center[3], &cl->center[4],
                &cl->center[5], &cl->center[6], &cl->center[7], &cl->center[8], &cl->center[9],
                &cl->center[10], &cl->center[11], &cl->center[12], &cl->center[13], &cl->center[14],
                &cl->center[15], &cl->center[16], &cl->center[17], &cl->center[18], &cl->center[19],
                &cl->center[20], &cl->center[21]
            );
            if (assigned != 29) errx(EXIT_FAILURE, "File with wrong format  '%s'", testName);
        }
        fclose(file);
        fprintf(stderr, "Model with %d clusters took \t%-30fs\n", model.size, ((double)(clock() - start)) / ((double)1000000));
    }

    Point ex;
    ex.value = malloc(dimension * sizeof(float));
    ex.id = -1;
    Match match;
    if (clRank == 0) {
        printf("id,isMach,clusterId,label,distance,radius\n");
        fprintf(stderr, "Reading test from %20s\n", testName);
        file = fopen(testName, "r");
        if (file == NULL) errx(EXIT_FAILURE, "bad file open '%s'", testName);
        while (fgets(line, line_len, file)) {
            if (line[0] == '#') continue;
            // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
            int assigned = sscanf(line,
                "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,"
                "%c\n",
                &ex.value[0], &ex.value[1], &ex.value[2], &ex.value[3], &ex.value[4],
                &ex.value[5], &ex.value[6], &ex.value[7], &ex.value[8], &ex.value[9],
                &ex.value[10], &ex.value[11], &ex.value[12], &ex.value[13], &ex.value[14],
                &ex.value[15], &ex.value[16], &ex.value[17], &ex.value[18], &ex.value[19],
                &ex.value[20], &ex.value[21], &ex.label
            );
            ex.id++;
            if (assigned != 23) errx(EXIT_FAILURE, "File with wrong format  '%s'", testName);
            //
            match.distance = (float) dimension;
            match.pointId = ex.id;
            for (int i = 0; i < model.size; i++) {
                double distance = 0;
                for (int j = 0; j < dimension; j++) {
                    float diff = model.vals[i].center[j] - ex.value[j];
                    distance += diff * diff;
                }
                if (match.distance > distance) {
                    match.clusterId = model.vals[i].id;
                    match.label = model.vals[i].label;
                    match.radius = model.vals[i].radius;
                    match.distance = distance;
                }
            }
            match.isMatch = match.distance <= match.radius ? 'y' : 'n';
            
            printf("%d,%c,%d,%c,%f,%f\n",
                match.pointId, match.isMatch, match.clusterId,
                match.label, match.distance, match.radius);
        }
        fclose(file);
    }
    
    fprintf(stderr, "Finalizing\n");
    MPI_Finalize();
    
    fprintf(stderr, "Classifier with %d examples took \t%-30fs\n", ex.id, ((double)(clock() - start)) / ((double)1000000));
    return 0;
}

// void distribute(int clRank, int clSize, char *testName, int dimension) {
//     clock_t start = clock();
//     char *modelName = "datasets/model-clean.csv";
//     Model *model;
//     // read and Broadcast Model
//     if (clRank == 0) {
//         model = MNS_readModelFile(modelName);
//         //
//         for (int dest = 1; dest < clSize; dest++) {
//             fprintf(stderr, "Sending to %d\n", dest);
//             MPI_Send(&model->size, 1, MPI_INT, dest, 2000, MPI_COMM_WORLD);
//             for (int i = 0; i < model->size; i++) {
//                 Cluster *cl = &(model->vals[i]);
//                 MPI_Send(&cl, sizeof(Cluster), MPI_BYTE, dest, 2002, MPI_COMM_WORLD);
//                 MPI_Send(cl->center, dimension, MPI_FLOAT, dest, 2002, MPI_COMM_WORLD);
//             }
//         }
//         fprintf(stderr, "Model sent in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
//     } else {
//         fprintf(stderr, "Waiting for model\n");
//         MPI_Status status;
//         model = malloc(sizeof(Model));
//         MPI_Recv(&(model->size), 1, MPI_INT, MPI_ANY_SOURCE, 2000, MPI_COMM_WORLD, &status);
//         model->vals = malloc(model->size * sizeof(Cluster));
//         for (int i = 0; i < model->size; i++) {
//             Cluster *cl = &(model->vals[i]);
//             MPI_Recv(&cl, sizeof(Cluster), MPI_BYTE, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
//             cl->center = malloc(dimension * sizeof(float));
//             MPI_Recv(cl->center, dimension, MPI_FLOAT, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
//         }
//         fprintf(stderr, "Model recv in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
//     }
//     fprintf(stderr, "Finalizing\n");
//     MPI_Finalize();
//     exit(EXIT_SUCCESS);
// }


// int classifier(int clRank, int clSize, char *testName, Model *model, int dimension) {
//     clock_t start = clock();
//     //
//     Point example;
//     example.value = malloc(dimension * sizeof(float));
//     example.id = 0;
//     Match match;
//     //
//     if (clRank == 0) {
//         fprintf(stderr, "Reading test from %20s\n", testName);
//         FILE *kyotoOnl = fopen(testName, "r");
//         if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", testName);
//         // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
//         char l;
//         printf("id,isMach,clusterId,label,distance,radius\n");
//         int dest = 1;
//         while (!feof(kyotoOnl)) {
//             dest = dest >= clSize ? 1 : dest + 1;
//             for (int i = 0; i < dimension; i++) {
//                 fscanf(kyotoOnl, "%f,", &(example.value[i]));
//             }
//             fscanf(kyotoOnl, "%c\n", &l);
//             //
//             MPI_Send(&(example.id), 1, MPI_INT, dest, 2003, MPI_COMM_WORLD);
//             MPI_Send(example.value, dimension, MPI_FLOAT, dest, 2004, MPI_COMM_WORLD);
//             // printPoint(example);
//             MNS_classify(model, &example, &match);
//             // if (match.label == '\0') {
//             //     errx(EXIT_FAILURE, "bad match label '%c'\n", match.label);
//             // }
//             printf("%d,%c,%d,%c,%f,%f\n",
//                 match.pointId, match.isMatch, match.clusterId,
//                 match.label, match.distance, match.radius);
//             example.id++;
//         }
//         fclose(kyotoOnl);
//     } else {
//         MPI_Status status;
//         MPI_Recv(&(example.id), 1, MPI_INT, MPI_ANY_SOURCE, 2003, MPI_COMM_WORLD, &status);
//         MPI_Recv(example.value, dimension, MPI_FLOAT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
//         //
//         MNS_classify(model, &example, &match);
//         printf("%d,%c,%d,%c,%f,%f\n",
//             match.pointId, match.isMatch, match.clusterId,
//             match.label, match.distance, match.radius);
//     }
//     MPI_Finalize();
//     exit(EXIT_SUCCESS);
//     // MNS_classifier(model, argv[2]);
//     fprintf(stderr, "Total examples \t%d\n", example.id);

//     for (int i = 0; i < model->size; i++) {
//         free(model->vals[i].center);
//     }
//     free(model->vals);
//     free(model);
//     fprintf(stderr, "Done %s in \t%fs\n", __FUNCTION__, ((double)(clock() - start)) / ((double)1000000));
//     MPI_Finalize();
//     exit(EXIT_SUCCESS);
// }
