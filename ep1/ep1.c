#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ep1.h"

#define debug(...) if (DEBUG) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); }

int DEBUG = 0;


/**
 * Allocates space for a job object.
 */
job_t* new_job() {
    job_t* job = (job_t*) malloc(sizeof(job_t));
    return job;
}


/**
 * Allocates space for a job list object, initializing the jobs list
 * with NULL.
 */
job_list_t* new_job_list() {
    int i;
    job_list_t* jobs = (job_list_t*) malloc(sizeof(job_list_t));
    
    jobs->length = 0;
    for (i = 0; i < MAX_JOBS; i++) {
        jobs->list[i] = NULL;
    }

    return jobs;
}


/**
 * Removes a job from the job list by comparing their name. Note that it
 * moves the remaining array one position ahead.
 */
void remove_job(job_list_t* jobs, job_t* job) {
    int i;

    i = 0;
    while (strcmp(jobs->list[i]->name, job->name) != 0) {
        i++;
    }
    while (i < jobs->length) {
        jobs->list[i] = jobs->list[i+1];
        i++;
    }
    jobs->length--;
}


/**
 * Helper function meant to clear every allocated job. Note that it also
 * destroys thread-related stuff if the thread ever existed.
 */
void free_jobs(job_list_t* jobs) {
    job_t* job;
    int i;

    for (i = 0; i < jobs->length; i++) {
        job = jobs->list[i];

        if (job->thread) {
            pthread_mutex_destroy(&job->mutex);
            pthread_cond_destroy(&job->cond);
        }
        free(job);
    }
    free(jobs);
}


/**
 * Finishes the simulation of a particular job, recording the instant in
 * which it ended and moving it from ready to done list.
 */
void finish_simulation(job_simulation_t* simulation) {
    job_list_t* jobs_done = simulation->jobs_done;
    time_t finished_at;

    time(&finished_at);
    simulation->job->tf = finished_at - simulation->started_at;
    
    jobs_done->list[jobs_done->length] = simulation->job;
    jobs_done->length++;

    remove_job(simulation->jobs_ready, simulation->job);
}


/**
 * The function which simulates a time consuming task. It is meant to
 * be run onto a separate thread.
 */
void* work(void* arg) {
    job_simulation_t* simulation = (job_simulation_t*) arg;
    job_t* job = (job_t*) simulation->job;
    
    debug("%s: started\n", job->name);

    while (job->remaining) {
        debug("%s: %ds left\n", job->name, job->remaining);
        pthread_mutex_lock(&job->mutex);
        if (job->is_paused) {
            debug("%s: paused\n", job->name);
            pthread_mutex_unlock(&job->mutex);
            pthread_cond_wait(&job->cond, &job->mutex);
            debug("%s: resumed\n", job->name);
        }
        pthread_mutex_unlock(&job->mutex);

        sleep(1);
        job->remaining--;
    }

    finish_simulation(simulation);
    debug("%s: finished\n", job->name);

    return NULL;
}


/**
 * There are jobs left as long as there is lines to parsed on trace file
 * or jobs already parsed on ready list.
 */
int jobs_left(FILE* file, job_list_t* jobs_ready) {
    return !feof(file) || jobs_ready->length > 0;
}


/**
 * Preemption occurs whenever the previous and the current jobs are present
 * and their names don't match.
 */
int had_preemption(job_t* prev_job, job_t* curr_job) {
    return prev_job && curr_job && strcmp(prev_job->name, curr_job->name) != 0;
}


/**
 * Parses job data contained within a given string. This string corresponds
 * to a line in trace file.
 */
job_t* read_job(char* job_data) {
    job_t* job;

    job = new_job();

    sscanf(job_data, "%s %d %d %d", job->name, &job->t0, &job->dt, &job->deadline);

    job->thread = NULL;    
    job->is_paused = 1;
    job->remaining = job->dt;

    return job;
}


/**
 * Tries to parse all jobs starting at a particular instant so that we discover all
 * new jobs and decide which one is the shortest.
 */
void jobs_starting_at(FILE* file, job_list_t* next_jobs, int curr_instant) {
    job_t* job;
    char job_data[MAX_LINE_LEN];

    while (!feof(file)) {
        fgets(job_data, MAX_LINE_LEN, file);
        job = read_job(job_data);

        /**
         * If the job read does not start in the current instant, rewind to previous
         * line on trace file and abort execution
         */
        if (job->t0 != curr_instant) {
            fseek(file, -strlen(job_data), SEEK_CUR);
            break;
        } else {
            debug("new process: %s %d %d %d\n", job->name, job->t0, job->dt, job->deadline);

            next_jobs->list[next_jobs->length] = job;
            next_jobs->length++;
        }
    }
}


/**
 * Acquires the lock of a particular job's thread and set is paused flag to
 * true, in such a way that its execution is stopped.
 */
void pause_job(job_t* job) {
    if (job == NULL) {
        return;
    }

    pthread_mutex_lock(&job->mutex);
    job->is_paused = 1;
    pthread_mutex_unlock(&job->mutex);
}


/**
 * This function shall start a job's thread by updating its associated flag
 * to true. If the jobs doesn't have a thread yet, creates one and init its
 * mutex and conditional variables.
 */
