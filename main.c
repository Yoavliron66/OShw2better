#include "hw2.h"


//MUTEXES
static int is_running = 1;
pthread_mutex_t fifo_mutex = PTHREAD_COND_INITIALIZER;
pthread_mutex_t counter_mutex[MAX_NUM_OF_COUNTERS];
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;

//COND VAR
pthread_cond_t wake_up = PTHREAD_COND_INITIALIZER;

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
    for (int j=0;j<times;j++)
    {
       for (int i = start_command + 1; i < count_commands; i++){
            char* command_token = strtok(job[i], " ");
            if (strcmp(command_token, "increment") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, NULL, 10);
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, NULL, 10);
                counter_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_sleep = (int)strtol(x, NULL, 10);
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


void* worker_main(void *data)
{
    worker_data* wdata = (worker_data*)data;
    FILE** counters = wdata->counters_fpp;
    jobs_fifo* fifo = wdata->fifo;
    int whoami = wdata->thread_number;
    time_t* start_known_to_w = wdata->start_time_ptr;
    while (1)
    {
        pthread_mutex_lock(&running_mutex);
        if (!is_running)
        {
            pthread_mutex_unlock(&running_mutex);
            break;
        }
        pthread_mutex_unlock(&running_mutex);
        pthread_mutex_lock(&fifo_mutex);
        while (is_fifo_empty(fifo))
        {
            pthread_cond_wait(&wake_up,&fifo_mutex);
        }
        char* command = fifo_pop(fifo);
        pthread_mutex_unlock(&fifo_mutex);
        char **job = NULL;
        int count_commands = parse_command(command,job);

        // Job exec
        for (int i = 0; i < count_commands; i++){
            char* command_token = strtok(job[i], " ");
            if (strcmp(command_token, "increment") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, NULL, 10);
                counter_file_name(x, file_name);
                update_counter_file(file_name, 1,counter_number);
            }
            if (strcmp(command_token, "decrement") == 0){
                char* x = strtok(NULL, " ");
                char file_name[12] = "counterxx.txt";
                int counter_number = strtol(x, NULL, 10);
                counter_file_name(x, file_name);
                update_counter_file(file_name, -1,counter_number);
            }
            if (strcmp(command_token, "msleep") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_sleep = (int)strtol(x, NULL, 10);
                usleep(x_sleep*1000);
            }
            if (strcmp(command_token, "repeat") == 0)
            {
                char* x = strtok(NULL, " ");
                int x_repeat = (int)strtol(x, NULL, 10);
                repeat(i, job, count_commands, x_repeat);
                break;
            }
        }
        free_parsed_command(job,count_commands);
        // FIX ME - IF dispatcher wake some threads or all threads after pushing to FIFO the next line is not needed
        pthread_cond_broadcast(&wake_up);
    }

}


//Dispatcher code

int main(int argc, char* argv[])
{
    //Start of the timer
    time_t start_time;
    //Initialize the dispatcher
    jobs_fifo fifo;
    bool full = false;
    bool empty = false;
    worker_data thread_data[MAX_NUM_OF_THREADS];

    //Analyze the command line arguments
    assert((argc != NUM_OF_CMD_LINE_ARGS));

    int num_counters = (int) strtol(argv[3],'\0',10);
    int num_threads = (int) strtol(argv[2],'\0',10);
    FILE* cmd_file_fp = fopen(argv[1],"r");
    assert (cmd_file_fp);
    int log_enabled = (int) strtol(argv[4],'\0',10);

    //Create counters - creates num_counts of counters as txt file, each initialized to 0
    char* counters_names [MAX_NUM_OF_COUNTERS];
    for (int i = 0; i < num_counters; i++)
    {
        sprintf(counters_names,"counter%2d.txt",i);
    }

    FILE* counters_fp [MAX_NUM_OF_COUNTERS];
    for (int i = 0; i < num_counters; i++)
    {
        counters_fp[i] = fopen(counters_names[i],"w");
        assert (counters_fp[i]);
        fprintf(counters_fp[i],"%d",0); //FIXME - check it
        pthread_mutex_init(&counter_mutex[i],NULL);
        fclose(counters_fp[i]);
    }

    //Create threads
    pthread_t workers[MAX_NUM_OF_THREADS];
    //Create threads log files
    FILE* worker_logs[MAX_NUM_OF_THREADS];


    for (int i = 0; i < num_threads; i++)
    {
        char worker_log_file_name[20];
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
    if (strcmp(token,"dispatcher"))
    {
        //dispatcher sleep + dispatcher wait implementation

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

            pthread_cond_signal(&wake_up); // wake up any thread
            pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
        }

        //push job to the FIFO
        pthread_mutex_lock(&fifo_mutex); // FIFO MUTEX LOCK
        fifo_push(&fifo,job+strlen(token)+1); // job pointer incremented by len of worker + space
        pthread_cond_signal(&wake_up); // wake up any thread
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
        pthread_cond_signal(&wake_up); // wake up any thread
        pthread_mutex_unlock(&fifo_mutex); // FIFO MUTEX UNLOCK
    }

    //Wait for all workers to finish
    for (int i = 0; i < num_threads; i++)
        {
            ptherad_join(workers[i]);
        }

    //Statistics

    //Logs

    }