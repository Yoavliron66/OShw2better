//INCLUDES
#include <limits.h>
#include "hw2.h"
#include "jobs_fifo.c"

//global time statistic veriables
long long jobs_turnaround_time = 0;
long long jobs_mintime = LLONG_MAX;
long long jobs_maxtime = 0;

//global number of awake workers
int num_awake_workers = 0;

//global numer of jobs statistic veriable
int number_of_jobs = 0;

//Global running flag
int running_flag = 1;

//MUTEXES
pthread_mutex_t fifo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex[MAX_NUM_OF_COUNTERS];
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_time_vars_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_awake_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

//COND VAR
pthread_cond_t wake_up_worker = PTHREAD_COND_INITIALIZER; // wake-up signal for each worker
pthread_cond_t wake_up_dispatcher = PTHREAD_COND_INITIALIZER; // wake-up signal for dispatcher


int counter_semicolon(char *command)
{
    int counter = 0;
    for (char *c = command; *c != '\0'; c++)
    {
        if (*c == ';') counter++;
    }
    return counter;
}

int parse_command(char* command, char** argv){
    int arg_count = counter_semicolon(command) + 1;
    char *token = strtok(command, ";");
    argv = (char**)malloc(sizeof(char*)*(arg_count));
    int i = 0;

     while (token != NULL)
    {
        argv[i] = (char*)malloc(strlen(token) + 1);
        strcpy(argv[i], token);
        i++;
        token = strtok(NULL, ";");
    }
    return arg_count;
}

// Free the allocated memory for argv
void free_parsed_command(char **argv, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        if (argv[i] != NULL) {
            free(argv[i]); // Free each string
        }
    }
    free(argv); // Free the array of pointers
}

//Worker increment/decrement x which delta is for -1 or +1
void update_counter_file(char *filename, int delta, int counter_number)
{
    FILE* fd;
    int val = 0;
    pthread_mutex_lock(&counter_mutex[counter_number]);
    fd = fopen(filename, "r");
    if(fd == NULL)
    {
        perror("Error opening file for reading");
        pthread_mutex_unlock(&counter_mutex[counter_number]);
        return;
    }
    if (fscanf(fd,"%d",&val)!=1)
    {
        fprintf(stderr, "Error reading number from file\n");
        fclose(fd);
        pthread_mutex_unlock(&counter_mutex[counter_number]);
        return;
    }
    fclose(fd);
    pthread_mutex_unlock(&counter_mutex[counter_number]);
    int val_to_prinnt = val + delta;
    pthread_mutex_lock(&counter_mutex[counter_number]);
    fd = fopen(filename, "w");
    if (fd == NULL)
    {
        perror("Error opening file for writing");
        pthread_mutex_unlock(&counter_mutex[counter_number]);
        return;
    }
    fprintf(fd, "%d",val+val_to_prinnt);
    fclose(fd);
    pthread_mutex_unlock(&counter_mutex[counter_number]);
}


void msleep (int mseconds)
{
    usleep(mseconds*1000);
}

void repeat (int start_command, char** job, int count_commands, int times)
{
    char* end_ptr = NULL;
    for (int j=0;j<times;j++)
    {
       for (int i = start_command + 1; i < count_commands; i++){
            char* command_token = strtok(job[i], " ");
            if (strcmp(command_token, "increment") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.1\n");
                exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.2\n");
                exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_sleep = (int)strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.3\n");
                exit(1);
                }
                usleep(x_sleep*1000);
            }
        }
    }
}

void counter_file_name(char* command_x, char* filename)
{
    bool less_then_ten = false;
    if (strlen(command_x) == 1) less_then_ten = true;
    if (less_then_ten){
        filename[5] = '0';
        filename[6] = command_x[0];
    }
    else {
        filename[5] = command_x[0];
        filename[6] = command_x[1];
    }
}

// Initialize the mutex array for counters files
void init_mutexes() {
    for (int i = 0; i < MAX_NUM_OF_COUNTERS; i++) {
        if (pthread_mutex_init(&counter_mutex[i], NULL) != 0) {
            perror("Failed to initialize counter mutex");
            exit(1);
        }
    }
}

// Destroy the mutex array for counters files
void destroy_mutexes() {
    for (int i = 0; i < MAX_NUM_OF_COUNTERS; i++) {
        pthread_mutex_destroy(&counter_mutex[i]);
    }
}

