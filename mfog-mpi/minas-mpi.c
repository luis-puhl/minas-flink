#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <err.h>
#include <string.h>
#include <mpi.h>

#include "./minas.c"

extern int MNS_dimesion;

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int clRank, clSize;
    MPI_Comm_size(MPI_COMM_WORLD, &clSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &clRank);
    if (argc != 3) {
        errx(EXIT_FAILURE, "Missing arguments, expected 2, got %d\n", argc - 1);
    }
    MNS_dimesion = 22;
    clock_t start = clock();
    //
    Model *model;
    // read and Broadcast Model
    if (clRank == 0) {
        fprintf(stderr, "Reading model from \t'%s'\nReading test from \t'%s'\n", argv[1], argv[2]);
        model = MNS_readModelFile(argv[1]);
        fprintf(stderr, "Model read in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
        //
        int len;
        for (int dest = 1; dest < clSize; dest++) {
            fprintf(stderr, "Sending to %d\n", dest);
            MPI_Send(&model->size, 1, MPI_INT, dest, 2000, MPI_COMM_WORLD);
            for (int i = 0; i < model->size; i++) {
                Cluster *cl = &(model->vals[i]);
                MPI_Send(&cl, sizeof(Cluster), MPI_BYTE, dest, 2002, MPI_COMM_WORLD);
                len = strlen(cl->label) + 1;
                MPI_Send(&len, 1, MPI_INT, dest, 2002, MPI_COMM_WORLD);
                MPI_Send(&(cl->label), len, MPI_CHAR, dest, 2002, MPI_COMM_WORLD);
                len = strlen(cl->category) + 1;
                MPI_Send(&len, 1, MPI_INT, dest, 2002, MPI_COMM_WORLD);
                MPI_Send(&(cl->category), len, MPI_CHAR, dest, 2002, MPI_COMM_WORLD);;
                MPI_Send(cl->center, MNS_dimesion, MPI_FLOAT, dest, 2002, MPI_COMM_WORLD);
            }
        }
        fprintf(stderr, "Model sent in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
    } else {
        fprintf(stderr, "Waiting for model\n");
        MPI_Status status;
        model = malloc(sizeof(Model));
        MPI_Recv(&(model->size), 1, MPI_INT, MPI_ANY_SOURCE, 2000, MPI_COMM_WORLD, &status);
        model->vals = malloc(model->size * sizeof(Cluster));
        int len;
        for (int i = 0; i < model->size; i++) {
            Cluster *cl = &(model->vals[i]);
            MPI_Recv(&cl, sizeof(Cluster), MPI_BYTE, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            MPI_Recv(&len, 1, MPI_INT, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            printf("len=%d\n", len);
            cl->label = malloc(len * sizeof(char));
            MPI_Recv(cl->label, len, MPI_CHAR, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            MPI_Recv(&len, 1, MPI_INT, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            cl->category = malloc(len * sizeof(char));
            MPI_Recv(cl->category, len, MPI_CHAR, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
            cl->center = malloc(MNS_dimesion * sizeof(float));
            MPI_Recv(cl->center, MNS_dimesion, MPI_FLOAT, MPI_ANY_SOURCE, 2002, MPI_COMM_WORLD, &status);
        }
        fprintf(stderr, "Model recv in \t%fs\n", ((double)(clock() - start)) / ((double)1000000));
    }
    fprintf(stderr, "Finalizing\n");
    MPI_Finalize();
    exit(EXIT_SUCCESS);

    //
    Point example;
    example.value = malloc(MNS_dimesion * sizeof(float));
    example.id = 0;
    Match match;
    //
    if (clRank == 0) {
        FILE *kyotoOnl = fopen(argv[2], "r");
        if (kyotoOnl == NULL) errx(EXIT_FAILURE, "bad file open '%s'", argv[2]);
        // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,A
        char l;
        printf("id,isMach,clusterId,label,distance,radius\n");
        int dest = 1;
        while (!feof(kyotoOnl)) {
            dest = dest >= clSize ? 1 : dest + 1;
            for (int i = 0; i < MNS_dimesion; i++) {
                fscanf(kyotoOnl, "%f,", &(example.value[i]));
            }
            fscanf(kyotoOnl, "%c\n", &l);
            //
            MPI_Send(&(example.id), 1, MPI_INT, dest, 2003, MPI_COMM_WORLD);
            MPI_Send(example.value, MNS_dimesion, MPI_FLOAT, dest, 2004, MPI_COMM_WORLD);
            // printPoint(example);
            MNS_classify(model, &example, &match);
            // if (match.label == '\0') {
            //     errx(EXIT_FAILURE, "bad match label '%c'\n", match.label);
            // }
            printf("%d,%c,%d,%s,%f,%f\n",
                match.pointId, match.isMatch, match.clusterId,
                match.label, match.distance, match.radius);
            example.id++;
        }
        fclose(kyotoOnl);
    } else {
        MPI_Status status;
        MPI_Recv(&(example.id), 1, MPI_INT, MPI_ANY_SOURCE, 2003, MPI_COMM_WORLD, &status);
        MPI_Recv(example.value, MNS_dimesion, MPI_FLOAT, MPI_ANY_SOURCE, 2004, MPI_COMM_WORLD, &status);
        //
        MNS_classify(model, &example, &match);
        printf("%d,%c,%d,%s,%f,%f\n",
            match.pointId, match.isMatch, match.clusterId,
            match.label, match.distance, match.radius);
    }
    MPI_Finalize();
    exit(EXIT_SUCCESS);
    // MNS_classifier(model, argv[2]);
    fprintf(stderr, "Total examples \t%d\n", example.id);

    for (int i = 0; i < model->size; i++) {
        free(model->vals[i].center);
    }
    free(model->vals);
    free(model);
    fprintf(stderr, "Done %s in \t%fs\n", argv[0], ((double)(clock() - start)) / ((double)1000000));
    MPI_Finalize();
    exit(EXIT_SUCCESS);
}
