#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


#define SHMMAX_SYS_FILE "/proc/sys/kernel/shmmax"
#define DEFAULT_SHMMAX 2000000

int T, proc;

typedef struct {
	int thread_id;
	int offset;
	int T;
	unsigned long long min;
	int *ptr_seg;
	int *premier;
	int *occurance;
} segment;

unsigned int getSHMMAX(){
	unsigned int shmmax;
	FILE *f = fopen(SHMMAX_SYS_FILE, "r");

	if (!f)
		return DEFAULT_SHMMAX;

	if (fscanf(f, "%u", &shmmax) != 1) {
		fclose(f);
		return DEFAULT_SHMMAX;
	}

	fclose(f);
	return shmmax;
}

int P(int numsem, int semid) {
	struct sembuf op;
	op.sem_op = -1;
	op.sem_num = numsem;
	op.sem_flg = 0;
	return semop(semid, &op, 1);
}

int V(int numsem, int semid) {
	struct sembuf op;
	op.sem_op = 1;
	op.sem_num = numsem;
	op.sem_flg = 0;
	return semop(semid, &op, 1);
}

int estPremier(unsigned long long num) {
	unsigned long long i;
	for (i = 2; i <= sqrt(num); ++i) {
		if (num % i == 0)
			return 0;
	}
	return 1;
}

void fils(int num, int semid,	unsigned long long min, unsigned long long max,
		int* ptr_seg, int* premier, int* occurance){
	
	int debut, fin;
	int size = max - min + 1;

	while (1) {
		//le fils choisit l'interval:
		P(0, semid);
		//section critique:
		if (ptr_seg [0]>= size) {
			V(0, semid);
			pthread_exit((void*) 0); //travail fini.
		}
		//initialisation de debut et fin.
		debut = ptr_seg[0];
		if ((debut + T) < size) {
			ptr_seg[0] = debut + T; //T est inferieur a l'interval restant.
			fin = debut + T;
		}
		else {
			ptr_seg[0] = size; //T est superieur a l'interval restant.
			fin = size;
		}
		//fin section critique:
		V(0, semid);

		//le fils cherche les nombres premiers:
		for (int i = debut; i < fin; ++i) {
			if (estPremier(i + min)) {
				premier[i] = 1;
				occurance[num]++;
			}
		}
	}
}

void* thread_job(void *arg){

	segment *seg = (segment *) arg;
	printf("thread %d, T: %d\n", seg->thread_id, seg->T);
	//le fils cherche les nombres premiers:
	for (int i = 0; i < seg->T; ++i) {
		if (estPremier(i + seg->min + seg->offset)) {
			seg->premier[i + seg->offset] = 1;
			seg->occurance[seg->thread_id]++;
		}
	}
	free(seg);
	return NULL;
}

void affichePremier(int* premier, unsigned long long min, unsigned long long max){
	printf("nombres premiers: \n");
	int size = max - min + 1;
	for (int i = 0; i < size; ++i) {
		if (premier[i] == 1)
			printf("%llu ", min + i);
	}
}

void afficheOccurance(int* occurance){
	printf("\nNombre d'occurance: \n");
	for (int i = 0; i < proc; ++i) {
		printf("%d: %d\n", i, occurance[i]);
	}
}


void computePremier(unsigned long long min, unsigned long long max,
		int sizeSegment, int nbInts){
	
	//creation du segment et attachement:
	int memid = shmget(IPC_PRIVATE, sizeSegment * sizeof (int), IPC_CREAT | 0666);
	if (memid == -1) {
		perror("shmget");
		exit(1);
	}

	//initialisation des pointeurs
	int* ptr_seg = shmat(memid, 0, 0);
	int* premier = ptr_seg + 1;
	int* occurance = premier + nbInts;

	//initialisation des variables a 0:
	for (int i = 0; i < sizeSegment; ++i)
		ptr_seg[i] = 0;

	//creation de la semaphore:
	int semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (semid == -1) {
		perror("semget");
		exit(1);
	}

	//initialisation de la semaphore:
	semctl(semid, 0, SETVAL, 1);

	pthread_t pthread_id[proc];

	int size = max - min + 1;
	int offset = 0;
	int mod = size % proc;
	//creation de proc processus:
	for (int i = 0; i < proc - 1; ++i) {
		segment *seg = malloc(sizeof (segment));
		seg->min = min;
		seg->ptr_seg = ptr_seg;
		seg->premier = premier;
		seg->occurance = occurance;
		seg->thread_id = i;
		seg->offset = offset;
		seg->T = size / proc + (mod-- <= 0 ? 0 : 1);
		offset += seg->T;
		pthread_create(&pthread_id[i], NULL, thread_job, (void*) seg);
	}

	segment *seg = malloc(sizeof (segment));
	seg->min = min;
	seg->ptr_seg = ptr_seg;
	seg->premier = premier;
	seg->occurance = occurance;
	seg->thread_id = proc - 1;
	seg->offset = offset;
	seg->T = size / proc;
	if (proc == 1)
		seg->T = max - min;
	pthread_create(&pthread_id[proc - 1], NULL, thread_job, (void*) seg);

	for (int i = 0; i < proc; ++i) {
		pthread_join(pthread_id[i], NULL);
	}

//	//creation de proc processus:
//	for (int i = 0; i < proc; ++i) {
//		pid_t pid = fork();
//		if (pid == 0) //fils travaille:
//			fils(i, semid, min, max, ptr_seg, premier, occurance);
//	}

	//pere attend tous ses fils:	
//	while (wait(NULL) != -1);

	affichePremier(premier, min, max);
	afficheOccurance(occurance);

	//detachement:
	semctl(semid, 0, IPC_RMID, 0);
	shmctl(memid, IPC_RMID, NULL);
	exit(0);
}

void parseCmd(int argc, char **argv,
		unsigned long long *min, unsigned long long *max, int *proc) {

	if (argc != 4) {
		printf("Usage: %s min max nb_threads.\n", argv[0]);
		exit(1);
	}

	*min = strtoul(argv[1], NULL, 10);
	*max = strtoul(argv[2], NULL, 10);
	*proc = atoi(argv[3]);
}

int main(int argc, char* argv[]){
	
	unsigned long long min, max;
	parseCmd(argc, argv, &min, &max, &proc);

	//nombre d'entiers dans le segment:
	int size = max - min + 1;
	int n = 1 + size + proc;
	int sizeSegment = 0;
	unsigned int shmmax = getSHMMAX();
	if (n * sizeof(int) > shmmax)
		sizeSegment = shmmax / sizeof(int);
	else
		sizeSegment = n;

	int nbInts = sizeSegment - 1 - proc;
	int nbSegments = n / sizeSegment + (sizeSegment == n ? 0 : 1);
	
	for (int i = 0; i < nbSegments - 1; i++) {
		pid_t pid = fork();
		if (pid == 0)
			computePremier(min + i * nbInts, min + (i + 1) * nbInts,
					sizeSegment, nbInts);
	}
	if (fork() == 0)
		computePremier(min + (nbSegments - 1) * nbInts, max, sizeSegment, nbInts);
	//pere attend tous ses fils:	
	while (wait(NULL) != -1);
}

