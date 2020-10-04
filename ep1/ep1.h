#include <pthread.h>

#define MAX_JOBS 100
#define MAX_LINE_LEN 1000
#define CLOCK_LEN 1

#define FCFS 1
#define SRTN 2
#define ROUND_ROBIN 3

typedef struct job {
    char name[30];
    int t0;
    int dt;
    int deadline;
    int remaining;              // How many seconds the job needs to run
    int tf;                     // The moment the job stop its execution
    int is_paused;              // Whether this job should be paused or not
    pthread_t thread;           // A pointer to this job's thread
    pthread_mutex_t mutex;      // A mutex to control access to thread state
    pthread_cond_t cond;        // Conditional variable that awaits a signal to resume the job
} job_t;


typedef struct job_list {
    job_t* list[MAX_JOBS];
    int length;
} job_list_t;


typedef struct sim_state {
    job_t *job;
    job_list_t* jobs_done;
    job_list_t* jobs_ready;
    time_t started_at;
} job_simulation_t;
