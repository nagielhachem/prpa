#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


unsigned long long min, max;
int T, proc, threads;
int memid, semid;

//segment partage
int *ptr_seg = NULL;
int *occurance = NULL;
int *premier = NULL;

int P(int numsem) {
	struct sembuf op;
	op.sem_op = -1;
	op.sem_num = numsem;
	op.sem_flg = 0;
	return semop(semid, &op, 1);
}

int V(int numsem) {
	struct sembuf op;
	op.sem_op = 1;
	op.sem_num = numsem;
	op.sem_flg = 0;
	return semop(semid, &op, 1);
}

typedef struct {
	unsigned long long number;
	unsigned long long min;
	unsigned long long max;

} domaine;

int est_premier(unsigned long long n, unsigned long long sn) {
	for (unsigned long long i = 2; i < sn; ++i)
		if (n % i == 0)
			return 0;
	return 1;
}

void* est_premier_parallel(void* dom) {
	domaine* d = (domaine*) dom;
	unsigned long long i;

	for (i = d->min; i < d->max; ++i)
		if (d->number % i == 0)
			pthread_exit((void*) 1);
	pthread_exit((void*) 0);

}

void fils(int num){
	int debut, fin;
	int size = max - min + 1;

	while (1) {
		//le fils choisit l'interval:
		P(0);
		//section critique:
		if (*ptr_seg >= size) {
			V(0);
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
		V(0);

		//le fils cherche les nombres premiers:
		for (int i = debut; i < fin; ++i) {
			unsigned long long n = i + min;
			unsigned long long sn = sqrt(n);
			unsigned long long N = sn / threads;

			if (num < 100000000 || N == 0 || threads == 1) {
				if (est_premier(n, sn)) {
					premier[i] = 1;
					++occurance[num];
				}
			}
			else {
				pthread_t pthread_id[threads];

				domaine dom[threads];
				dom[0].number = n;
				dom[0].min = 2;
				dom[0].max = N;

				for (int j = 0; j < threads - 1; ++j) {
					pthread_create(&pthread_id[j], NULL, est_premier_parallel, (void*) &dom[0]);
					dom[j + 1].min = dom[j].max;
					dom[j + 1].max = (j + 1) * N;
				}
				dom[threads - 1].min = dom[threads - 2].max;
				dom[threads - 1].max = sn;
				pthread_create(&pthread_id[threads - 1], NULL, est_premier_parallel, (void*) &dom[threads - 1]);

				int non_premier = 0;
				for (int j = 0; j < threads; ++j) {
					int tmp;
					pthread_join(pthread_id[j], (void **) &tmp);
					non_premier += tmp;
					if (non_premier) {  
						for (int k = j + 1; k < threads; ++k)
							pthread_cancel(pthread_id[k]);
						break;
					}
					if (non_premier == 0) {
						premier[i] = 1;
						++occurance[num];
					}
				}
			}
		}
	}
}



int main(int argc, char* argv[]){
	if (argc != 6) {
		printf("Usage: %s min max T nb_process nb_threads\n", argv[0]);
		exit(1);
	}

	pid_t pid;
	min = strtoul(argv[1], NULL, 10);
	max = strtoul(argv[2], NULL, 10);
	T = atoi(argv[3]);
	proc = atoi(argv[4]);
	threads = atoi(argv[5]);

	//nombre d'entiers dans le segment:
	int n = 1 + (max - min + 1) + proc;

	//creation du segment et attachement:
	memid = shmget(IPC_PRIVATE, n * sizeof (int), IPC_CREAT | 0666);
	if (memid == -1) {
		perror("shmget");
		exit(1);
	}
	ptr_seg = shmat(memid, 0, 0);
	
	//initialisation des variables a 0:
	for (int i = 0; i < n; ++i)
		ptr_seg[i] = 0;

	//initialisation des pointeurs
	premier = ptr_seg + 1;
	occurance = premier + (max - min + 1);

	//creation de la semaphore:
	semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if (semid == -1) {
		perror("semget");
		exit(1);
	}

	//initialisation de la semaphore:
	semctl(semid, 0, SETVAL, 1);

	//creation de proc processus:
	for (int i = 0; i < proc; ++i) {
		pid = fork();
		if (pid == 0) //fils travaille:
			fils(i);
	}

	//pere attend tous ses fils:	
	while (wait(NULL) != -1);

	//pere affiche:
	printf("nombres premiers: \n");
	int size = max - min + 1;
	for (int i = 0; i < size; ++i) {
 		if (premier[i] == 1)
			printf("%llu ", min + i);
	}

	printf("\nNombre d'occurance: \n");
	for (int i = 0; i < proc; ++i) {
		printf("%d: %d\n", i, occurance[i]);
	}

	//detachement:
	shmctl(memid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID, 0);

}

