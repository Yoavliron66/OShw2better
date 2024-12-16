//INCLUDES
#include <limits.h>
#include "hw2.h"

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

bool is_fifo_empty(jobs_fifo* fifo){
    if (fifo ->size == 0){
        return true;
    }
    return false;
}

bool is_fifo_full(jobs_fifo* fifo){
    if (fifo -> size == NUM_OF_OUTSTANDING){
        return true;
    }
    return false;
}

void fifo_push(jobs_fifo* fifo, char* inserted_job){
    if (is_fifo_full(fifo)){
        printf("Tried to push to a full FIFO, push aborted, job lost");
        return;
    }
    fifo->jobs[fifo->wr_ptr] = inserted_job;
    if (fifo->wr_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->wr_ptr = 0;
    }
    else {
        fifo->wr_ptr += 1;
    }
    fifo->size += 1;
}

char* fifo_pop(jobs_fifo* fifo){
    char* res;
    if (is_fifo_empty(fifo)){
        printf("Tried to pop from an empty FIFO, popping NULL");
        res = NULL;
        return res;
    }
    res = fifo->jobs[fifo->rd_ptr];
    if (fifo->rd_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->rd_ptr = 0;
    }
    else {
        fifo->rd_ptr += 1;
    }
    fifo->size -= 1;
    return res;
}

// FIX ME - check if needed
// void free_fifo(jobs_fifo* fifo){
//     for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
//         if (fifo->jobs[i] == NULL) {
//             continue;
//         }
//         free(fifo->jobs[i]);
//     }
// }


void init_fifo(jobs_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        fifo->jobs[i] = NULL;
    }
    fifo->size = 0;
    fifo->rd_ptr = 0;
    fifo->wr_ptr = 0;
}


int counter_semicolon(char *command)
{
    int counter = 0;
    for (char *c = command; *c != '\0'; c++)
    {
        if (*c == ';') counter++;
    }
    return counter;
}

int parse_command(char* command, char* job[MAX_NUM_OF_JOBS]){
    int arg_count = counter_semicolon(command) + 1;
    char *token = strtok(command, ";");
    //FIX ME - REMOVE
    //*argv = (char**)malloc(sizeof(char*)*(arg_count));
    int i = 0;
    while (token != NULL)
    {
        job[i] = (char*)malloc(strlen(token) + 1);
        strcpy(job[i], token);
        i++;
        token = strtok(NULL, ";");
    }
    return arg_count;
}

// Free the allocated memory for argv
void free_parsed_command(char* job[MAX_NUM_OF_JOBS], int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        free(job[i]);
    }
}

//Worker increment/decrement x which delta is for -1 or +1
//FIXME - PLS ADD COMMENTS
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

    int val_to_print = val + delta; 
    printf("counter number is %d, the valtp is %d\n",counter_number, val_to_print); //DEBUG
    fd = fopen(filename, "w");

    if (fd == NULL)
    {
        perror("Error opening file for writing");
        pthread_mutex_unlock(&counter_mutex[counter_number]);
        return;
    }

    fprintf(fd, "%d",val_to_print);
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
            if (command_token == NULL) continue;
            if (strcmp(command_token, "increment") == 0){
                char* x = strtok(NULL, " ");
                char file_name[13];
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                    fprintf(stderr, "Error: strtol failed.4, Job lost\n");
                    exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[13];
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                    fprintf(stderr, "Error: strtol failed.4, Job lost\n");
                    exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_sleep = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.3\n");
                exit(1);
                }
                usleep(x_sleep*1000);
            }
        }
    }
}

