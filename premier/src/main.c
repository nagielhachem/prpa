#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


#define SHMMAX_SYS_FILE "/nbThread/sys/kernel/shmmax"
#define DEFAULT_SHMMAX 800000
#define PROC 4

int T, nbThread;

typedef struct {
	int thread_id;
	int offset;
	int T;
	unsigned long long min;
	int *ptrSeg;
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

int estPremier(unsigned long long num) {
	unsigned long long i;
	for (i = 2; i <= sqrt(num); ++i) {
		if (num % i == 0)
			return 0;
	}
	return 1;
}

void* thread_job(void *arg){

	segment *seg = (segment *) arg;
	//printf("thread %d, T: %d\n", seg->thread_id, seg->T);
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
	for (int i = 0; i < nbThread; ++i) {
		printf("%d: %d\n", i, occurance[i]);
	}
}

void computePremier(unsigned long long min, unsigned long long max,
		int sizeSegment, int nbInts, int *ptrSeg){

	int* premier = ptrSeg + 1;
	int* occurance = premier + nbInts;

	//initialisation des variables a 0:
	for (int i = 0; i < sizeSegment; ++i)
		ptrSeg[i] = 0;

	pthread_t pthread_id[nbThread];

	int size = max - min + 1;
	int offset = 0;
	int mod = size % nbThread;

	//creation de nbThread nbThreadessus:
	for (int i = 0; i < nbThread - 1; ++i) {
		segment *seg = malloc(sizeof (segment));
		seg->min = min;
		seg->ptrSeg = ptrSeg;
		seg->premier = premier;
		seg->occurance = occurance;
		seg->thread_id = i;
		seg->offset = offset;
		seg->T = size / nbThread + (mod-- <= 0 ? 0 : 1);
		offset += seg->T;
		pthread_create(&pthread_id[i], NULL, thread_job, (void*) seg);
	}

	segment *seg = malloc(sizeof (segment));
	seg->min = min;
	seg->ptrSeg = ptrSeg;
	seg->premier = premier;
	seg->occurance = occurance;
	seg->thread_id = nbThread - 1;
	seg->offset = offset;
	seg->T = size / nbThread;

	if (nbThread == 1)
		seg->T = max - min;
	pthread_create(&pthread_id[nbThread - 1], NULL, thread_job, (void*) seg);

	for (int i = 0; i < nbThread; ++i) {
		pthread_join(pthread_id[i], NULL);
	}

	affichePremier(premier, min, max);
	afficheOccurance(occurance);

}

void parseCmd(int argc, char **argv,
		unsigned long long *min, unsigned long long *max, int *nbThread) {

	if (argc != 4) {
		printf("Usage: %s min max nbThreads.\n", argv[0]);
		exit(1);
	}

	*min = strtoul(argv[1], NULL, 10);
	*max = strtoul(argv[2], NULL, 10);
	*nbThread = atoi(argv[3]);
}

int main(int argc, char* argv[]){

	unsigned long long min, max;
	parseCmd(argc, argv, &min, &max, &nbThread);

	//nombre d'entiers dans le segment:
	int size = max - min + 1;
	int n = 1 + size + nbThread;
	int sizeSegment = 0;
	unsigned int shmmax = getSHMMAX();
	if (n * sizeof(int) > shmmax)
		sizeSegment = shmmax / sizeof(int);
	else
		sizeSegment = n;

	int nbInts = sizeSegment - 1 - nbThread;
	int nbSegments = n / sizeSegment + (sizeSegment == n ? 0 : 1);
	int nbProc = nbSegments < PROC ? nbSegments : PROC;

	// définition du début et de la fin des segments de chaque nbThread
	unsigned long long starts[nbProc];
	unsigned long long ends[nbProc];
	int memid[nbProc];
	int *ptrSeg[nbProc];

	starts[0] = min;
	ends[0] = min + (nbSegments / nbProc + (nbSegments % nbProc > 0 ? 1 : 0)) * nbInts;
	for (int i = 1; i < nbProc; i++){
		starts[i] = ends[i - 1] + 1;
		ends[i] = starts[i] + (nbSegments / nbProc 
				+ (i < nbSegments % nbProc ? 1 : 0)) * nbInts;
	}
	ends[nbProc - 1] = max;

	for (int i = 0; i < nbProc; ++i) {
		//creation du segment et attachement:
		memid[i] = shmget(IPC_PRIVATE, sizeSegment * sizeof (int), IPC_CREAT | 0666);
		if (memid[i] == -1) {
			for (int j = 0; j < i; ++j) 
				shmctl(memid[i], IPC_RMID, NULL);
			perror("shmget");
			return 1;
		}
		ptrSeg[i] = shmat(memid[i], 0, 0);
	}

	for (int i = 0; i < nbProc - 1; i++) {
		if (fork() == 0) {
			if (nbSegments <= nbProc)
				computePremier(min + i * nbInts, min + (i + 1) * nbInts,
						sizeSegment, nbInts, ptrSeg[i]);
			else {
				for (int j = 0; j < nbSegments / nbProc; j++)
					computePremier(starts[i] + j * nbInts, starts[i] + (j + 1) * nbInts, 
							sizeSegment, nbInts, ptrSeg[i]);
				if (i < nbSegments % nbProc)
					computePremier(starts[i] + nbSegments / nbProc * nbInts, ends[i],
							sizeSegment, nbInts, ptrSeg[i]);
			}
			exit(0);
		}
	}
	if (fork() == 0){
		if (nbSegments <= nbProc)
			computePremier(min + (nbSegments - 1) * nbInts,
					max, sizeSegment, nbInts,	ptrSeg[nbProc - 1]);
		else{
			for (int j = 0; j < nbSegments / nbProc - 1; j++)
				computePremier(starts[nbProc - 1] + j * nbInts, 
						starts[nbProc - 1] + (j + 1) * nbInts, sizeSegment, 
						nbInts, ptrSeg[nbProc - 1]);
			computePremier(starts[nbProc - 1] + (nbSegments / nbProc - 1) * nbInts,
					ends[nbProc - 1], sizeSegment, nbInts, ptrSeg[nbProc - 1]);
		}
		exit(0);
	}
	
	//pere attend tous ses fils:
	while (wait(NULL) != -1);
	//detachement:
	for (int i = 0; i < nbProc; ++i) 
		shmctl(memid[i], IPC_RMID, NULL);
}

