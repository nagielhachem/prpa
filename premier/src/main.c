#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

int min, max, T, proc;
int memid, semid;

//segment partage
int *ptr_seg;
int *occurance;
int *premier;

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

int estPremier(int num) {
	for (int i = 2; i < sqrt(num); ++i) {
		if (num % i == 0)
			return 0;
	}
	return 1;
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
			if (estPremier(i + min)) {
				premier[i] = 1;
				++occurance[num];
			}
		}
	}
}



int main(int argc, char* argv[]){
	if (argc != 5) {
		printf("Usage: %s min max T nb_process.\n", argv[0]);
		exit(1);
	}

	int n;
	pid_t pid;
	min = atoi(argv[1]);
	max = atoi(argv[2]);
	T = atoi(argv[3]);
	proc = atoi(argv[4]);

	//nombre d'entiers dans le segment:
	n = 1 + (max - min + 1) + proc;

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
	for (int i = 0; i < max - min + 1; ++i) {
 		if (premier[i] == 1)
			printf("%d ", min + i);
	}

	printf("\nNombre d'occurance: \n");
	for (int i = 0; i < proc; ++i) {
		printf("%d: %d\n", i, occurance[i]);
	}

	//detachement:
	shmctl(memid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID, 0);

}

