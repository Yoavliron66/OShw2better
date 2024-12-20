#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>



#define MAX_NUM_OF_THREADS 4096
#define MAX_NUM_OF_COUNTERS 100
#define NUM_OF_CMD_LINE_ARGS 5
#define MAX_LINE_WIDTH 1024
#define NUM_OF_OUTSTANDING 4096
#define MAX_NUM_OF_JOBS 100
#define COUNTER_FILE_NAME_WIDTH 13


typedef struct JOBSFIFO
{
    int size;
    char* jobs[NUM_OF_OUTSTANDING];
    int wr_ptr;
    int rd_ptr;
} jobs_fifo;

typedef struct TIMEFIFO {
    int size;
    long long read_time[NUM_OF_OUTSTANDING];
    int wr_ptr;
    int rd_ptr;
}time_fifo;


struct worker_data
{
    FILE** counters_fpp;
    jobs_fifo* fifo;
    time_fifo* time_fifo;
    int thread_number;
    time_t* start_time_ptr;
    long long job_read_time_long;
    int log_enabled;
}typedef worker_data;


bool is_fifo_empty(jobs_fifo* fifo);
bool is_fifo_full(jobs_fifo* fifo);
void fifo_push(jobs_fifo* fifo, char* inserted_job);
char* fifo_pop(jobs_fifo* fifo);
void free_fifo(jobs_fifo* fifo);
void init_fifo(jobs_fifo* fifo);
void free_fifo(jobs_fifo* fifo);
void make_file_name(char* command_x, char filename[COUNTER_FILE_NAME_WIDTH]);
void execute_command_line(int num_of_basic_commands, char** parsed_command_line);