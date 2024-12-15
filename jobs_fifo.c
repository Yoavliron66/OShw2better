

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


void free_fifo(jobs_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        if (fifo->jobs[i] == NULL) {
            continue;
        }
        free(fifo->jobs[i]);
    }
}


void init_fifo(jobs_fifo* fifo){
    for (int i = 0; i < NUM_OF_OUTSTANDING; i++){
        fifo->jobs[i] = NULL;
    }
    fifo->size = 0;
    fifo->rd_ptr = 0;
    fifo->wr_ptr = 0;
}