void start_job(job_simulation_t* simulation) {
    job_t* job = simulation->job;

    if (job->thread == NULL) { 
        pthread_mutex_init(&job->mutex, NULL);
        pthread_cond_init(&job->cond, NULL);
        pthread_create(&job->thread, NULL, work, simulation);
    }

    pthread_mutex_lock(&job->mutex);
    job->is_paused = 0;
    pthread_cond_signal(&job->cond);
    pthread_mutex_unlock(&job->mutex);
}


/* ========================== */
/* ======== SRTN DEFS ======= */
/* ========================== */

/**
 * Inserts a new job into a job list in such a way that the list it kept
 * sorted by its remanining time.
 */
void insert_sorted(job_list_t* jobs, job_t* new_job) {
    int i;

    i = jobs->length;
    while (i > 0 && jobs->list[i-1]->remaining > new_job->remaining) {
        jobs->list[i] = jobs->list[i-1];
        i--;
    }
    jobs->list[i] = new_job;
    jobs->length++;
}


/**
 * This function inserts all new jobs into the ready list in such a way
 * that they are sorted by their remaining time.
 */
job_t* insert_new_jobs_sorted(job_list_t* next_jobs, job_list_t* jobs_ready) {
    int i;

    for (i = 0; i < next_jobs->length; i++) {
        insert_sorted(jobs_ready, next_jobs->list[i]);
    }

    return jobs_ready->list[0];
}


/**
 * Simulates jobs processing using shortest-remaining-time-next scheduler.
 */
int srtn_run(FILE* file_input, job_list_t* jobs_done) {
    job_simulation_t simulation;
    job_list_t *next_jobs, *jobs_ready;
    job_t *curr_job, *prev_job;
    time_t start, now;
    int i, instant, preemptions;
    
    time(&start);

    next_jobs = new_job_list();
    jobs_ready = new_job_list();
    
    preemptions = 0;
    curr_job = NULL;

    simulation.started_at = start;
    simulation.jobs_done = jobs_done;
    simulation.jobs_ready = jobs_ready;

    /**
     * We'll keep iterating until we reach the end of the file or we don't
     * have any job to process on ready list.
     */
    while (jobs_left(file_input, jobs_ready)) {
        time(&now);
        instant = now - start;
        printf("\n\n@ %ds\n", instant);

        /**
         * We always pause the executing thread before checking any new
         * processes arriving at the simulator. We also save the current
         * job being executed so we can find whether a preemption occurred.
         */
        pause_job(curr_job);
        prev_job = curr_job;

        jobs_starting_at(file_input, next_jobs, instant);
        curr_job = insert_new_jobs_sorted(next_jobs, jobs_ready);

        if (had_preemption(prev_job, curr_job)) {
            preemptions++;
        }
        if (curr_job) {
            simulation.job = curr_job;
            start_job(&simulation);
        }

        /**
         * We reset the next_jobs length so we don't end up appending the
         * future jobs to this helper array; then, we sleep until next second.
         */
        next_jobs->length = 0;
        sleep(CLOCK_LEN);
    }

    /**
     * Makes sure every thread has been finished before trying returning in
     * addition to freeing the next_jobs list.
     */
    for (i = 0; i < jobs_ready->length; i++) {
        pthread_join(jobs_ready->list[i]->thread, NULL);
    }
    free_jobs(next_jobs);
    
    return preemptions;
}


/**
 * Decides which scheduler simulator should be used proxying the amount of
 * changes it took while simulating the jobs.
 */
int run_scheduler(int scheduler, FILE* file_input, job_list_t* jobs_done) {
    switch (scheduler) {
        case FCFS:
            // return fcfs_run(file_input, jobs_done);
            break;

        case SRTN:
            return srtn_run(file_input, jobs_done);
            break;

        case ROUND_ROBIN:
            printf("To be done...\n");
            break;
    }
    return 0;
}


void write_results(FILE* file, job_list_t* jobs, int preemptions) {
    job_t* job;
    int i;

    for (i = 0; i < jobs->length; i++) {
        job = jobs->list[i];
        fprintf(file, "%s %d %d\n", job->name, job->tf, job->tf - job->t0);
    }
    fprintf(file, "%d\n", preemptions);
}


int main(int argc, char* argv[]) {
    FILE *file_input, *file_output;
    job_list_t* jobs_done;
    int scheduler, preemptions;

    if (argc < 4) {
        printf("Uso: ./ep1 <escalonador> <arq-trace> <arq-saida> [d]\n");
        return 1;
    }

    scheduler = atoi(argv[1]);
    file_input = fopen(argv[2], "r");
    file_output = fopen(argv[3], "w");
    
    /**
     * Set the global DEBUG variable if optional debugger parameter is given
     */
    DEBUG = (argc == 5 && argv[4][0] == 'd') ? 1 : 0;

    jobs_done = new_job_list();
    preemptions = run_scheduler(scheduler, file_input, jobs_done);

    write_results(file_output, jobs_done, preemptions);

    fclose(file_input);
    fclose(file_output);

    free_jobs(jobs_done);

    return 0;
}