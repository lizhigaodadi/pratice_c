#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


#define DEBUG 1
#define DBG if (DEBUG)

typedef struct task{
    struct task *next;
    void (*func)(void *arg);
    void *arg; // will be executed
} task_t;

int undo_task_count = 0;

task_t *work_task;
task_t *work_task_end; // end of work list
task_t *work_task_head; // head of work list


// init condition
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


void handle_task(void *arg) {
    char *work = (char *)arg;
    printf("working for %s.....\n",work);
}


void *work(void *arg) {
    task_t *cur_task;
    DBG printf("now is work\n");
    // execute work_task
    for (;;) {

        pthread_mutex_lock(&mutex);  // use mutex to protect condition 
        while (work_task == NULL) {   //means there is no task need to do now!
            //go to sleep
            pthread_cond_wait(&cond, &mutex); // wait for others to signal

        }
        // now there are some tasks need to do
        printf("work get the lock\n");

        cur_task = work_task;
        work_task = cur_task->next;
        undo_task_count--;
        pthread_mutex_unlock(&mutex);
        
        printf("thread: %ld ready to do some work\n",pthread_self());
        cur_task->func(cur_task->arg);
        printf("work end! waiting for next!\n");
        
    }
}

  
void process(void ) {  // constantly process work_task
    task_t *cur_task = malloc(sizeof(task_t));
    // init 
    cur_task->next = NULL;
    cur_task->func = handle_task;
    cur_task->arg = (void *)"fuck your mother";

    pthread_mutex_lock(&mutex);  // asure now no others can update this task_list
    DBG printf("process get the lock\n");
    DBG printf("add a new task to task_end\n");
    // add a new task to do
    work_task_end->next = cur_task;
    work_task_end = cur_task;
    undo_task_count++;

    //signal to others to know this message 
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
    
}



void *handle_process(void *arg) {
    DBG printf("now is handle_process\n");
    for (;;) {
        process();
        sleep(2);
        printf("handle_process again! current undo_work_task: %d\n",undo_task_count);
    }
}



int main(int arc,char **argv) {
    pthread_t tid1;
    pthread_t tid2;


    DBG printf("init our resources\n");
    work_task = malloc(sizeof(task_t));
    if (work_task == NULL) {
        perror("malloc work_task failed!\n");
        exit(EXIT_FAILURE);
    }

    work_task_end = work_task; // pointer to the end of the work_task
    work_task->next = NULL;
    work_task->func = handle_task;
    work_task->arg = (void *) "hello world";
    undo_task_count++;

    DBG printf("begin to create threads\n");

    pthread_create(&tid1,NULL,handle_process,NULL);
    pthread_create(&tid2,NULL,work,NULL);

    if (DEBUG) printf("create threads succfully\n");

    //wait to end
    pthread_join(tid1,NULL);
    pthread_join(tid2,NULL);

    task_t *tmp_task;

    // free our memory
    while (work_task_head == NULL) {
        tmp_task = work_task_head->next;
        free(work_task_head);
        work_task_head = tmp_task;
    }
    
    return 0;
}