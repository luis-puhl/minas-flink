#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include "mpi.h"


#define THRESHOLD 5

#define SIZE (2 * THRESHOLD)

#define ERR_MSG_LEN 128

char _unknown_samples[SIZE];
int _out_pos = 0;

// char *_snd_buff;
// char *_rcv_buff;
int *_snd_buff;
int *_rcv_buff;

int _buflen;

char _received_samples[SIZE];

char _proc_name[MPI_MAX_PROCESSOR_NAME];
int _numprocs, _rank;

// Semaforo de dados para o emissor
sem_t _sender_sem;


void *
classifier(void *arg)
{
	int num, out_pos=0;

	do {
		// espera amostra 

		// operação fictícia aqui
		sleep(1);
		num=rand()%10;

		// printf("Classificador@%s (%d) analisou %d\n",_proc_name,_rank,num); fflush(stdout);

		// classifica...

		// se unknown, insere amostra desconhecida no buffer e acorda emissor
		if(num > THRESHOLD) {   // operacao ficticia aqui...

			// printf("Classificador@%s (%d) repassou %d\n",_proc_name,_rank,num); fflush(stdout);

			// insere amostra num buffer para emissor empacotar...
			_unknown_samples[out_pos]=num;
			out_pos = (out_pos+1) % SIZE;
			
			// acorda thread de envio (semáforo)
			sem_post(&_sender_sem);
		}

	} while(1);

	pthread_exit(NULL);
}

// monta e, ao atingir o threshold, envia mensagem com lista de amostras desconhecidas
void * 
u_sender(void *arg)
{
	int out_pos=0;
	int ind=0;
	int unknown=0;
	
	do {
		// espera threshold (semáforo)
		sem_wait(&_sender_sem);

		// insere amostra na lista de unknowns

		// int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype,
  		//              void *outbuf, int outsize, int *position, MPI_Comm comm);
		MPI_Pack(&_unknown_samples[out_pos], 1, MPI_INT, _snd_buff, _buflen, &ind, MPI_COMM_WORLD);

		printf("Emissor@%s (%d) empacotou %d. New size: %d\n",_proc_name,_rank,_unknown_samples[out_pos],ind); 
		fflush(stdout);

		out_pos = (out_pos+1) % SIZE;

		unknown++;

		// se atingiu threshold, 
		if (unknown >= THRESHOLD) {
			// envia amostras: mensagem bloqueante MPI

			printf("Emissor@%s (%d): enviando unknowns\n", _proc_name,_rank); fflush(stdout);

			// int MPI_Send(void *buf, int count, MPI_Datatype dtype, int dest, int tag, MPI_Comm comm)
			// MPI_Send(_snd_buff,unknown,MPI_INT,0,_rank,MPI_COMM_WORLD);

			// int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
			MPI_Bsend( _snd_buff, ind, MPI_PACKED, 0, _rank, MPI_COMM_WORLD);
			// ou
			// MPI_Bsend( _snd_buff, unknown, MPI_INT, 0, _rank, MPI_COMM_WORLD);

			unknown = 0;
			ind = 0;
		}

	} while(1);

	pthread_exit(NULL);
}

