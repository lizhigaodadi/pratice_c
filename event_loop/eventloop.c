#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <strings.h>

#define MAX_EVENTS 10 

typedef void (*callback_t)(void *arg);

typedef struct event {
    int fd;
    int events;
    callback_t callback;
    void *arg;  //arg for callback
} event_t;

struct timer {
    long long expires;  // when will execute 
    callback_t callback;
    void *arg;
};

struct event_loop {  //event loop
    int epfd;  //epoll_fd
    struct epoll_event *epoll_events;  // epoll_events array
    int epoll_event_size;
    struct event *events;
    int event_count;

    //timer events
    struct timer *timers;
    int timer_count;
};




int event_loop_init(struct event_loop *loop) {  
    bzero(loop, sizeof(struct event_loop));

    loop->epfd = epoll_create(MAX_EVENTS);
    if (loop->epfd < 0) {
        perror("epoll_create failed");
        return -1;
    }

    loop->epoll_events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    if (loop->epoll_events == NULL) {
        perror("malloc failed");
        return -2;
    }

    loop->epoll_event_size = 0;
    loop->events = NULL;
    loop->event_count = 0;
    loop->timers = NULL;
    loop->timer_count =0;

    return 0;
}


//get current time 
long long get_current_time() {
    struct timeval tv;
    gettimeofday(&tv,NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void delete_timer(struct event_loop *loop,int target_index) {
    int i;

    //delete timer
    for (i = target_index; i < loop->timer_count - 1;i++ ) {
        loop->timers[i] = loop->timers[i + 1];
    }

    //realloc timers list memory
    loop->timers = realloc(loop->timers,--loop->timer_count * sizeof(struct timer));
    if (loop->timers == NULL) {
        perror("realloc failed");
    }
}


void handle_timer(struct event_loop *loop) {  //handle timer
    int i;
    long long current_time = get_current_time();
    for (i = 0;i < loop->timer_count;i++) { //handle all the all need to handle timers
        if (loop->timers[i].expires <= current_time) { //find the timer
            loop->timers[i].callback(loop->timers[i].arg);

            //delete the timer from loop
            delete_timer(loop,i); //delete the timer from loop
            i--;
        }
    }
}

void handle_event(struct event_loop *loop) {  //handle event
    int i;
    event_t *event;
    for (i = 0;i < loop->event_count;i++) {
        *event = loop->events[i];
        if (event->callback) {
            event->callback(event->arg);
        }
    }
}

int event_loop_add(struct event_loop *loop,int fd,short event,callback_t cb,void *arg) {
    //create epoll_event
    struct epoll_event *ev = malloc(sizeof(struct epoll_event));
    ev->events = event;
    //create event;
    struct event *e = malloc(sizeof(struct event));
    e->fd = fd;
    e->callback = cb;
    e->arg = arg;
    e->events = event;
    ev->data.ptr = e;

    //add
    if (epoll_ctl(loop->epfd,EPOLL_CTL_ADD,fd,ev)) {
        perror("epoll_ctl: add");
        return -1;
    }

    return 0;

}

int event_loop_del(struct event_loop *loop,int fd) {
    struct epoll_event event;
    epoll_ctl(loop->epfd,EPOLL_CTL_DEL,fd,&event);
}


void event_loop_destroy(struct event_loop *loop) {
    
    if (loop->epfd != -1) {
        close(loop->epfd);
    }
    free(loop->epoll_events);
    free(loop->events);
    if (loop->timer_count != 0) {
        free(loop->timers);
    }

}


void event_loop_work(struct event_loop *loop) {  //event_loop work function
    int nfds;
    int i;

    for (;;) {  //handle events
        //wait for event
        nfds = epoll_wait(loop->epfd,loop->epoll_events,MAX_EVENTS,-1);

        if (nfds == -1) {
            perror("epoll_wait failed");
            continue;
        }


        for (i = 0;i<nfds;i++) {  //find these epoll_events
            struct epoll_event ep_event = loop->epoll_events[i];
            // create event for this epoll_event
            if (ep_event.events & EPOLLIN) {  //this is a read event
                //add event
                struct event *p_event = ep_event.data.ptr;
                loop->events[loop->event_count++] = *p_event;
            }
        }

        handle_event(loop);
        handle_timer(loop);
    }
}


//add a timer to the event loop
int add_timer(struct event_loop *loop,callback_t cb,void *arg,long long offset_time) {
    //realocate the timer memory
    loop->timers = realloc(loop->timers,sizeof(struct timer) * ++loop->timer_count);


    (loop->timers + loop->timer_count - 1)->callback = cb;
    (loop->timers + loop->timer_count - 1)->arg = arg;
    (loop->timers + loop->timer_count - 1)->expires = offset_time + get_current_time();


    return 0;
}



int add_event(struct event_loop *loop,struct epoll_event *ep_event) {

    //check our if over 
    if (loop->epoll_event_size >= MAX_EVENTS) {
        perror ("too many events\n");
        return -1;
    }

    loop->epoll_events[loop->epoll_event_size++] = *ep_event;

    return 0;
}

//test
int main(int argc, char **argv) {
    struct event_loop loop;
    event_loop_init(&loop);
    printf("hello world\n");
    event_loop_destroy(&loop);
    return 0;
}