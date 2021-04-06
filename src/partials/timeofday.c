#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

/*
Pensando num programa que deseje determinar seu consumo de recursos, total ou em
um trecho de código, isto é possível usando as chamadas gettimeofday(2) (ou
clock_gettime(2)) e getrusage(2):
*/

/*
  // para uso com gettimeofday(): precisão de microsegundos
  struct timeval {
     time_t      tv_sec;  // seconds
     suseconds_t tv_usec; // microseconds
   };
*/

int main(int argc, char const *argv[]) {
    struct timeval inic, fim;
    struct rusage r1, r2;

    // determina quanto foi consumido de recursos até aqui (getrusage)
    getrusage(RUSAGE_SELF, &r1);

    // determina o instante atual (gettimeofday)
    // int gettimeofday(struct timeval *tv, struct timezone *tz);
    gettimeofday(&inic, 0);

    // aqui entra o código cujo tempo se quer avaliar

    // verifica instante em que a execução do trecho de código foi concluída
    gettimeofday(&fim,0);

    // determina quanto foi consumido de recursos até aqui (getrusage)
    getrusage(RUSAGE_SELF, &r2);

    // diferença entre r2 e r1 (ru_utime e ru_stime) indica consumo de recursos para o trecho de código
    // diferença entre fim e inic indica o tempo decorrido (elapsed time)
    printf("Elapsed time:%f sec\n User time:%f sec\n System time:%f sec\n\n",
        (fim.tv_sec + fim.tv_usec/1e6) - (inic.tv_sec + inic.tv_usec/1e6),
        (r2.ru_utime.tv_sec + r2.ru_utime.tv_usec/1e6) - (r1.ru_utime.tv_sec + r1.ru_utime.tv_usec/1e6),
        (r2.ru_stime.tv_sec + r2.ru_stime.tv_usec/1e6) - (r1.ru_stime.tv_sec + r1.ru_stime.tv_usec/1e6));

    /*
    Outra forma de medir o tempo decorrido, usando clock_gettime, com precisão de
    nanosegundos (10-9)
    */
    /*
    // para uso com clock_gettime(): precisão de nanosegundos
    struct timespec {
        time_t  tv_sec;     // seconds
        long    tv_nsec;    // and nanoseconds
    }
    */
    float etime;
    struct timespec tv_start, tv_end;

    clock_gettime(CLOCK_REALTIME, &tv_start);

    // aqui entra o código cujo tempo se quer avaliar

    clock_gettime(CLOCK_REALTIME, &tv_end);

    // tempo decorrido: elapsed time
    etime = (tv_end.tv_sec + tv_end.tv_nsec/1e9) - 
            (tv_start.tv_sec + tv_start.tv_nsec/1e9) ;
}

long int toddiff(struct timeval *tod1, struct timeval *tod2) {
    long int t1, t2;

    t1 = tod1->tv_sec * 1e6 + tod1->tv_usec;

    t2 = tod2->tv_sec * 1e6 + tod2->tv_usec;

    return t1 - t2;
}
