#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include "mpi.h"


#define ERR_MSG_LEN 128

#define THRESHOLD 5
#define SIZE (2 * THRESHOLD)

#define SAMPLES 2

#define SAMP_TAG 1
#define UNK_TAG 2

// char _unknown_samples[SIZE];
int _unknown_samples[SIZE];
int _out_pos = 0;

// char *_snd_buff;
// char *_rcv_buff;
// char *_model_buff;
int *_snd_buff;
int *_rcv_buff;
int *_model_buff;

int _buflen;

char _proc_name[MPI_MAX_PROCESSOR_NAME];
int _numprocs, _rank;

// Semaforo de dados para o emissor
sem_t _sender_sem;


void *
classifier(void *arg)
{
	int i, num, out_pos=0, in_pos=0;
	MPI_Status status;
	
	do {
		// espera amostra vinda do rank 0, com TAG SAMP_TAG
		// int MPI_Recv(void* buf,int count,MPI_Datatype datatype, int source, int tag,MPI_Comm comm,MPI_Status *status);
		// MPI_Recv(_rcv_buff, SAMPLES, MPI_PACKED, 0, MPI_ANY_TAG, MPI_COMM_WORLD,&status);
		MPI_Recv(_rcv_buff, SAMPLES, MPI_INT, 0, SAMP_TAG, MPI_COMM_WORLD,&status);

		in_pos = 0;

		for(i=0; i< SAMPLES; i++) {

			// int MPI_Unpack(const void *inbuf, int insize, int *position, void *outbuf, int outcount, 
			//                MPI_Datatype datatype, MPI_Comm comm)
			MPI_Unpack(_rcv_buff, _buflen, &in_pos, &num, 1, MPI_INT, MPI_COMM_WORLD);

			// printf("classifier@%s (%d) analisou %d\n",_proc_name,_rank,num); fflush(stdout);

			// classifica... de forma simulada aqui... 

			// se unknown, insere amostra desconhecida no buffer e acorda emissor
			if(num > THRESHOLD) {   // operacao ficticia aqui...

				// insere amostra num buffer para emissor empacotar...
				_unknown_samples[out_pos]=num;

				// printf("classifier@%s (%d) repassou %d\n",_proc_name,_rank,_unknown_samples[out_pos]); fflush(stdout);
				printf("classifier@%s (%d) repassou %d\n",_proc_name,_rank,num); fflush(stdout);

				out_pos = (out_pos+1) % SIZE;
			
				// acorda thread de envio (semáforo)
				sem_post(&_sender_sem);
			}
		}
	} while(1);

	pthread_exit(NULL);
}

