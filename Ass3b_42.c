#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <signal.h>
#include <time.h>

#include <unistd.h>

const int THREAD_READY = 0, THREAD_RUNNING = 1, THREAD_TERMINATED = 2;
/* Number of threads. */
int N, BFSZ, K, *thread_statuses = NULL, *ack_statuses = NULL,
  q_in, q_out, *ready_queue = NULL;

typedef struct {
  int tid;
  /* ...Other ags... */
} thread_arg_t ;

pthread_t *threads = NULL;
thread_arg_t *thread_args = NULL;

/* Cause SIGUSR1 to sleep. */
void handle_signals(int sig_no) {
  if( sig_no == SIGUSR1 )
    pause();
}

/* Start a new thread. */
void *start_worker(void * ptr) {
  thread_arg_t *arg = (thread_arg_t *) ptr;

  int a[K];
  srand(time(NULL) * arg->tid);
  for(int i=0; i<K; i++)
    a[i] = rand();

  for(int i=0; i<K; i++) {
    for(int j=0; j+1<i; j++) {
      if( a[j] > a[j+1] ) {
	int temp = a[j];
	a[j] = a[j+1];
	a[j+1] = temp;
      }
    }
  }

  pthread_exit(EXIT_SUCCESS);
}

/* Start scheduler. */
void *start_scheduler(void * ptr) {
  /* Install signal handlers before multithreading. */
  if( signal(SIGUSR1, handle_signals) == SIG_ERR ) {
    fprintf(stderr, "Couldn't attach SIGUSR2.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }
  if( signal(SIGUSR2, handle_signals) == SIG_ERR ) {
    fprintf(stderr, "Couldn't attach SIGUSR2.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }

  /* Initialize an empty ready queue. */
  q_in = q_out = 0;

  /* Create threads and update statuses. */
  for(int i=0; i<N; i++) {
    thread_args[i].tid = i;
    if( pthread_create(&threads[i], NULL,
		       &start_worker, &thread_args[i]) != 0 ) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
    thread_statuses[i] = THREAD_READY;
    pthread_kill(threads[i], SIGUSR1);
    ready_queue[q_in++] = i;
    if( q_in == BFSZ ) q_in = 0;
  }

  while( q_out != q_in ) { // ready_queue is not empty
    int next_thread = ready_queue[q_out++];
    if( q_out == BFSZ ) q_out = 0;

    struct timespec ts;
    if( clock_gettime(CLOCK_REALTIME, &ts) == -1 ) {
      perror("clock_gettime");
      exit(EXIT_FAILURE);
    }
    ts.tv_sec += 1; // set 1 second time quantum

    /* Wake up thread. */
    if( pthread_kill(threads[next_thread], SIGUSR2) != 0 ) {
      fprintf(stderr, "SIGUSR2 failed.");
      fflush(stderr);
      exit(EXIT_FAILURE);
    }
    thread_statuses[next_thread] = THREAD_RUNNING;

    /* Wait for thread to finish before timeout. */
    if( pthread_timedjoin_np(threads[next_thread], NULL, &ts) == 0 ) {
      thread_statuses[next_thread] = THREAD_TERMINATED;
    } else { /* Switch back on timeout. */
      if( pthread_kill(threads[next_thread], SIGUSR1) != 0 ) {
	fprintf(stderr, "SIGUSR1 failed.");
	fflush(stderr);
	exit(EXIT_FAILURE);
      }
      thread_statuses[next_thread] = THREAD_READY;
      ready_queue[q_in++] = next_thread;
      if( q_in == BFSZ ) q_in = 0;      
    }
  }

}

/* Start reporter. */
void *start_reporter(void * ptr) {
  while( ack_statuses == NULL || thread_statuses == NULL )
    continue;
  for(int i=0; ; i = (i+1)%N) {
    if( ack_statuses[i] != thread_statuses[i] ) {
      int flag = ack_statuses[i] = thread_statuses[i];
      if( flag == THREAD_READY ) {
	printf("Thread #%d pushed into READY queue.\n", i);
	fflush(stdout);
      } else if( flag == THREAD_RUNNING ) {
	printf("Thread #%d scheduled to run.\n", i);
	fflush(stdout);
      } else if( flag == THREAD_TERMINATED ) {
	printf("Thread #%d terminated.\n", i);
	fflush(stdout);
      }
    }
  }
}

int main(int argc, char*argv[]) {

  printf("Enter number of threads to run followed by number of integers to sort by each thread.\n");
  fflush(stdout);
  scanf("%d%d", &N, &K);
  if( N <= 1 )
    exit(EXIT_SUCCESS);
  BFSZ = N+1;
  pthread_t scheduler, reporter;

  threads = (pthread_t *) calloc(N, sizeof(pthread_t));
  thread_args = (thread_arg_t *) calloc(N, sizeof(thread_arg_t));
  thread_statuses = (int *) calloc(N, sizeof(int));
  ack_statuses = (int *) calloc(N, sizeof(int));
  ready_queue = (int *) calloc(BFSZ, sizeof(int));

  /* Create scheduler and reporter threads. */
  if( pthread_create(&scheduler, NULL, &start_scheduler, NULL) != 0 ) {
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }

  if( pthread_create(&reporter, NULL, &start_reporter, NULL) != 0 ) {
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }

  /* Wait for scheduler (which waits until all processes are finished.) */
  pthread_join(scheduler, NULL);
  /* Once joined, kill the reporter thread. */
  pthread_cancel(reporter);

  /* Free allocated memory. */
  free(threads);
  free(thread_args);
  free(thread_statuses);
  free(ack_statuses);
  free(ready_queue);

  exit(EXIT_SUCCESS);
}