void *
m_receiver(void *arg)
{
	int newgroups;

	do {
		// Espera conjunto de novos grupos: mensagem bloqueante MPI, divulgada em broadcast
		
		// Recebe mensagem em broadcast. Emisso é o nó de rank 0
		MPI_Bcast(&newgroups, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
		printf("%s, rank %d, recebeu broadcast: %d\n", _proc_name, _rank, newgroups); fflush(stdout);

		// desempacota dados

		// cria nova lista de modelos

		// bloqueia acesso à lista
		// troca lista de modelos 
		// libera acesso à nova lista

	} while(1);

	pthread_exit(NULL);
}

int
main (int argc, char *argv[])
{
	int result, count, i, tag;
	char err_msg[ERR_MSG_LEN];
	double startwtime = 0.0, endwtime;
	int namelen;
	int newgroups;

	MPI_Status status;

	pthread_t class_t, sender_t, receiver_t;

	// srand(time(NULL));
	srand(getpid());

	// inicializa MPI. Fazer antes de criar threads, que vão usar mecanismos de comunicação
	result = MPI_Init(&argc,&argv);

	if (result != MPI_SUCCESS) {
		printf ("Erro iniciando programa MPI.\n");
		MPI_Abort(MPI_COMM_WORLD, result);
	}

	// Determina número de processos em execução na aplicação
	MPI_Comm_size(MPI_COMM_WORLD, &_numprocs);

	// Determina ranking desse processo no grupo
	MPI_Comm_rank(MPI_COMM_WORLD, &_rank);

	// Determina nome do host local
	MPI_Get_processor_name(_proc_name, &namelen);

	printf("Hello from %s, rank %d\n",_proc_name, _rank);

	// inicia semáforo do transmissor 

	// int sem_init (sem_t *sem, int pshared, unsigned int value);
	if (sem_init(&_sender_sem, 0, 0) < 0) {
		strerror_r(errno,err_msg,ERR_MSG_LEN);
		printf("Erro em sem_init: %s\n",err_msg);
		exit(1);
	}

	// determina espaço necessário para empacotamento dos dados no transmissor e recebimento no nó tratador
	MPI_Pack_size( THRESHOLD, MPI_INT, MPI_COMM_WORLD, &_buflen );
	_buflen += MPI_BSEND_OVERHEAD;


	// Verifica se é o processo 0 de rank 0 do MP: detector de novidades
	if (_rank == 0) {

		// Aloca buffer para recebimento	
		// _rcv_buff = (char *)malloc(_buflen);
		_rcv_buff = (int *)malloc(_buflen);


		// fica num loop � `a espera de unknonws, monta grupos e propaga a todos os classificadores
		do {

			printf("Rank 0 esperando dados...\n");

			// rank 0 recebe dos demais (MPI_Comm_size -1)
			// Uso de MPI_ANY_SOURCE e MPI_ANY_TAG: não se sabe a ordem de envio

			// int MPI_Recv(void* buf, int count, MPI_Datatype datatype,
			//              int source, int tag, MPI_Comm comm, MPI_Status *status);
			MPI_Recv( _rcv_buff, THRESHOLD, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&status);

			// int MPI_Unpack(void* inbuf, int insize, int *position, void *outbuf, int outcount, 
			//                MPI_Datatype datatype, MPI_Comm comm);
			// MPI_Unpack(_rcv_buff, insize, position, outbuf, outcount, MPI_INT, MPI_COMM_WORLD);

			// verifica quantos dados, do tipo desejado, foram recebidos
			// Returns the number of entries received.
			// int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count);
			result = MPI_Get_count(&status, MPI_INT, &count);

			// After the MPI_Recv() returns with a message, the source id of the sender and the tag of the message:
			// status.MPI_SOURCE  = source id of sender    
			// status.MPI_TAG     = tag of message    

			printf("Rank 0 recebeu: %d INTs de %d\n", count, status.MPI_TAG);

			for(int i=0; i < count; i++) 
				printf("%d: (%d)  ",i,(int) _rcv_buff[i]);
			printf("\n"); fflush(stdout);

			// monta nova lista de grupos
			newgroups =	rand() % count;
			// empacota informações para envio
			
			// propaga lista a todos os nós: nó 0 é emissor, demais nós recebem
			MPI_Bcast(&newgroups, 1, MPI_INT, 0, MPI_COMM_WORLD);

		} while (1);

		
	} else { // processos classificadores de amostra 

		// Aloca buffer para transmissão das amostras unknown empacotadas	
		// _snd_buff = (char *)malloc(_buflen);
		_snd_buff = (int *)malloc(_buflen);

		// associa o buffer para envio
		MPI_Buffer_attach(_snd_buff, _buflen);

		// Cria threads
		result = pthread_create(&class_t, NULL, classifier, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		} result = pthread_create(&sender_t, NULL, u_sender, (void *)NULL); if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		}
		result = pthread_create(&receiver_t, NULL, m_receiver, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		// espera threads terminarem

		// result = pthread_join(thread[t], (void **)ret);
		result = pthread_join(class_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		result = pthread_join(sender_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		result = pthread_join(receiver_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
	}

	sem_destroy(&_sender_sem);

	// ambos os processos executam a finalização MPI
	MPI_Finalize();

	return 0;
}