void counter_file_name(char* command_x, char filename[13])
{
    //FIXME - pls add comments
    strcpy(filename, "counterxx.txt");
    if (strlen(command_x) == 1) {
        filename[8] = command_x[0];
        filename[7] = 48;
    } else {
        filename[7] = command_x[0];
        filename[8] = command_x[1];
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
        printf("Worker active\n");
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
            pthread_mutex_lock(&running_mutex); //MUTEX LOCK
            if (running_flag==0) //terminating when fifo empty and the main loop is finished
            {
                pthread_mutex_unlock(&running_mutex);//MUTEX UNLOCK
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&running_mutex); //MUTEX UNLOCK

        }

       
        printf("wr ptr = %d, rd_ptr = %d, size = %d\n", fifo->wr_ptr, fifo->rd_ptr, fifo->size); //DEBUG
        char* command = fifo_pop(fifo);
        printf("Work has been popped by worker: %s\n", command); //DEBUG
        pthread_mutex_unlock(&fifo_mutex);

        //Handle awake workers counter - increment
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers ++;
        pthread_mutex_unlock(&num_awake_workers_mutex);


        char *job[MAX_NUM_OF_JOBS];
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
                char file_name[13];
                int counter_number = strtol(x, &end_ptr, 10);
                if (*end_ptr != '\0') {
                fprintf(stderr, "Error: strtol failed.4, Job lost\n");
                exit(1);
                }
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[13];
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
        fclose(thread_log_file);
        number_of_jobs++;
        pthread_mutex_unlock(&global_time_vars_mutex);
        // FIX ME - IF dispatcher wake some threads or all threads after pushing to FIFO the next line is not needed
        //pthread_cond_broadcast(&wake_up_worker);

        //Handle awake workers counter - decrement + wake-up call to dispatcher
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers --;
        if (num_awake_workers == 0) pthread_cond_broadcast(&wake_up_dispatcher); //wake up dispatcher if the last worker done his job
        pthread_mutex_unlock(&num_awake_workers_mutex);

        //terminating
        pthread_mutex_lock(&running_mutex); //MUTEX LOCK
        if (running_flag==0) //terminating when fifo empty and the main loop is finished
            {
            pthread_mutex_unlock(&running_mutex);//MUTEX UNLOCK
            pthread_exit(NULL);
            }
        pthread_mutex_unlock(&running_mutex); //MUTEX UNLOCK

    } // end of while(1)

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
    init_fifo(&fifo);
    printf("FIFO has been initialized\n");
    bool full = true;
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
        char* new_line = strchr(job,'\n');
        if (new_line)
        {
            *new_line = '\0';
        }
        
        if (strcmp(job, "\n") == 0) continue; //empty line = "\n"

        //Separate between worker and dispathcer job
        char* token = strtok(job," "); // FIXME - check with Gadi, IGOR
        if (token == NULL) {
                fprintf(stderr, "Error:strtok failed.\n");
                exit(1);
                }

        //Dispatcher code
        if (strcmp(token,"dispatcher") == 0)
        {
            token = strtok(NULL," "); 
            if (token == NULL) {
            fprintf(stderr, "Error: missing command after 'dispatcher'.\n");
            exit(1);
            }

            if (strcmp(token,"wait") == 0)
            {
                pthread_mutex_lock(&num_awake_workers_mutex); // FIFO MUTEX LOCK
                while (num_awake_workers>0)
                {
                    pthread_cond_wait(&wake_up_dispatcher,&num_awake_workers_mutex);
                }
                pthread_mutex_unlock(&num_awake_workers_mutex); // FIFO MUTEX UNLOCK
            }

            if (strcmp(token,"sleep") == 0)
            {
                token = strtok(NULL,"\n");// fetch the X var - time for sleep
                if (token == NULL) {
                fprintf(stderr, "Error: missing sleep time after 'sleep'.\n");
                exit(1);
                }
                sleep_time = (int) strtol(token,&end_ptr,10); //sleeptime in miliseconds
                usleep (1000*sleep_time);
            }

        }//End of dispatcher code
        //Woker code
        if (strcmp(token,"worker") == 0)
        {

            while (full)
            {
                pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK

                if (!is_fifo_full(&fifo)) {
                    full = false;
                    pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UN
                    break;
                }
                full = true;
                pthread_cond_broadcast(&wake_up_worker); // wake up any thread
                pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
            }

            //push job to the FIFO
            pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK
            fifo_push(&fifo,job+strlen(token)+1); // job pointer incremented by len of worker + space 
            pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
            pthread_cond_broadcast(&wake_up_worker); // wake up any thread
            printf("work has been pushed to the FIFO %s\n", job+strlen(token)+1); //DEBUG
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
        pthread_cond_broadcast(&wake_up_worker); // wake up any thread
        pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
    }

    //termination of worker loop
    pthread_mutex_lock(&running_mutex); //MUTEX LOCK
    running_flag = 0;
    pthread_mutex_unlock(&running_mutex); //MUTEX UNLOCK

    pthread_cond_broadcast(&wake_up_worker);

    //Wait for all workers to finish
    for (int i = 0; i < num_threads; i++)
        {
            printf("try to join %d worker\n",i); //DEBUG
            pthread_join(workers[i],NULL);
            printf("thread %d sucsessfully joined \n",i); //DEBUG

        }

    //Destroy dynamic initiated mutexes
    destroy_mutexes();
    //Free counters names array
    for (int i = 0; i < num_counters; i++) {
    if (counters_names[i] != NULL){
    free(counters_names[i]); 
    counters_names[i] = NULL;}

    }


    //Statistics

    //Logs

    return 0;
    }