// monta lista e, ao atingir o threshold, envia mensagem com lista de amostras desconhecidas ao no detector
void * 
u_sender(void *arg)
{
	int out_pos=0;
	int ind=0;
	int unknown=0;
	
	do {
		// espera threshold (semaforo)
		sem_wait(&_sender_sem);

		// insere amostra na lista de unknowns

		// int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype,
  		//              void *outbuf, int outsize, int *position, MPI_Comm comm);
		MPI_Pack(&_unknown_samples[out_pos], 1, MPI_INT, _snd_buff, _buflen, &ind, MPI_COMM_WORLD);

		printf("u_sender@%s (%d) empacotou %d (%d). New size: %d\n",_proc_name,_rank,
		        _unknown_samples[out_pos], out_pos, ind); fflush(stdout);

		out_pos = (out_pos+1) % SIZE;

		unknown++;

		// se atingiu threshold, 
		if (unknown >= THRESHOLD) {
			// envia amostras: mensagem bloqueante MPI

			printf("u_sender@%s (%d): enviando unknowns: ", _proc_name,_rank); fflush(stdout);
			for(int i=0; i< unknown; i++) 
				printf("%d: (%d)  ",i,(int) _snd_buff[i]);
			printf("\n"); fflush(stdout);

			// int MPI_Send(void *buf, int count, MPI_Datatype dtype, int dest, int tag, MPI_Comm comm)
			// MPI_Send(_snd_buff,unknown,MPI_INT,0,UNK_TAG,MPI_COMM_WORLD);

			// int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
			MPI_Bsend( _snd_buff, ind, MPI_PACKED, 0, UNK_TAG, MPI_COMM_WORLD);
			// ou
			// MPI_Bsend( _snd_buff, unknown, MPI_INT, 0, UNK_TAG, MPI_COMM_WORLD);

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
		// Espera conjunto de novos grupos / modelos: mensagem bloqueante MPI, divulgada em broadcast
		
		// Recebe mensagem em broadcast. Emissor eh noh de rank 0
		// int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
		MPI_Bcast(&_model_buff, 1, MPI_INT, 0, MPI_COMM_WORLD);
		// Se a propagacao do novo modelo nao for por broadcast, usar um MPI_Recv aqui...
	
		// printf("m_receiver@%s, rank %d, recebeu broadcast\n", _proc_name, _rank); fflush(stdout);

		// desempacota dados
		// MPI_Unpac( ... _model_buff, ...);

		// cria nova lista de modelos

		// bloqueia acesso `a lista pelo classificador
		// troca lista de modelos 
		// libera acesso `a nova lista pelo classificador

	} while(1);

	pthread_exit(NULL);
}

void *
sampler(void *arg)
{
	int i, ind, num;
	int dest = 1;

	do {
		ind = 0;

		for(i=0; i < SAMPLES; i++ ) {

			// ler informacoes do arquivo do dataset: enviar 1 a 1 ou em blocos?
			// operação fictícia aqui
			usleep(50000);
			num=rand()%10;

			// int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype,
  			//              void *outbuf, int outsize, int *position, MPI_Comm comm);
			MPI_Pack(&num, 1, MPI_INT, _snd_buff, _buflen, &ind, MPI_COMM_WORLD);

			// printf("sampler@%s (%d) empacotou %d\n",_proc_name,_rank,num); fflush(stdout);
		}
		// envia "amostras" empacotadas...

		// int MPI_Send(void *buf, int count, MPI_Datatype dtype, int dest, int tag, MPI_Comm comm)
		MPI_Send(_snd_buff, SAMPLES, MPI_INT, dest, SAMP_TAG, MPI_COMM_WORLD);

		// printf("sampler@%s (%d) enviou para no %d\n",_proc_name,_rank,dest); fflush(stdout);

		// dest vai de 1 a _numprocs-1		
		dest = (dest+1) % _numprocs;
		if(!dest) dest=1;

	} while (1);

	pthread_exit(NULL);
}

void *
detector(void *arg)
{
	MPI_Status status;
	int result, count, num, ind, out_pos;

	// fica num loop `a espera de unknonws, monta grupos e propaga a todos os classificadores
	do {
		// printf("detector@%s (%d) esperando dados...\n",_proc_name,_rank);

		// rank 0 recebe dos demais (MPI_Comm_size -1)
		// Uso de MPI_ANY_SOURCE e MPI_ANY_TAG: não se sabe a ordem de envio

		// int MPI_Recv(void* buf, int count, MPI_Datatype datatype,
		//              int source, int tag, MPI_Comm comm, MPI_Status *status);
		MPI_Recv( _rcv_buff, THRESHOLD, MPI_INT, MPI_ANY_SOURCE, UNK_TAG, MPI_COMM_WORLD,&status);

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

		printf("detector@%s (%d) recebeu %d INTs de %d: ",_proc_name, _rank, count, status.MPI_SOURCE); fflush(stdout);
		for(int i=0; i < count; i++) printf("%d: (%d)  ",i,(int) _rcv_buff[i]);
		// for(int i=0, out_pos=0; i < count; i++) {
			// MPI_Unpack(_rcv_buff, _buflen, &out_pos, &num, 1, MPI_INT, MPI_COMM_WORLD);
			// printf("%d: (%d)  ",i,num);
		// }
		printf("\n"); fflush(stdout);

		// Detecta novidades e monta nova lista de grupos: modelo...

		// Por ora, modelo eh soh um numero... neste caso, simulado, com 1 int apenas...
		num = rand()%10;

		// empacota informacoes do modelo para envio
		ind = 0;
		// int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype,
  		//              void *outbuf, int outsize, int *position, MPI_Comm comm);
		MPI_Pack(&num, 1, MPI_INT, _model_buff, _buflen, &ind, MPI_COMM_WORLD);
		
		// Se os grupos nao forem globais, envia apenas para o no emissor (status.MPI_SOURCE da msg recebida)...
		// Senao, propaga em broadcast, como abaixo...
	
		// propaga lista a todos os nós: nó 0 é emissor, demais nós recebem
		MPI_Bcast(_model_buff, 1, MPI_INT, 0, MPI_COMM_WORLD);

	} while (1);

	pthread_exit(NULL);
} 


int
main (int argc, char *argv[])
{
	int result, count, i, tag;
	char err_msg[ERR_MSG_LEN];
	int namelen;
	int provided;

	// identificadores das threads
	pthread_t class_t, send_t, rec_t, det_t, samp_t;

	// srand(time(NULL));
	srand(getpid());

	// inicializa MPI. Fazer antes de criar threads, que vao usar mecanismos de comunicacao
	// result = MPI_Init(&argc,&argv);
	// int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
	result = MPI_Init_thread(&argc,&argv,MPI_THREAD_MULTIPLE, &provided);

	if (result != MPI_SUCCESS || provided != MPI_THREAD_MULTIPLE) {
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

	// Ambos os tipos de no, rank 0 e demais, fazem envio e recebimento

	// Aloca buffer para recebimento. rank 0 recebe unknowns; rank > 0 recebem amostras
	// _rcv_buff = (char *)malloc(_buflen);
	_rcv_buff = (int *)malloc(_buflen);

	// Aloca buffer para envio / recebimento do modelo. rank 0 usa para propagar; ranks > 0 usam para receber
	// _model_buff = (char *)malloc(_buflen);
	_model_buff = (int *)malloc(_buflen);

	// Aloca buffer para transmissao. rank 0 envia amostras; rank > 0 envia unknowns
	// _snd_buff = (char *)malloc(_buflen);
	_snd_buff = (int *)malloc(_buflen);

	// associa o buffer para envio
	MPI_Buffer_attach(_snd_buff, _buflen);


	// Verifica se é o processo 0 de rank 0 do MP: detector de novidades
	if (_rank == 0) {

		// criar 2 threads aqui: 1 para ler do arquivo e distribuir circularmente (?) as amostras
		// e outra para deteccao de novidades: receber desconhecidos, processar grupos e distribuir modelos.
		// Nos demais ranks, thread classifier recebe amostras via MPI_Recv, ao inves de ler arquivo local...

		result = pthread_create(&samp_t, NULL, sampler, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		} 
		result = pthread_create(&det_t, NULL, detector, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		} 

		// espera threads acabarem...

		result = pthread_join(det_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		result = pthread_join(samp_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		
	} else { // processos (nos) classificadores de amostra 

		// Cria threads
		result = pthread_create(&class_t, NULL, classifier, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		} 
		result = pthread_create(&send_t, NULL, u_sender, (void *)NULL); if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		}
		result = pthread_create(&rec_t, NULL, m_receiver, (void *)NULL);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Falha da criacao de thread: %s\n",err_msg);

			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		// espera threads terminarem

		result = pthread_join(class_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		result = pthread_join(send_t, (void **)&result);
		if (result) {
			strerror_r(result,err_msg,ERR_MSG_LEN);
			printf("Erro em pthread_join: %s\n",err_msg);
		}
		result = pthread_join(rec_t, (void **)&result);
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