void* worker_main(void *data)
{
    worker_data* wdata = (worker_data*)data;
    FILE** counters = wdata->counters_fpp;
    jobs_fifo* fifo = wdata->fifo;
    int whoami = wdata->thread_number;
    time_t* start_known_to_w = wdata->start_time_ptr;
    char* end_ptr = NULL; // string for strtol usage
    //init log file

    while (1)
    {
        // Open the worker log file
        char worker_log_file_name[20];
        sprintf(worker_log_file_name, "thread%04d.txt", whoami); // Zero-padded to 4 digits
        FILE* thread_log_file;
        thread_log_file = fopen(worker_log_file_name, "w");
        if (thread_log_file == NULL) {
            printf("worker %d failed to open his log file with path %s", whoami, worker_log_file_name);
            exit(1);
        }

        pthread_mutex_lock(&running_mutex);
        if (!running_flag)
        {
            pthread_mutex_unlock(&running_mutex);
            break;
        }
        pthread_mutex_unlock(&running_mutex);
        pthread_mutex_lock(&fifo_mutex);
        while (is_fifo_empty(fifo))
        {
            pthread_cond_wait(&wake_up_worker,&fifo_mutex);
        }
        char* command = fifo_pop(fifo);
        pthread_mutex_unlock(&fifo_mutex);

        //Handle awake workers counter - increment
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers ++;
        pthread_mutex_unlock(&num_awake_workers_mutex);


        char **job = NULL;
        //Jobs starts here
        //calc start jobs time
        int count_commands = parse_command(command,job);
        time_t job_started_time;
        job_started_time = clock();
        long long job_start_elapsed_time = ((long long)(job_started_time - *start_known_to_w) *1000) / CLOCKS_PER_SEC;
        fprintf(thread_log_file, "TIME %lld: START job %s\n", job_start_elapsed_time, command);

        // Job exec
        
        for (int i = 0; i < count_commands; i++){
            char* command_token = strtok(job[i], " ");
            if (strcmp(command_token, "increment") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.4\n");
                exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[13] = "counterxx.txt";
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.5\n");
                exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_sleep = (int)strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.6\n");
                exit(1);
                }
                usleep(x_sleep*1000);
            }
            if (strcmp(command_token, "repeat") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_repeat = (int)strtol(x,&end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.7\n");
                exit(1);
                }

                repeat(i, job, count_commands, x_repeat);
                break;
            }
        }
        time_t job_ended_time = clock();
        long long job_end_elapsed_time = ((long long)(job_ended_time - *start_known_to_w) *1000) / CLOCKS_PER_SEC;
        fprintf(thread_log_file, "TIME %lld: END job %s\n", job_end_elapsed_time, command);
        free_parsed_command(job,count_commands);
        //statistics area
        pthread_mutex_lock(&global_time_vars_mutex);
        long long delta_worktime = ((long long)(job_ended_time - job_start_elapsed_time) *1000) / CLOCKS_PER_SEC;
        jobs_turnaround_time += delta_worktime;
        if (delta_worktime < jobs_mintime) {
            jobs_mintime = delta_worktime;
        }
        if (delta_worktime > jobs_maxtime) {
            jobs_maxtime = delta_worktime;
        }
        number_of_jobs++;
        pthread_mutex_unlock(&global_time_vars_mutex);
        // FIX ME - IF dispatcher wake some threads or all threads after pushing to FIFO the next line is not needed
        //pthread_cond_broadcast(&wake_up_worker);

        //Handle awake workers counter - decrement + wake-up call to dispatcher
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers --;
        if (num_awake_workers == 0) pthread_cond_broadcast(&wake_up_dispatcher); //wake up dispatcher if the last worker done his job
        pthread_mutex_unlock(&num_awake_workers_mutex);
    }
    return 0;

}

//Dispatcher code - main function

