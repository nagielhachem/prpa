#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

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

void* estPremier(void* dom) {
	domaine* d = (domaine*) dom;
	unsigned long long i;
	
	for (i = d->min; i < d->max; ++i)
		if (d->number % i == 0)
			pthread_exit((void*) 1);
	pthread_exit((void*) 0);
}

int main(int argc, char** argv){
	if (argc < 3) {
		printf("Usage: %s nb_to_test nb_threads\n", argv[0]);
		return 1;
	}
	int proc = atoi(argv[2]);
	unsigned long long n = strtoul(argv[1], NULL, 10);
	unsigned long long sn = sqrt(n);
	unsigned long long N = sn / proc;

	if (N == 0 || proc == 1) {
		if (est_premier(n, sn))
			printf("premier\n");
		else printf("non premier\n");
		return 0;
	}

	pthread_t pthread_id[proc];

	domaine dom[proc];
	dom[0].number = n;
	dom[0].min = 2;
	dom[0].max = N;

	for (int i = 0; i < proc - 1; ++i) {
		pthread_create(&pthread_id[i], NULL, estPremier, (void*) &dom[0]);
		dom[i + 1].min = dom[i].max;
		dom[i + 1].max = (i + 1) * N;
	}
	dom[proc - 1].min = dom[proc - 2].max;
	dom[proc - 1].max = sn;
	pthread_create(&pthread_id[proc - 1], NULL, estPremier, (void*) &dom[proc - 1]);

	int premier = 0;
	for (int i = 0; i < proc; ++i) {
		int tmp;
		pthread_join(pthread_id[i], (void **) &tmp);
		premier += tmp;
		if (premier) {
			printf("Non premier\n");
			return 0;
		}
	}
	printf("Premier\n");
	return 1;
}
