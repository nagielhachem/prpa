#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/msg.h>

typedef struct {
	long type; 
	double surface;
} resultat;

double f(double x){
	return 4 / (1 + x * x);
}

double pi(int n){
	double pi = 0;
	for (int i = 1; i <= n; ++i) {
		pi += f((i - 0.5) / n);
	}
	pi = pi / n;
	return pi;
}

double erreur(int n){
	double p = 4 * atan(1.0);
	return fabs((p - pi(n)));
}

int main(int argc, char* argv[]){
	if (argc != 3) {
		printf("Usage: %s nb_iterations nb_process\n", argv[0]);
		return 9;
	}
	int n, m;
	n = atoi(argv[1]);
	m = atoi(argv[2]);

	int msgid;
	resultat res;

	/* Création de la file de messages et récupération de l'identificateur interne */
	if ((msgid = msgget(IPC_PRIVATE ,IPC_CREAT | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	pid_t p;
	for(int i = 1; i <= m; ++i) {
		p = fork();
		if (p == -1) {
			perror("Error in fork.\n");
			exit(2);
		}
		if (p == 0) {
			//fils
			double pi = 0;
			for (int j = i; j <= n; j = j + m) {
				pi += f((j - 0.5) / n);
			}
			pi = pi / n;
			res.surface = pi;
			res.type = 1;

			if (msgsnd(msgid, &res, sizeof (double), 0) == -1) {
				perror("msgrcv");
				exit(1);
			}

			//le fils a fini son travail:
			exit(0);
		}
	}

	//pere
	double valeur = 0;
	for(int i = 0; i < m; ++i) {
		if (msgrcv(msgid, &res, sizeof(double) , 1L, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}
		valeur += res.surface;
	}
	printf("resulta: pi = %lg\n", valeur);
	double err = fabs((4 * atan(1.0) - valeur));
	printf("n = %d m = %d erreur: %lg\n", n, m, err);
	printf("n = %d erreur: %lg\n", n, erreur(n));

}

/*
	 n = 10000:
resulta: pi = 3.14159
n= 10000 m= 10 erreur: 8.33333e-10
n= 10000 erreur: 8.33341e-10

n = 100000:
resulta: pi = 3.14159
n= 100000 m= 10 erreur: 8.33333e-12
n= 100000 erreur: 8.36708e-12

n = 10000000:
resulta: pi = 3.14159
n= 10000000 m= 10 erreur: 8.88178e-16
n= 10000000 erreur: 6.4837e-14

n = 100000000:
resulta: pi = 3.14159
n= 100000000 m= 10 erreur: 4.88498e-14
n= 100000000 erreur: 4.41425e-13

avantage:
calcule beaucoup plus rapide car le travail est divise en m processus.
moins d'erreur car la multiplication de nombre tres petit genere des erreurs de calcule. Quand on divise le travail en m processus, il y a moins de multiplication, elles sont remplace par la somme qui genere moins d'erreur.
*/
