#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ep1.h"

#define debug(...) if (DEBUG) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); }

int DEBUG = 0;

int getcpu;

time_t start_at;

job_t* new_job() {
    job_t* job = (job_t*) malloc(sizeof(job_t));
    return job;
}


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
 * Parse all jobs from trace file and input them into a list
 */
void read_jobs(FILE* file, job_list_t* ready_jobs) {
    job_t* job;

    ready_jobs->length = 0;
    while (!feof(file)) {
        job = new_job();

        fscanf(file, "%s", job->name);
        fscanf(file, "%d", &job->t0);
        fscanf(file, "%d", &job->dt);
        fscanf(file, "%d", &job->deadline);

        job->acknowledged = 0;
        job->is_paused = 1;
        job->remaining = job->dt;

        ready_jobs->list[ready_jobs->length] = job;
        ready_jobs->length++;
    }
}


/**
 * Helper function meant to clear every allocated object
 */
void free_jobs(job_list_t* jobs) {
    job_t* job;

    while (jobs->length >= 0) {
        job = jobs->list[jobs->length];
        free(job);
        jobs->length--;
    }
    free(jobs);
}


/**
 * Tells whether a list has jobs yet to be processed or not
 */
int jobs_left(job_list_t jobs_ready) {
    return jobs_ready.length > 0;
}


/**
 * Yields whether a job still needs to be executed or not
 */
int job_finished(job_t* job) {
    return job->remaining == 0;
}


/**
 * Tells whether the job teorethical start time has passed, which would
 * indicate that the job can run.
 */
int can_run(job_t* job, time_t start) {
    time_t now;
    time_t job_start = start + job->t0;

    time(&now);

    return now >= job_start;
}


/**
 * This function sets a job as acknowledged, ie, as known by the scheduler.
 * Since all jobs are preloaded in memory when the trace file is read, this
 * is the equivalent as 'the batch of jobs received by the system at a particular
 * moment of time'.
 */
void acknowledge_jobs(job_t* curr_job, job_list_t* jobs_ready) {
    int i;
    job_t* job;

    if (!curr_job->acknowledged) {
        debug("new process: %s %d %d %d\n", curr_job->name, curr_job->t0, curr_job->dt, curr_job->deadline);
        curr_job->acknowledged = 1;
    }

    i = 1;
    job = jobs_ready->list[i];

    while (job != NULL && curr_job->t0 == job->t0 && !job->acknowledged) {
        debug("new process: %s %d %d %d\n", job->name, job->t0, job->dt, job->deadline);
        job->acknowledged = 1;
        job = jobs_ready->list[++i];
    }
}


/**
 * The function which simulates a time consuming task. It is meant to
 * be run onto a separate thread.
 */
void* work(void* arg) {
    job_list_t* jobs_ready = (job_list_t*) arg;
    job_t* job = jobs_ready->list[0];
    time_t now;
    int i;

    time(&now);
    now=now - start_at;

    i=1;

    getcpu=sched_getcpu();

    debug("process %s started using cpu %d\n", job->name, getcpu);

    while (!job_finished(job)) {
        sleep(1);

        /*
         * Checks if any job is coming to the scheduler at given moment.
         */
        if(jobs_ready->list[i]!= NULL && now==jobs_ready->list[i]->t0){
          acknowledge_jobs(jobs_ready->list[i], jobs_ready);
          i++;
        }
        now++;
        job->remaining--;
    }

    return NULL;
}


/**
 * Register the moment the job finished its execution adding it to
 * the list of done jobs.
 */
void mark_as_finished(int elapsed, job_t* job, job_list_t* jobs_done) {
    debug("process %s ended using cpu %d\n", job->name, getcpu);
    job->tf = elapsed;
    jobs_done->list[jobs_done->length] = job;
    jobs_done->length++;

}


/**
 * Removes a job from the front of a job list, moving the remaining
 * items one position ahead
 */
void pop_job(job_list_t* jobs) {
    int i;

    for (i = 0; i < jobs->length - 1; i++) {
        jobs->list[i] = jobs->list[i+1];
    }

    jobs->length--;
    jobs->list[jobs->length] = NULL;
}


/**
 * Simulates jobs processing using first-come first-served scheduler.
 * A new thread is created for each job and it remains running until
 * it completes its task. Note that a job may not start being processed
 * before its initial time–held in t0 attribute.
 */
int fcfs_run(job_list_t* jobs_ready, job_list_t* jobs_done) {
    job_t* curr_job;
    time_t finish_at;

    time(&start_at);

    while (jobs_left(*jobs_ready)) {
        curr_job = jobs_ready->list[0];

        if (!can_run(curr_job, start_at)) {
            sleep(1);
            continue;
        }

        acknowledge_jobs(curr_job, jobs_ready);

        /**
         * Creates a new thread using the inner pointer in job object
         * so that it can perform its task and wait until the thread
         * finishes
         */
        pthread_create(&curr_job->thread, NULL, work, jobs_ready);

        pthread_join(curr_job->thread, NULL);

        time(&finish_at);

        mark_as_finished(finish_at - start_at, curr_job, jobs_done);
        pop_job(jobs_ready);
    }

    /**
     * No context change is done since each thread runs until it's done
     */
    return 0;
}


/**
 * Decides which scheduler simulator should be used proxying the amount of
 * changes it took while simulating the jobs.
 */
int run_scheduler(int scheduler, job_list_t* jobs_ready, job_list_t* jobs_done) {
    switch (scheduler) {
        case FCFS:
            return fcfs_run(jobs_ready, jobs_done);
            break;

        case SRTN:
            printf("To be done...\n");
            break;

        case ROUND_ROBIN:
            printf("To be done...\n");
            break;
    }
    return 0;
}


void write_results(FILE* file, job_list_t* jobs, int context_changes) {
    job_t* job;
    int i;

    for (i = 0; i < jobs->length; i++) {
        job = jobs->list[i];
        fprintf(file, "%s %d %d\n", job->name, job->tf, job->tf - job->t0);
        if(i==jobs->length-1)debug("Tasks finished: %d %d\n", job->tf, job->tf - job->t0);
    }
    fprintf(file, "%d\n", context_changes);
    debug("Contexts changes: %d\n", context_changes);
}



int main(int argc, char* argv[]) {
    FILE *file_input, *file_output;
    job_list_t *jobs_ready, *jobs_done;
    int scheduler, context_changes;

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

    jobs_ready = new_job_list();
    jobs_done = new_job_list();

    /**
     * Read all the jobs from input file_input at once and make current job
     * as the first one.
     */
    read_jobs(file_input, jobs_ready);

    context_changes = run_scheduler(scheduler, jobs_ready, jobs_done);

    write_results(file_output, jobs_done, context_changes);

    fclose(file_input);
    fclose(file_output);

    free_jobs(jobs_ready);
    free_jobs(jobs_done);

    return 0;
}
