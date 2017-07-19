//Nagi El Hachem 130416	
//Marc Nohra 121311
/*

1- Il faut utiliser une mutexe pour assurer l'acces par un seul processus a la variable partager p en lecture et ecriture.

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

int Max, T, proc;
int memid, semid;

//segment partage
int* ptr_seg;
int *occurance;
int *premier;

int P(int numsem){
	struct sembuf op;
	op.sem_op=-1;
	op.sem_num=numsem;
	op.sem_flg=0;
	return semop(semid,&op,1);
}



int V(int numsem){
	struct sembuf op;
	op.sem_op=1;
	op.sem_num=numsem;
	op.sem_flg=0;
	return semop(semid,&op,1);
}

int estPremier(int num){
	int i;
	for(i=2; i<num; ++i){
		if(num%i==0)
			return 0;
	}
	return 1;
}

void fils(int num){

	int debut, fin, i;

	while(1){
		//le fils choisit l'interval:
		P(0);
		//section critique:
		if(*ptr_seg >= Max){
			V(0);
			exit(0); //travail fini.
		}
		//initialisation de debut et fin.
		debut = *ptr_seg;
		if((*ptr_seg+T-1)<=Max){
			*ptr_seg = *ptr_seg + T; //T est inferieur a l'interval restant.
			fin = debut + T;
		}
		else {
			*ptr_seg = Max; //T est superieur a l'interval restant.
			fin = Max;
		}
		//fin section critique:
		V(0);
		
		//le fils cherche les nombres premiers:
		for(i = debut; i<fin; ++i){
			if(estPremier(i)==1){
				premier[i] = 1;
				++occurance[num];
			}
		}
	}
}



int main(int argc, char* argv[]){
	if(argc != 4){
		printf("Il faut entrer les valeurs respectives de Max, T et proc.\n");
		exit(1);
	}

	int i,n;
	pid_t pid;
	Max = atoi(argv[1]);
	T = atoi(argv[2]);
	proc = atoi(argv[3]);

	//nombre d'entiers dans le segment:
	n = 1 + Max + proc;

	//creation du segment et attachement:
	memid = shmget(IPC_PRIVATE, n*sizeof(int), IPC_CREAT | 0666);
	ptr_seg = shmat(memid, 0, 0);

	//initialisation des variables a 0:
	ptr_seg[0] = 1;
	for(i=1; i<n; ++i)
		ptr_seg[i]=0;
		
	//initialisation des pointeurs
	premier = ptr_seg + 1;
	occurance = premier + Max;

	//creation de la semaphore:
	semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if(semid ==-1){
		perror("semget");
		exit(1);
	}

	//initialisation de la semaphore:
	semctl(semid,0,SETVAL,1);

	//creation de proc processus:
	for(i=0; i<proc; ++i){
		pid=fork();
		if(pid==0)//fils travaille:
			fils(i);
	}

	//pere attend tous ses fils:	
	while(wait(NULL)!=-1);
		
	//pere affiche:
	printf("nombres premiers: \n");
	for(i=0; i<Max; ++i){
		if(premier[i]==1)
			printf("%d ", i);
	}
	
	printf("\nNombre d'occurance: \n");
	for(i=0; i<proc; ++i){
		printf("%d: %d\n", i, occurance[i]);
	}
	
	//detachement:
	shmdt(ptr_seg);
	semctl(semid,0,IPC_RMID,0);
	
}

