#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

//my thread pool implementation


typedef struct task {
    void (*func)(void *arg);
    void *arg;
    struct task *next;  // linklist
} task_t;

typedef struct thread_pool {
    pthread_mutex_t mutex; // mutex for the queue
    pthread_cond_t  notify; //notify resources
    task_t *head;
    task_t *tail;
    int tasks_count;  // current number of tasks
    int max_tasks;    // max number of tasks  
    int shutdown;  // 0 shutdown 1 running
    int thread_count;  // thread_count
    pthread_t *threads;  // array of pthread_t

} thread_pool_t;


void *thread_pool_worker(void *arg) {  //
    //constantly check there is any work needed to do
    thread_pool_t *pool = (thread_pool_t *)arg;
    task_t *task;
    printf("thread: %ld into thread_pool_worker\n",pthread_self());
    
    for (;;) { // 
        pthread_mutex_lock(&pool->mutex);

        while (pool->head == NULL && pool->shutdown) {
            pthread_cond_wait(&pool->notify, &pool->mutex); // wait for task
        }


        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }

        //get the task
        task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) {
            pool->tail = NULL;
        }
        printf("hello world\n");

        pool->tasks_count--;

        //unlock
        pthread_mutex_unlock(&pool->mutex);

        //execute the task
        task->func(task->arg);

        //free current task
        free(task);

    }
    

}


int thread_pool_destroy(thread_pool_t *pool) {  // destroy the pool
    int i;
    task_t *task;
    task_t *next;

    pool->shutdown = 1;
    //wait for all the task

    for (i = 0;i<pool->thread_count;i++) {
        pthread_join(pool->threads[i],NULL);
    }

    //free the mutex resource

    pthread_cond_destroy(&pool->notify);
    pthread_mutex_destroy(&pool->mutex);


    //free the last tasks
    task = pool->head;
    while (task != NULL) {
        next = task->next;
        free(task);
        task = next;
    }


    return 0;
}


int thread_pool_add(thread_pool_t *pool,void (*run)(void *arg),void *arg) {  // add our task to the thread_pool
    printf ("ready to add task\n");
    if (pool->tasks_count >= pool->max_tasks) {  // no more tasks
        perror("there is no more tasks can be added\n");
        return -2;  
    }

    task_t *task = malloc(sizeof(task_t));

    if (task == NULL) {
        perror("task_t malloc failed\n");
        return -1;
    }

    task->func = run;
    task->arg = arg;
    task->next = NULL;


    //add to the linked list
    pthread_mutex_lock(&pool->mutex);

    if (pool->head == NULL) { //means there is no task need to do

        pool->head = task;
        pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task; // as new tail
    }

    pool->tasks_count++;
    //notify others 
    if (pool->tasks_count == 1) {  // just notify one thread
        pthread_cond_signal(&pool->notify);
    } else {
        pthread_cond_broadcast(&pool->notify);
    }

    pthread_mutex_unlock(&pool->mutex);
    printf("add a new task successfully\n");
}


int thread_pool_init(thread_pool_t *pool,int max_tasks,int thread_count) {
    int i;
    //init pthread resources
    pthread_mutex_init(&pool->mutex,NULL);
    pthread_cond_init(&pool->notify,NULL);

    pool->max_tasks = max_tasks;
    pool->tasks_count = 0;
    pool->shutdown = 0;
    pool->thread_count = thread_count;

    pool->head = NULL;
    pool->tail = NULL;

    pool->threads = (pthread_t *) malloc (sizeof(pthread_t) * thread_count);

    //create thread reources

    for (i = 0; i < pool->thread_count; i++) {
        pthread_create((pool->threads + i),NULL,thread_pool_worker,(void *)pool);    
    }


    return 0;
}

// test thread pool

void thread_pool_test_func(void *arg) {
    printf("current thread: %ld\n",pthread_self());
}


int main(int argc, char **argv) {
    //create thread_pool
    thread_pool_t pool;
    int i;

    if (thread_pool_init(&pool, 10, 4)!= 0) {
        perror("thread_pool_init failed\n");
        return -1;
    }

    //add tasks
    for (i = 0; i < 8; i++) {
        thread_pool_add(&pool,thread_pool_test_func,NULL);
    }
	
	printf("sleep begin!\n");
	sleep(5);
	printf("sleep end!\n");
    thread_pool_destroy(&pool);

	printf("everything is ok\n");

    //wait for all the tasks

}
