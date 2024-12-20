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
int num_of_waiting_threads = 0; // under fifo cond and mutex

//MUTEXES
pthread_mutex_t fifo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex[MAX_NUM_OF_COUNTERS];
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_time_vars_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_awake_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

//COND VAR
pthread_cond_t wake_up_worker = PTHREAD_COND_INITIALIZER; // wake-up signal for each worker
pthread_cond_t wake_up_dispatcher = PTHREAD_COND_INITIALIZER; // wake-up signal for dispatcher

//jobs fifo functions

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
    //FIFO is full case
    if (is_fifo_full(fifo)){
        printf("Tried to push to a full FIFO, push aborted, job lost");
        return;
    }

    //FIFO is not full case
    strcpy(fifo->jobs[fifo->wr_ptr],inserted_job);
    //wrap around
    if (fifo->wr_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->wr_ptr = 0;
    }
    else {
        fifo->wr_ptr ++;
    }

    fifo->size ++;
}

char* fifo_pop(jobs_fifo* fifo){
    char* res;
    if (is_fifo_empty(fifo)){
        printf("Tried to pop from an empty FIFO, popping NULL");
        res = NULL;
        return res;
    }
    res = fifo->jobs[fifo->rd_ptr];

    //wrap -around
    if (fifo->rd_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->rd_ptr = 0;
    }
    else {
        fifo->rd_ptr ++;
    }
    fifo->size --;
    return res;
}

void init_fifo(jobs_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        fifo->jobs[i] = malloc(1024*sizeof(char*)); //mem alloc to each job, note that max_line_width = 1024
    }
    fifo->size = 0;
    fifo->rd_ptr = 0;
    fifo->wr_ptr = 0;
}

void free_fifo(jobs_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        free(fifo->jobs[i]) ;
    }
}

// time fifo functions
bool is_time_fifo_empty(time_fifo* fifo){
    if (fifo ->size == 0){
        return true;
    }
    return false;
}

bool is_time_fifo_full(time_fifo* fifo){
    if (fifo -> size == NUM_OF_OUTSTANDING){
        return true;
    }
    return false;
}

void time_fifo_push(time_fifo* fifo, long long inserted_time){
    //FIFO is full case
    if (is_time_fifo_full(fifo)){
        printf("Tried to push to a full FIFO, push aborted, job lost");
        return;
    }

    //FIFO is not full case
    fifo->read_time[fifo->wr_ptr] = inserted_time;
    //wrap around
    if (fifo->wr_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->wr_ptr = 0;
    }
    else {
        fifo->wr_ptr ++;
    }

    fifo->size ++;
}

long long time_fifo_pop(time_fifo* fifo){
    long long res;
    if (is_time_fifo_empty(fifo)){
        printf("Tried to pop from an empty FIFO, popping NULL");
    }
    res = fifo->read_time[fifo->rd_ptr];
    //wrap -around
    if (fifo->rd_ptr == NUM_OF_OUTSTANDING - 1){
        fifo->rd_ptr = 0;
    }
    else {
        fifo->rd_ptr ++;
    }
    fifo->size --;
    return res;
}

void init_time_fifo(time_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        fifo->read_time[i] = 0;
    }
    fifo->size = 0;
    fifo->rd_ptr = 0;
    fifo->wr_ptr = 0;
}

int counter_semicolon(char *command)
{
    int counter = 0;
    char* ptr = command;
    int i = 0;

    while (ptr[i] != '\0') {
        if (ptr[i] == ';') counter++;
        i++;
    }
    return counter;
}

void parse_command(const char *command, char** parsed_command_line,int num_of_basic_commands) {

    //Allocate mem for copy
    char *command_line_copy = (char*)malloc((strlen(command) + 1) * sizeof(char));
    if (command_line_copy == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for command copy.\n");
        exit(1);
    }
    strcpy(command_line_copy, command);

    int index = 0;
    char *saveptr;
    char *token = strtok_r(command_line_copy, ";", &saveptr);

    while (index < num_of_basic_commands && token != NULL) {
    parsed_command_line[index] = (char*)malloc((strlen(token) + 1) * sizeof(char)); //
    if (parsed_command_line[index] == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for job %d.\n", index);
        free(command_line_copy);
        for (int j = 0; j < index; j++) {
            free(parsed_command_line[j]);
            parsed_command_line[j] = NULL;
        }
        exit(1);
    }

    strcpy(parsed_command_line[index], token);
    index++;
    token = strtok_r(NULL, ";", &saveptr);

    }
    free(command_line_copy);
    command_line_copy = NULL;
    return;
}

