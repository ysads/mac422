#include <pthread.h>

#define MAX_JOBS 100
#define CLOCK_LEN 200 

#define FCFS 1
#define SRTN 2
#define ROUND_ROBIN 3

typedef struct sim {
    char name[30];
    int t0;
    int dt;
    int deadline;
    int acknowledged;           // Whether the scheduler knows about this job or not
    int remaining;              // How many seconds the job needs to run
    int tf;                     // The moment the job stop its execution
    int is_paused;
    pthread_t thread;           // A pointer to this job's thread
    pthread_mutex_t mutex;
    // pthread_cond_t is_paused;   // Whether the associated thread should be paused
} job_t;

typedef struct sim_list {
    job_t* list[MAX_JOBS];
    int length;
} job_list_t;