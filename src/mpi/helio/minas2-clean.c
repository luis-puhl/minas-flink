#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <mpi.h>

#define ERR_MSG_LEN 128
#define THRESHOLD 5
#define SIZE (2 * THRESHOLD)
#define SAMPLES 2
#define SAMP_TAG 1
#define UNK_TAG 2

int _unknown_samples[SIZE], _out_pos = 0, *_snd_buff, *_rcv_buff, *_model_buff, _buflen, _numprocs, _rank;
char _proc_name[MPI_MAX_PROCESSOR_NAME];
sem_t _sender_sem;

void * classifier(void *arg) {
    int i, num, out_pos = 0, in_pos = 0;
    MPI_Status status;
    for (size_t i = 0; i < 10; i++) {
        MPI_Recv(_rcv_buff, SAMPLES, MPI_INT, 0, SAMP_TAG, MPI_COMM_WORLD, &status);
        in_pos = 0;
        for (i = 0; i < SAMPLES; i++) {
            MPI_Unpack(_rcv_buff, _buflen, &in_pos, &num, 1, MPI_INT, MPI_COMM_WORLD);
            if (num > THRESHOLD) {
                _unknown_samples[out_pos] = num;
                printf("classifier@%s (%d) repassou %d\n", _proc_name, _rank, num);
                fflush(stdout);
                out_pos = (out_pos + 1) % SIZE;
                sem_post(&_sender_sem);
            }
        }
    }
    pthread_exit(NULL);
}

void *u_sender(void *arg) {
    int out_pos = 0;
    int ind = 0;
    int unknown = 0;
    for (size_t i = 0; i < 10; i++) {
        sem_wait(&_sender_sem);
        MPI_Pack(&_unknown_samples[out_pos], 1, MPI_INT, _snd_buff, _buflen, &ind, MPI_COMM_WORLD);
        printf("u_sender@%s (%d) empacotou %d (%d). New size: %d\n", _proc_name, _rank,
               _unknown_samples[out_pos], out_pos, ind);
        fflush(stdout);
        out_pos = (out_pos + 1) % SIZE;
        unknown++;
        if (unknown >= THRESHOLD) {
            printf("u_sender@%s (%d): enviando unknowns: ", _proc_name, _rank);
            fflush(stdout);
            for (int i = 0; i < unknown; i++)
                printf("%d: (%d)  ", i, (int)_snd_buff[i]);
            printf("\n");
            fflush(stdout);
            MPI_Bsend(_snd_buff, ind, MPI_PACKED, 0, UNK_TAG, MPI_COMM_WORLD);
            unknown = 0;
            ind = 0;
        }
    }
    pthread_exit(NULL);
}

void *m_receiver(void *arg) {
    int newgroups;
    for (size_t i = 0; i < 10; i++) {
        MPI_Bcast(&_model_buff, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }
    pthread_exit(NULL);
}

void *sampler(void *arg) {
    int i, ind, num;
    int dest = 1;
    for (size_t i = 0; i < 10; i++) {
        ind = 0;
        for (i = 0; i < SAMPLES; i++) {
            usleep(50000);
            num = rand() % 10;
            MPI_Pack(&num, 1, MPI_INT, _snd_buff, _buflen, &ind, MPI_COMM_WORLD);
        }
        MPI_Send(_snd_buff, SAMPLES, MPI_INT, dest, SAMP_TAG, MPI_COMM_WORLD);
        dest = (dest + 1) % _numprocs;
        if (!dest)
            dest = 1;
    }
    pthread_exit(NULL);
}

void *detector(void *arg) {
    MPI_Status status;
    int result, count, num, ind, out_pos;
    for (size_t i = 0; i < 10; i++) {
        MPI_Recv(_rcv_buff, THRESHOLD, MPI_INT, MPI_ANY_SOURCE, UNK_TAG, MPI_COMM_WORLD, &status);
        result = MPI_Get_count(&status, MPI_INT, &count);
        printf("detector@%s (%d) recebeu %d INTs de %d: ", _proc_name, _rank, count, status.MPI_SOURCE);
        fflush(stdout);
        for (int i = 0; i < count; i++)
            printf("%d: (%d)  ", i, (int)_rcv_buff[i]);
        printf("\n");
        fflush(stdout);
        num = rand() % 10;
        ind = 0;
        MPI_Pack(&num, 1, MPI_INT, _model_buff, _buflen, &ind, MPI_COMM_WORLD);
        MPI_Bcast(_model_buff, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int result, count, i, tag, namelen, provided;
    char err_msg[ERR_MSG_LEN];
    pthread_t class_t, send_t, rec_t, det_t, samp_t;
    srand(getpid());
    result = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (result != MPI_SUCCESS || provided != MPI_THREAD_MULTIPLE) {
        printf("Erro iniciando programa MPI.\n");
        MPI_Abort(MPI_COMM_WORLD, result);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &_numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
    MPI_Get_processor_name(_proc_name, &namelen);
    printf("Hello from %s, rank %d\n", _proc_name, _rank);
    if (sem_init(&_sender_sem, 0, 0) < 0) {
        strerror_r(errno, err_msg, ERR_MSG_LEN);
        printf("Erro em sem_init: %s\n", err_msg);
        exit(1);
    }
    MPI_Pack_size(THRESHOLD, MPI_INT, MPI_COMM_WORLD, &_buflen);
    _buflen += MPI_BSEND_OVERHEAD;
    _rcv_buff = (int *)malloc(_buflen);
    _model_buff = (int *)malloc(_buflen);
    _snd_buff = (int *)malloc(_buflen);
    MPI_Buffer_attach(_snd_buff, _buflen);
    if (_rank == 0) {
        result = pthread_create(&samp_t, NULL, sampler, (void *)NULL);
        if (result) {
            strerror_r(result, err_msg, ERR_MSG_LEN);
            printf("Falha da criacao de thread: %s\n", err_msg);
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
        result = pthread_create(&det_t, NULL, detector, (void *)NULL);
        if (result) {
            strerror_r(result, err_msg, ERR_MSG_LEN);
            printf("Falha da criacao de thread: %s\n", err_msg);
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
        result = pthread_join(det_t, (void **)&result);
        if (result) {
            strerror_r(result, err_msg, ERR_MSG_LEN);
            printf("Erro em pthread_join: %s\n", err_msg);
        }
        result = pthread_join(samp_t, (void **)&result);
        if (result) {
            strerror_r(result, err_msg, ERR_MSG_LEN);
            printf("Erro em pthread_join: %s\n", err_msg);
        }
    } else {
        struct tCaller {
            pthread_t* a;
            void *(*b)(void *);
        };
        struct tCaller threads[3] = {
            {&class_t, classifier},
            {&send_t, u_sender},
            {&rec_t, m_receiver},
        };
        for (size_t i = 0; i < 3; i++){
            result = pthread_create(threads[i].a, NULL, threads[i].b, (void *)NULL);
            if (result) {
                strerror_r(result, err_msg, ERR_MSG_LEN);
                printf("Falha da criacao de thread: %s\n", err_msg);
                MPI_Finalize();
                exit(EXIT_FAILURE);
            }
        }
        for (size_t i = 0; i < 3; i++){
            result = pthread_join(*threads[i].a, (void **)&result);
            if (result) {
                strerror_r(result, err_msg, ERR_MSG_LEN);
                printf("Erro em pthread_join: %s\n", err_msg);
            }
        }
    }
    sem_destroy(&_sender_sem);
    MPI_Finalize();
    return 0;
}