// Free the allocated memory for argv
void free_parsed_command(char** parsed_command_line, int num_of_basic_commands) {
    for (int i = 1; i < num_of_basic_commands; i++) {
        free(parsed_command_line[i]);
    }
    free (parsed_command_line);
    return;
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

void repeat (int start_index, char** parsed_command_line, int num_of_basic_commands, int times)
{

    //Repeat basic comands after "repeat" x timex
    for (int j=0;j<times;j++){
        for (int i = start_index+1; i < num_of_basic_commands; i++){

            char* command_copy = (char*) malloc ((strlen(parsed_command_line[i])+1)*sizeof(char));
            if (command_copy == NULL)
            {
                fprintf(stderr, "Error: Memory allocation failed for job %d.\n",i);
                free (command_copy);
                exit(1);
            }

            //Copy
            strcpy(command_copy,parsed_command_line[i]);

            //Parse by " "
            char *saveptr;
            char* command_token = strtok_r(command_copy, " ",&saveptr);
            if (command_token == NULL)
            {
                fprintf(stderr, "Error: strtok failed for command %d :%s\n",i,parsed_command_line[i]);
                free (command_copy);
                exit(1);
            }

            if (strcmp(command_token, "increment") == 0){
                command_token = strtok_r(NULL, " ",&saveptr);
                char file_name[COUNTER_FILE_NAME_WIDTH];
                int counter_number = (int) strtol(command_token,NULL, 10);
                make_file_name(command_token, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok_r(NULL, " ",&saveptr);
                char file_name[COUNTER_FILE_NAME_WIDTH];
                int counter_number = strtol(x, NULL, 10);
                make_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok_r(NULL, " ",&saveptr);
                int x_sleep = (int)strtol(x, NULL, 10);
                usleep(x_sleep*1000);
            }
            free(command_copy);
            command_copy = NULL;
        }//end of for i
    }//end of for j
}

void make_file_name(char* command_x, char filename[COUNTER_FILE_NAME_WIDTH])
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
    time_fifo* time_fifo = wdata->time_fifo;
    int whoami = wdata->thread_number;
    time_t* start_known_to_w = wdata->start_time_ptr;
    int log_enabled = wdata->log_enabled;
    char* end_ptr = NULL; // string for strtol usage
    //init log file

    while (1)
    {
        if (!running_flag) {
        break;
        }

        //Check if FIFO is empty
        pthread_mutex_lock(&fifo_mutex);
        while (is_fifo_empty(fifo))
        {

            if (!running_flag) {
                pthread_mutex_unlock(&fifo_mutex);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&wake_up_worker,&fifo_mutex);// Wait for wake-up call from dispatcher
        }
        pthread_mutex_unlock(&fifo_mutex);

        // Try to pop job from FIFO
        pthread_mutex_lock(&fifo_mutex); //MUTEX LOCK
        char* command_line = fifo_pop(fifo);
        long long read_time = time_fifo_pop(time_fifo);
        pthread_mutex_unlock(&fifo_mutex);

        //Handle awake workers counter - increment
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers ++;
        pthread_mutex_unlock(&num_awake_workers_mutex);

        //Mem alloc for parsed command line
        int num_of_semicolons = counter_semicolon(command_line);
        char **parsed_command_line =(char**) malloc((num_of_semicolons + 1)*sizeof(char*)); //num_of_semicolons*sizeof(char*)
        if (parsed_command_line == NULL)
        {
            fprintf(stderr, "Error: Memory allocation failed for parsed command line\n");
            free (parsed_command_line);
            exit(1);
        }
        //Init parsed_command_line
        for (int i = 0; i < num_of_semicolons; i++)
        {
            parsed_command_line[i] = NULL;
        }

        //Parse command line into parsed command line, separate by ;
        int num_of_basic_commands = num_of_semicolons +1;
        parse_command(command_line,parsed_command_line, num_of_basic_commands);

        //Start point for job-execution time
        time_t job_started_time;
        job_started_time = clock();
        long long job_start_elapsed_time = ((long long)(job_started_time - *start_known_to_w) *1000) / CLOCKS_PER_SEC;
        // Open the worker log file
        char worker_log_file_name[20];
        sprintf(worker_log_file_name, "thread%04d.txt", whoami); // Zero-padded to 4 digits
        FILE* thread_log_file;
        if (log_enabled) {
            thread_log_file = fopen(worker_log_file_name, "a");
            if (thread_log_file == NULL) {
                printf("worker %d failed to open his log file with path %s", whoami, worker_log_file_name);
                exit(1);
            }
            fprintf(thread_log_file, "TIME %lld: START job %s\n", job_start_elapsed_time, command_line);
            fclose(thread_log_file);
        }

        // Job execution
        execute_command_line(num_of_basic_commands,parsed_command_line);

        //Stop point for job-execution time
        time_t job_ended_time = clock();
        long long job_end_elapsed_time = ((long long)(job_ended_time - *start_known_to_w) *1000) / CLOCKS_PER_SEC;
        if (log_enabled) {
            thread_log_file = fopen(worker_log_file_name, "a");
            if (thread_log_file == NULL) {
                printf("worker %d failed to open his log file with path %s", whoami, worker_log_file_name);
                exit(1);
            }
            fprintf(thread_log_file, "TIME %lld: END job %s\n", job_end_elapsed_time, command_line);
        //Free all allocated memory - parsed_command_line
            fclose(thread_log_file);
        }
        free_parsed_command(parsed_command_line,num_of_basic_commands); //FIXME - free each parsed_command_line [i]

        //Log and statistics handling
        pthread_mutex_lock(&global_time_vars_mutex);
        long long delta_worktime = ((long long)(job_end_elapsed_time - read_time) *1000) / CLOCKS_PER_SEC;
        jobs_turnaround_time += delta_worktime;
        if (delta_worktime < jobs_mintime) {
            jobs_mintime = delta_worktime;
        }
        if (delta_worktime > jobs_maxtime) {
            jobs_maxtime = delta_worktime;
        }
        number_of_jobs++;
        pthread_mutex_unlock(&global_time_vars_mutex);

        pthread_mutex_unlock(&global_time_vars_mutex);

        //Handle awake workers counter - decrement + wake-up call to dispatcher
        pthread_mutex_lock(&num_awake_workers_mutex);
        num_awake_workers --;
        if (num_awake_workers == 0) pthread_cond_signal(&wake_up_dispatcher); //wake up dispatcher if the last worker done his job
        pthread_mutex_unlock(&num_awake_workers_mutex);

    } // end of while(1)
    return 0;

}

void execute_command_line(int num_of_basic_commands, char** parsed_command_line ){
    for (int i = 0; i < num_of_basic_commands; i++){

            char* command_copy = (char*) malloc ((strlen(parsed_command_line[i])+1)*sizeof(char));
            if (command_copy == NULL)
            {
                fprintf(stderr, "Error: Memory allocation failed for job %d.\n",i);
                free (command_copy);
                exit(1);
            }

            //Copy
            strcpy(command_copy,parsed_command_line[i]);

            //Parse by " "
            char *saveptr;
            char* command_token = strtok_r(command_copy, " ",&saveptr);
            if (command_token == NULL)
            {
                fprintf(stderr, "Error: strtok failed for command %d :%s\n",i,parsed_command_line[i]);
                free (command_copy);
                exit(1);
            }

            if (strcmp(command_token, "increment") == 0){
                command_token = strtok_r(NULL, " ",&saveptr);
                char file_name[COUNTER_FILE_NAME_WIDTH];
                int counter_number = (int) strtol(command_token,NULL, 10);
                make_file_name(command_token, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok_r(NULL, " ",&saveptr);
                char file_name[COUNTER_FILE_NAME_WIDTH];
                int counter_number = strtol(x, NULL, 10);
                make_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok_r(NULL, " ",&saveptr);
                int x_sleep = (int)strtol(x, NULL, 10);
                usleep(x_sleep*1000);
            }
            if (strcmp(command_token, "repeat") == 0)
            {
                char* x = strtok_r(NULL, " ",&saveptr);
                int x_repeat = (int)strtol(x,NULL, 10);
                repeat(i, parsed_command_line, num_of_basic_commands, x_repeat);
                break;
            }
            free(command_copy);
            command_copy = NULL;
        }
}


//Dispatcher code - main function

int main(int argc, char* argv[]) {
    //Mutexes initialization
    init_mutexes();
    //Start point of the timer
    time_t start_time;
    start_time = clock();
    //Initialize the dispatcher
    jobs_fifo fifo;
    init_fifo(&fifo);
    time_fifo time_fifo;
    init_time_fifo(&time_fifo);
    printf("FIFO has been initialized\n");
    bool full = true;
    bool empty = false;
    worker_data thread_data[MAX_NUM_OF_THREADS];
    int sleep_time =0;

    //Analyze the command line arguments
    if (argc != 5) {
        fprintf(stderr, "Error: wrong number of arguments.\n");
        exit(1);
    }

    int num_counters = (int) strtol(argv[3],NULL,10);
    int num_threads = (int) strtol(argv[2],NULL,10);
    int log_enabled = (int) strtol(argv[4],NULL,10);

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
    FILE* worker_logs[MAX_NUM_OF_THREADS];
    //Disp log file
    FILE* dispatcher_log_file = NULL;
    // If log enabled we will create a disp log file here
    if (log_enabled) {
        dispatcher_log_file = fopen("dispatcher.txt", "w");
        if (dispatcher_log_file == NULL) {
            printf("Failed to create dispatcher log file, exiting main.\n");
            exit(1);
        }
    }
    if (dispatcher_log_file != NULL) {
        fclose(dispatcher_log_file);
    }
    for (int i = 0; i < num_threads; i++)
    {
        if (log_enabled) {
            char worker_log_file_name[20]; // FIXME - check it - char* or char
            sprintf(worker_log_file_name, "thread%04d.txt", i); // Zero-padded to 4 digits
            worker_logs[i] = fopen(worker_log_file_name, "w");
            if (worker_logs[i] == NULL) {
                printf("Failed to open a thread log file\n");
                exit(1);
            }
            fclose(worker_logs[i]);
        }
        thread_data[i].counters_fpp = counters_fp;
        thread_data[i].fifo = &fifo;
        thread_data[i].time_fifo = &time_fifo;
        thread_data[i].thread_number = i;
        thread_data[i].start_time_ptr = &start_time;
        thread_data[i].log_enabled = log_enabled;
        pthread_create(&workers[i], NULL, worker_main,(void *)&thread_data[i]);

    }
    ////////////////////////////////////////////////////
    //////////////////Run Dispatcher////////////////////
    ////////////////////////////////////////////////////

    //Read the jobs from the cmdfile
    char job[MAX_LINE_WIDTH];
    while (fgets(job,MAX_LINE_WIDTH,cmd_file_fp)) {
        time_t job_read_time = clock();
        long long job_read_time_long = (long long)(job_read_time/CLOCKS_PER_SEC)*1000;
        if (log_enabled) {
            dispatcher_log_file = fopen("dispatcher.txt", "a");
            if (dispatcher_log_file == NULL) {
                printf("Failed to open dispatcher log file, exiting main.\n");
                exit(1);
            }
            fprintf(dispatcher_log_file, "TIME %lld: read cmd line: %s\n", job_read_time_long, job);
            fclose(dispatcher_log_file);
        }
        char* saveptr;
        // Remove the trailing newline character, if any
        char* new_line = strchr(job,'\n');
        //FIXME - correct it according to possible input
        if (new_line){
            *new_line = '\0';
        }
        // Trim leading and trailing whitespace
        char* trimmed_job = job;
        while(isspace((unsigned char)*trimmed_job))
        {
            trimmed_job++;
        }
         char* end = trimmed_job + strlen(trimmed_job) - 1;
        while (end > trimmed_job && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
        // Skip empty or whitespace-only lines
        if (*trimmed_job == '\0') {
            continue;
        }
        if (strcmp(job, "\n") == 0) continue; //empty line = "\n"
        //Separate between worker and dispathcer job
        char* token = strtok_r(trimmed_job," ",&saveptr); // FIXME - check with Gadi, IGOR
        if (token == NULL) {
            fprintf(stderr, "Error:strtok failed.\n");
            exit(1);
        }
        printf("Processing token: %s\n", token); //DEBUG
        //Dispatcher code
        if (strcmp(token,"dispatcher") == 0)
        {
            token = strtok_r(NULL," ",&saveptr);
            if (token == NULL) {
                fprintf(stderr, "Error: missing command after 'dispatcher'.\n");
                exit(1);
            }

            if (strcmp(token,"_wait") == 0)
            {
                pthread_mutex_lock(&num_awake_workers_mutex); // FIFO MUTEX LOCK
                while (num_awake_workers>0)
                {
                    pthread_cond_wait(&wake_up_dispatcher,&num_awake_workers_mutex);
                }
                pthread_mutex_unlock(&num_awake_workers_mutex); // FIFO MUTEX UNLOCK
            }

            if (strcmp(token,"_sleep") == 0)
            {
                token = strtok_r(NULL," ",&saveptr);// fetch the X var - time for sleep
                if (token == NULL) {
                    fprintf(stderr, "Error: missing sleep time after 'sleep'.\n");
                    exit(1);
                }
                sleep_time = (int) strtol(token,NULL,10); //sleeptime in miliseconds
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
            time_fifo_push(&time_fifo, job_read_time_long);
            pthread_cond_signal(&wake_up_worker); // wake up call for all threads
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
            pthread_cond_broadcast(&wake_up_worker); // wake up any thread
            pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
        }

        //Termination of worker loop
        running_flag = 0;

        //wake up all workers that waiting for cond war to exit
        pthread_mutex_lock(&fifo_mutex); //MUTEX LOCK
        pthread_cond_broadcast(&wake_up_worker);
        pthread_mutex_unlock(&fifo_mutex); //MUTEX UNLOCK

        //Wait for all workers to finish
        for (int i = 0; i < num_threads; i++)
        {
            pthread_join(workers[i],NULL);
        }

        //Destroy dynamic initiated mutexes
        destroy_mutexes();

        //Free counters names array
        for (int i = 0; i < num_counters; i++) {
            if (counters_names[i] != NULL){
                free(counters_names[i]);
                counters_names[i] = NULL;}

        }
    //Destroy dynamic initiated mutexes
    destroy_mutexes();

    //Free counters names array
    for (int i = 0; i < num_counters; i++) {
    if (counters_names[i] != NULL){
    free(counters_names[i]);
    counters_names[i] = NULL;}
    }

        //Free FIFO
        free_fifo(&fifo);

        //Statistics
        pthread_mutex_lock(&global_time_vars_mutex);
        FILE* stat_file;
        stat_file = fopen("stats.txt", "w");
        if (stat_file == NULL) {
            printf("Stat file tereminate\n");
            exit(1);
        }
        time_t end_main_time;
        end_main_time = clock();
        long long end_time_elapsed = (end_main_time - start_time)/CLOCKS_PER_SEC * 1000;
        float avg_job_time = jobs_turnaround_time/number_of_jobs;
        fprintf(stat_file,"total running time: %lld milliseconds\n",end_time_elapsed);
        fprintf(stat_file,"sum of jobs turnaround time: %lld milliseconds\n",jobs_turnaround_time);
        fprintf(stat_file, "min job turnaround time: %lld milliseconds\n", jobs_mintime);
        fprintf(stat_file, "average job turnaround time: %f milliseconds\n", avg_job_time);
        fprintf(stat_file,"max job turnaround time: %lld milliseconds\n", jobs_maxtime);
        fclose(stat_file);
        pthread_mutex_unlock(&global_time_vars_mutex);
        return 0;

}
