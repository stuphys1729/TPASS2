// TPAssignment2.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define N 729
#define reps 1000
#include <omp.h> 

double a[N][N], b[N][N], c[N];
int jmax[N];


void init1(void);
void init2(void);
void runloop(int,int);
void loop1chunk(int, int);
void loop2chunk(int, int);
void valid1(void);
void valid2(void);

struct Work {
	int curr_it;
	int last_it;
};


int main(int argc, char *argv[]) {

	double start1, start2, end1, end2;
	float taken1, taken2;
	int r, num_threads;

	num_threads = omp_get_max_threads();

	init1();

	start1 = omp_get_wtime();

	for (r = 0; r<reps; r++) {
	    runloop(1, num_threads);
	}

	end1 = omp_get_wtime();

	valid1();

	taken1 = (float)(end1 - start1);

	printf("Total time for %d reps of loop 1 = %f\n", reps, taken1);


	init2();

	start2 = omp_get_wtime();

	for (r = 0; r<reps; r++) {
	  runloop(2, num_threads);
	}

	end2 = omp_get_wtime();

	valid2();

	taken2 = (float)(end2 - start2);

	printf("Total time for %d reps of loop 2 = %f\n", reps, taken2);

	FILE *f = fopen("data.txt", "a");
	if (f == NULL) {
	  printf("Error opening data file\n");
	  exit(1);
	}

	fprintf(f, "%d\t%f\t%f\n", num_threads, taken1, taken2);
	fclose(f);

}

void init1(void) {
	int i, j;

	for (i = 0; i<N; i++) {
		for (j = 0; j<N; j++) {
			a[i][j] = 0.0;
			b[i][j] = 3.142*(i + j);
		}
	}

}

void init2(void) {
	int i, j, expr;

	for (i = 0; i<N; i++) {
		expr = i % (3 * (i / 30) + 1);
		if (expr == 0) {
			jmax[i] = N;
		}
		else {
			jmax[i] = 1;
		}
		c[i] = 0.0;
	}

	for (i = 0; i<N; i++) {
		for (j = 0; j<N; j++) {
			b[i][j] = (double)(i*j + 1) / (double)(N*N);
		}
	}

}


void runloop(int loopid, int size) {

	// Initialises the structure for monitoring remaining work
	struct Work *work = (struct Work*)malloc(size * sizeof(struct Work));
	// A global flag for all work being finished
	int all_finished = 0;
	// A set of locks for each thread's progress
	omp_lock_t* prog_locks = (omp_lock_t*)malloc(size * sizeof(omp_lock_t));

#pragma omp parallel default(none) shared(loopid, work, all_finished, prog_locks) 
	{
		int lo, hi, remaining, load;
		int max_load;
		int max_thread;

		int my_id = omp_get_thread_num();
		int nthreads = omp_get_num_threads();
		int ipt = ceil((double)N / (double)nthreads);
		work[my_id].curr_it = my_id*ipt;
		int temp = (my_id + 1)*ipt;
		if (temp > N) work[my_id].last_it = N;
		else work[my_id].last_it = temp;

		omp_init_lock(&prog_locks[my_id]);

#pragma omp barrier

		// Print out the allocated local sets for debugging
		/*
#pragma omp single
		{
			for (int i = 0; i < nthreads; i++) {
				printf("Thread %d has been given domain %d to %d\n", i, work[i].curr_it, work[i].last_it);
			}
		}
		*/
		int finished = 0;
		while (!all_finished) {

			if (work[my_id].curr_it >= work[my_id].last_it) {
				// This thread has completed its local set of iterations
				//printf("Thread %d has finished its local set\n", my_id);

				// Acquire the locks of all threads so we can choose who to help
				for (int i = 0; i < nthreads; i++) {
					omp_set_lock(&prog_locks[i]);
				}

				max_load = 0;
				for (int i = 0; i < nthreads; i++) {
					load = work[i].last_it - work[i].curr_it;
					if (load > max_load) {
						max_load = load;
						max_thread = i;
					}
				}

				if (max_load) {
					lo = work[max_thread].curr_it;
					hi = lo + ceil((double)max_load / (double)nthreads);
					work[max_thread].curr_it = hi;
				}
				else all_finished = 1;

				// release each lock again
				for (int i = 0; i < nthreads; i++) {
					omp_unset_lock(&prog_locks[i]);
				}
			}
			else {
				// Continue executing local set of iterations
				// acquire this thread's lock
				omp_set_lock(&prog_locks[my_id]);
				lo = work[my_id].curr_it;

				remaining = work[my_id].last_it - lo;
				hi = lo + ceil((double)remaining / (double)nthreads);

				work[my_id].curr_it = hi;
				
				// release this thread's lock
				omp_unset_lock(&prog_locks[my_id]);
			}
				

			if (all_finished) break;

			//printf("Thread %d is performing iterations %d to %d\n", my_id, lo, hi);

			switch (loopid) {
			case 1: loop1chunk(lo, hi); break;
			case 2: loop2chunk(lo, hi); break;
			}
		}
	 
		#pragma omp barrier
		omp_destroy_lock(&prog_locks[my_id]);
	}
	free(work);
	free(prog_locks);
}

void loop1chunk(int lo, int hi) {
	int i, j;

	for (i = lo; i<hi; i++) {
		for (j = N - 1; j>i; j--) {
			a[i][j] += cos(b[i][j]);
		}
	}

}



void loop2chunk(int lo, int hi) {
	int i, j, k;
	double rN2;

	rN2 = 1.0 / (double)(N*N);

	for (i = lo; i<hi; i++) {
		for (j = 0; j < jmax[i]; j++) {
			for (k = 0; k<j; k++) {
				c[i] += (k + 1) * log(b[i][j]) * rN2;
			}
		}
	}

}

void valid1(void) {
	int i, j;
	double suma;

	suma = 0.0;
	for (i = 0; i<N; i++) {
		for (j = 0; j<N; j++) {
			suma += a[i][j];
		}
	}
	printf("Loop 1 check: Sum of a is %lf\n", suma);

}


void valid2(void) {
	int i;
	double sumc;

	sumc = 0.0;
	for (i = 0; i<N; i++) {
		sumc += c[i];
	}
	printf("Loop 2 check: Sum of c is %f\n", sumc);
}

