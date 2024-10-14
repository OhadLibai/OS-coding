#include <stdlib.h>
#include <threads.h>

#include "queue.h"


// Wrapping the content of the enq and deq
typedef struct ElmCell {
    void* elm;
    struct ElmCell* next;
} ElmCell;


// For managing the FIFO requirment of the threads
typedef struct ThreadWaitCell {
    cnd_t CV;   // for registration purposes
    struct ThreadWaitCell* next;
} ThreadWaitCell;


// Queue structure
typedef struct Q {
    ElmCell* elm_head;
    ElmCell* elm_tail;

    ThreadWaitCell* th_wait_head;   // head of waiting threads queue
    ThreadWaitCell* th_wait_tail;   // tail of waiting threads queue

    mtx_t q_lock;   // Ensuring the access to the global q DS is made synchronously

    size_t num_elm;            // Current number of elements in the queue
    size_t visited_count;  // inserted items/elments that were removed 
    size_t waiting_count;    // Number of deq threads waiting 
    
    
} Q;

/* GLOBAL Q */
Q q;

/*-------------------------------------------------------------------------------------------------*/

void initQueue(void) {
    // elem cells
    q.elm_head = NULL;
    q.elm_tail = NULL;

    //th cells
    q.th_wait_head = NULL;
    q.th_wait_tail = NULL;

    q.num_elm = 0;
    q.visited_count = 0;
    q.waiting_count = 0;

    mtx_init(&q.q_lock, mtx_plain); //  mtx_plain - ordinary mtx
}


void destroyQueue(void) {
    mtx_lock(&q.q_lock); // first take control

    // emptying the elements in the q
    ElmCell* curr_elm = q.elm_head;
    while (curr_elm != NULL) {
        ElmCell* temp = curr_elm;
        curr_elm = curr_elm->next;
        free(temp);
    }

    // saying goodbye to the waited threads 
    ThreadWaitCell* curr_waited_th = q.th_wait_head;
    while (curr_waited_th != NULL) {
        ThreadWaitCell* temp = curr_waited_th;
        curr_waited_th = curr_waited_th->next;
        cnd_destroy(&temp->CV); // dear thrd you dont need this anymore
        free(temp);
    }

    mtx_unlock(&q.q_lock);
    mtx_destroy(&q.q_lock); // nobody is waiting for the mutex now
}

/*-------------------------------------------------------------------------------------------------*/

// PRODUCER
void enqueue(void* new_elm) {
    // prepare for insertion, yet to lock
    ElmCell* new_node = (ElmCell*)malloc(sizeof(ElmCell));
    new_node->elm = new_elm;
    new_node->next = NULL;

    mtx_lock(&q.q_lock); // ground zero mode

    if (q.elm_tail == NULL) {
        q.elm_head = new_node;
        q.elm_tail = new_node;
    } 
    else {
        q.elm_tail->next = new_node; // chaining
        q.elm_tail = new_node; // enqueing
    }
    q.num_elm++;

    // waiting threads
    // wake up FIFO manner
    if (q.waiting_count > 0) { 
        ThreadWaitCell* to_wake = q.th_wait_head;
        q.th_wait_head = to_wake->next; // advancing next thrd
        if (q.th_wait_head == NULL) 
            q.th_wait_tail = NULL;
        
        cnd_signal(&to_wake->CV); // wake up lazy thrd
    }

    mtx_unlock(&q.q_lock);
}



// CONSUMER
void* dequeue(void) {
    ThreadWaitCell th_wait_node;
    cnd_init(&th_wait_node.CV); // register
    th_wait_node.next = NULL;

    mtx_lock(&q.q_lock); // take control of the q

    if (q.th_wait_head == NULL) {
        *q.th_wait_head = th_wait_node;
        *q.th_wait_tail = th_wait_node;
    } 
    else {
        q.th_wait_tail->next = &th_wait_node;
        q.th_wait_tail = &th_wait_node;
    }
    q.waiting_count++;

    while (q.num_elm == 0) {
        cnd_wait(&th_wait_node.CV, &q.q_lock); // wait for someone to wake me up in time
                                                // not busy waiting of course
    }
    q.waiting_count--; // this thrd is not waiting anymore
    cnd_destroy(&th_wait_node.CV); // no other thrd waiting for this CV anyway, sterilized cleanup

    // acting on elem queue
    ElmCell* elm_node = q.elm_head;
    void* elm = elm_node->elm; // ret_val
    q.elm_head = q.elm_head->next;
    if (q.elm_head == NULL) {
        q.elm_tail = NULL;
    }
    free(elm_node);

    q.num_elm--;
    q.visited_count++;

    mtx_unlock(&q.q_lock);

    return elm;
}

/*-------------------------------------------------------------------------------------------------*/


bool tryDequeue(void** elm) { // dont register with CV like a deq threads
    if (mtx_trylock(&q.q_lock)!=thrd_success) // try_lock
        return false;

    else { // mtx_trylock succeeded
        bool success;
        if (!(q.num_elm > 0)) 
            success = false;

        else {
            ElmCell* elem_node = q.elm_head;
            *elm = elem_node->elm; // change in-place
            q.elm_head = q.elm_head->next;

            if (q.elm_head == NULL)
                q.elm_tail = NULL;

            free(elem_node);

            q.num_elm--; // we just extracted
            q.visited_count++; // another one passed on the q
            
            success = true;
        }

        mtx_unlock(&q.q_lock);

        return success;
    }
}


/* straighforward, not asked for capturing  locks
 * could have been done with guaranteed atomicity
 */

size_t size(void) {
    return q.num_elm;
}

size_t waiting(void) {
    return q.waiting_count;
}

size_t visited(void) {
    return q.visited_count;
}