int main(int argc, char* argv[])
{
    //Mutexes initialization
    init_mutexes();

    //Start point of the timer
    time_t start_time;
    start_time = clock();

    //Initialize the dispatcher
    jobs_fifo fifo;
    bool full = false;
    bool empty = false;
    worker_data thread_data[MAX_NUM_OF_THREADS];
    int sleep_time =0;
    char* end_ptr = NULL; //string for strtol usage
    //Analyze the command line arguments
    if (argc != 5) {
                fprintf(stderr, "Error: wrong number of arguments.\n");
                exit(1);
                }

    int num_counters = (int) strtol(argv[3],&end_ptr,10);
    if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.8\n");
                exit(1);
                }
    int num_threads = (int) strtol(argv[2],&end_ptr,10);
    if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.9\n");
                exit(1);
                }
    int log_enabled = (int) strtol(argv[4],&end_ptr,10);
    if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.10\n");
                exit(1);
                }

    FILE* cmd_file_fp = fopen(argv[1],"r");
    if (cmd_file_fp == NULL) {
                fprintf(stderr, "Error: Cmd file opening fail.\n");
                exit(1);
                }

   

    //Create counters - creates num_counts of counters as txt file, each initialized to 0 inside
    char* counters_names [MAX_NUM_OF_COUNTERS];
    for (int i = 0; i < num_counters; i++) {
    counters_names[i] = (char*) malloc(10 * sizeof(char)); // allocate 10 chars for each counter name - "counterXX\0"
    sprintf(counters_names[i], "counter%02d.txt", i); 
    }

    FILE* counters_fp [MAX_NUM_OF_COUNTERS];
    for (int i = 0; i < num_counters; i++)
    {
        counters_fp[i] = fopen(counters_names[i],"w");
        if (counters_fp [i] == NULL) {
            printf("Failed to open a counter%02d file\n",i);
            exit(1);
        }
        fprintf(counters_fp[i],"%d",0); //FIXME - check it
        pthread_mutex_init(&counter_mutex[i],NULL); //FIXME check it
        fclose(counters_fp[i]);
    }

    //Create threads
    pthread_t workers[MAX_NUM_OF_THREADS];
    //Create threads log files
    FILE* worker_logs[MAX_NUM_OF_THREADS];


    for (int i = 0; i < num_threads; i++)
    {
        char worker_log_file_name[20]; // FIXME - check it - char* or char
        sprintf(worker_log_file_name, "thread%04d.txt", i); // Zero-padded to 4 digits
        worker_logs[i] = fopen(worker_log_file_name, "w");
        if (worker_logs[i] == NULL) {
            printf("Failed to open a thread log file\n");
            exit(1);
        }
        fclose(worker_logs[i]);
        thread_data[i].counters_fpp = counters_fp;
        thread_data[i].fifo = &fifo;
        thread_data[i].thread_number = i;
        thread_data[i].start_time_ptr = &start_time;
        pthread_create(&workers[i], NULL, worker_main,(void *)&thread_data[i]);

    }

    ////////////////////////////////////////////////////
    //////////////////Run Dispatcher////////////////////
    ////////////////////////////////////////////////////

    //Read the jobs from the cmdfile
    char job[MAX_LINE_WIDTH];
    while (fgets(job,MAX_LINE_WIDTH,cmd_file_fp)){

    //Separate between worker and dispathcer job
    char* token = strtok(job," ");
    assert (token); //Dispatcher or Worker

    //Dispatcher code
    if (!strcmp(token,"dispatcher"))
    {
        token = strtok(NULL," ");
        if (token == NULL) {
        fprintf(stderr, "Error: missing command after 'dispatcher'.\n");
        exit(1);
        }

        if (!strcmp(token,"wait"))
        {   
            pthread_mutex_lock(&num_awake_workers_mutex); // FIFO MUTEX LOCK
            while (num_awake_workers>0)
            {
                pthread_cond_wait(&wake_up_dispatcher,&num_awake_workers_mutex);
            }
            pthread_mutex_unlock(&num_awake_workers_mutex); // FIFO MUTEX UNLOCK
        }

        if (!strcmp(token,"sleep"))
        {
            token = strtok(NULL," ");// fetch the X var - time for sleep
            if (token == NULL) {
            fprintf(stderr, "Error: missing sleep time after 'sleep'.\n");
            exit(1);
            }

            sleep_time = (int) strtol(token,&end_ptr,10); //sleeptime in miliseconds
            printf("sleep_time is %d \n",sleep_time);
            printf("end ptr is %c \n",*end_ptr);
            // if (*end_ptr != '\0' || *end_ptr ) {
            // fprintf(stderr, "Error: strtol failed.11\n");
            // exit(1);
            // }
            usleep (1000*sleep_time);
        }

    }//End of dispatcher code

    //Woker code
    if (strcmp(token,"worker"))
    {

        while (full)
        {
            pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK

            if (!is_fifo_full(&fifo)) {
                full = false;
                pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UN
                break;
            }

            pthread_cond_signal(&wake_up_worker); // wake up any thread
            pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
        }

        //push job to the FIFO
        pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK
        fifo_push(&fifo,job+strlen(token)+1); // job pointer incremented by len of worker + space
        pthread_cond_signal(&wake_up_worker); // wake up any thread
        pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK

    } //End of worker code
    }
    ////////////////////////////////////////////////////
    //////////////////Exit Dispatcher///////////////////
    ////////////////////////////////////////////////////

    //Verify that fifo is empty
    while (!empty)
    {
        pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK

        if (is_fifo_empty(&fifo)) {
                empty = true;
                pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
                break;
            }
        pthread_cond_signal(&wake_up_worker); // wake up any thread
        pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
    }

    //Wait for all workers to finish
    for (int i = 0; i < num_threads; i++)
        {
            pthread_join(workers[i],NULL);
        }

    //Destroy dynamic initiated mutexes
    destroy_mutexes();
    //Free counters names array
    for (int i = 0; i < num_counters; i++) {
    free(counters_names[i]); 
    counters_names[i] = NULL;
    }
    //Statistics

    //Logs


    }



