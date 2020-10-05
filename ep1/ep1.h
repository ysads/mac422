#include <pthread.h>

#define MAX_JOBS 1000
#define MAX_LINE_LEN 1000
#define CLOCK_LEN 1

#define FCFS 1
#define SRTN 2
#define ROUND_ROBIN 3

#define NOW_OR_BEFORE 1
#define NOW 2

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
    time_t started_at;          // Instant at which the simulation started
    pthread_mutex_t mutex;      // Orchestrate operations to jobs_done and jobs_ready accross threads
} job_simulation_t;