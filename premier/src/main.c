#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


#define SHMMAX_SYS_FILE "/proc/sys/kernel/shmmax"
#define DEFAULT_SHMMAX 2000000

int T, proc;

//segment partage
int *ptr_seg = NULL;
int *occurance = NULL;
int *premier = NULL;


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

void fils(int num, int semid, unsigned long long min, unsigned long long max){
	int debut, fin;
	int size = max - min + 1;

	while (1) {
		//le fils choisit l'interval:
		P(0, semid);
		//section critique:
		if (*ptr_seg >= size) {
			V(0, semid);
			exit(0); //travail fini.
		}
		//initialisation de debut et fin.
		debut = *ptr_seg;
		if ((debut + T) < size) {
			*ptr_seg = debut + T; //T est inferieur a l'interval restant.
			fin = debut + T;
		}
		else {
			*ptr_seg = size; //T est superieur a l'interval restant.
			fin = size;
		}
		//fin section critique:
		V(0, semid);

		//le fils cherche les nombres premiers:
		for (int i = debut; i < fin; ++i) {
			if (estPremier(i + min)) {
				premier[i] = 1;
				++occurance[num];
			}
		}
	}
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
		int sizeSegment){
	pid_t pid;

	//creation de la semaphore:
	int semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (semid == -1) {
		perror("semget");
		exit(1);
	}

	//initialisation de la semaphore:
	semctl(semid, 0, SETVAL, 1);

	//initialisation des variables a 0:
	for (int i = 0; i < sizeSegment; ++i)
		ptr_seg[i] = 0;

	//creation de proc processus:
	for (int i = 0; i < proc; ++i) {
		pid = fork();
		if (pid == 0) //fils travaille:
			fils(i, semid, min, max);
	}

	//pere attend tous ses fils:	
	while (wait(NULL) != -1);

	semctl(semid, 0, IPC_RMID, 0);
}



int main(int argc, char* argv[]){
	if (argc != 5) {
		printf("Usage: %s min max T nb_process.\n", argv[0]);
		exit(1);
	}

	unsigned long long min, max;
	min = strtoul(argv[1], NULL, 10);
	printf("min: %llu\n", min);
	max = strtoul(argv[2], NULL, 10);
	printf("max: %llu\n", max);
	T = atoi(argv[3]);
	proc = atoi(argv[4]);
	int memid;

	//nombre d'entiers dans le segment:
	int size = max - min + 1;
	int n = 1 + size + proc;
	int sizeSegment = 0;
	unsigned int shmmax = getSHMMAX();
	if (n * sizeof(int) > shmmax)
		sizeSegment = shmmax / sizeof(int);
	else
		sizeSegment = n;

	printf("n: %d\n", n);
	printf("sizeSegment: %d\n", sizeSegment);

	//creation du segment et attachement:
	memid = shmget(IPC_PRIVATE, sizeSegment * sizeof(int), IPC_CREAT | 0666);

	if (memid == -1) {
		perror("shmget");
		exit(1);
	}

	ptr_seg = shmat(memid, 0, 0);
	//initialisation des pointeurs


//	int nbSegments = n * sizeof(int) / shmmax + 1;
	int nbInts = sizeSegment - 1 - proc;
	int nbSegments = n / sizeSegment + (sizeSegment == n ? 0 : 1);
	printf("nbSegments: %d, nbInts: %d\n", nbSegments, nbInts);
	premier = ptr_seg + 1;
	occurance = premier + nbInts;

	for (int i = 0; i < nbSegments - 1; i++){
		unsigned long long m1 = min + i * nbInts;
		unsigned long long m2 = min + (i + 1) * nbInts;
		printf("[%llu, %llu[\n", m1, m2);
		computePremier(m1, m2, sizeSegment);
		affichePremier(premier, m1, m2);
		afficheOccurance(occurance);
	}

	computePremier(min + (nbSegments - 1) * nbInts, max, sizeSegment);
	affichePremier(premier, min + (nbSegments-1) * nbInts, max);

	afficheOccurance(occurance);
	
	//detachement:
	shmctl(memid, IPC_RMID, NULL);

}

