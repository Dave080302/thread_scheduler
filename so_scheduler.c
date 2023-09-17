#include <stdio.h>
#include <pthread.h>
#include "so_scheduler.h"
#include <unistd.h>
#include <stdlib.h>
#define NEW 0
#define READY 1
#define RUNNING 2
#define WAITING 3
#define TERMINATED 4
typedef struct th
{
    int state;
    int prio;
    int time_slice;
    int waiting_on;
    tid_t id;
    so_handler *handler;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} so_thread;
typedef struct Queue
{
    struct Queue *next;
    so_thread* t;
}*PrioQueue;
typedef struct sched{
    int io;
    int time_slice;
    int started;
    PrioQueue queue;
    so_thread* running;
    int threads_no;
    so_thread** threads;
}scheduler;
static scheduler s;
int isEmptyQueue(void){
    if(s.queue == NULL)
        return 1; 
    return 0;
}
void enqueue(so_thread *new){
    new->state = READY;
    PrioQueue newelem = malloc(sizeof(struct Queue));
    newelem->t = new;    
    newelem->next = NULL;
    if(isEmptyQueue()){
        s.queue = newelem; /// daca avem o coada vida, capul(head == s.queue) devine thread-ul adaugat in coada
        return;
    }
    PrioQueue copy = s.queue;
    if(newelem->t->prio > copy->t->prio){
        newelem->next=copy;
        s.queue=newelem; /// daca prioritatea este mai mare decat capul cozii, 
    }
    else{
    while(copy->next != NULL && copy->next->t->prio >= new->prio)
        copy=copy->next; /// cautam locul in coada al elementului
    if(copy->next!=NULL){
        newelem->next = copy->next;
        copy->next = newelem; /// daca il gasim, il pozitionam acolo, daca nu, la final
    }
    else copy->next = newelem;
    }
}
void dequeue(void){
    if(s.queue->next != NULL){
        PrioQueue freed_object = s.queue;
        s.queue = s.queue->next;
        freed_object->next = NULL; /// daca avem peste 2 elemente, eliberam capul cozii si punem pointer-ul
        free(freed_object); /// catre cap la cel de-al 2 lea element
    }
    else{
        free(s.queue);
        s.queue=NULL;
    }
}
so_thread* first_elem(){
    return s.queue->t; /// thread-ul aflat in capul cozii
}
void context_switch(){
    so_thread *old_thread = NULL;
    int preempted = 0;
    if(s.running !=NULL){
        old_thread = s.running; 
        if(s.running->time_slice <= 0)
            s.running->time_slice = s.time_slice;/// reinstauram cuanta thread-ului din running daca aceasta a expirat
        if(!isEmptyQueue()){
            so_thread *new_running_thread;
            so_thread *next_in_queue = first_elem();
            if(s.running->state == RUNNING){ /// daca thread-ul din running nu si-a terminat executia, il comparam cu primul thread
                if(s.running->prio > next_in_queue->prio)  /// din coada ca sa stabilim care ar trebui sa ruleze primul
                    new_running_thread = s.running;
                else{
                    new_running_thread = next_in_queue;
                    dequeue();
                    enqueue(s.running); /// daca primul thread din coada are prioritate mai mare, il rulam pe acesta si 
                    preempted = 1; /// adaugam thread-ul din running inapoi in coada
                }

            }
            else{
                new_running_thread = next_in_queue;
                dequeue(); /// daca thread-ul si-a terminat executia sau este in WAITING, este preemptat si 
                preempted = 1; /// nu il readaugam in coada
            }
            new_running_thread->state = RUNNING;
            s.running = new_running_thread;
        }
    }
    else{
        if(!isEmptyQueue()){
        so_thread *new_running_thread = first_elem();
        dequeue();
        new_running_thread->state = RUNNING; /// in cazul in care nu ruleaza nici un thread, rulam
        s.running = new_running_thread; /// primul thread din coada
        }
        else
            return;
    }
    pthread_mutex_lock(&s.running->lock);
    pthread_cond_signal(&s.running->cond); /// rulam thread-ul
    pthread_mutex_unlock(&s.running->lock);
    if(preempted == 1 && old_thread->state!= TERMINATED){
        pthread_mutex_lock(&old_thread->lock);
        pthread_cond_wait(&old_thread->cond, &old_thread->lock); /* daca vechiul thread a fost preemptat*/ 
        pthread_mutex_unlock(&old_thread->lock);  /*si mai are de rulat instructiuni, atunci ii blocam executia*/
    }
}
int so_init(unsigned int time_quantum, unsigned int io)
{
    
    if(s.started ==1)
        return -1;
    if (io < 0 || io > SO_MAX_NUM_EVENTS)
        return -1;
    if (time_quantum <= 0)
        return -1;
    s.threads_no = 0;
    s.io = io;
    s.time_slice = time_quantum;
    s.started = 1;
    s.running = NULL;
    s.threads = malloc(1000 * sizeof(so_thread*));
    return 0;
}
void* thread_start(void* arg){
    so_thread *thread = (so_thread*)arg;
    pthread_mutex_lock(&thread->lock);
    pthread_cond_signal(&thread->cond); /// anuntam so_fork ca isi poate relua executia
    pthread_mutex_unlock(&thread->lock);
    pthread_mutex_lock(&thread->lock);
    pthread_cond_wait(&thread->cond, &thread->lock); /// asteptam ca thread-ul sa intre in running
    pthread_mutex_unlock(&thread->lock);
    thread->handler(thread->prio);
    thread->state = TERMINATED;
    context_switch();
    return NULL;
}
tid_t so_fork(so_handler *func, unsigned int priority)
{
    if(func == NULL || priority > SO_MAX_PRIO)
        return INVALID_TID;
    so_thread *new;
    new = malloc(sizeof(so_thread));
    new->handler = func;
    new->id = INVALID_TID;
    new->prio = priority;
    new->state = NEW;
    new->waiting_on = -1;
    new->time_slice = s.time_slice;
    pthread_mutex_init(&new->lock, NULL);
    pthread_cond_init(&new->cond, NULL);
    pthread_create(&new->id, NULL , (void*) thread_start, (void*) new);
    pthread_mutex_lock(&new->lock);
    pthread_cond_wait(&new->cond,&new->lock); /* asteptam sa inceapa executia thread_start pana sa */
    pthread_mutex_unlock(&new->lock); /* continuam cu executia so_fork*/
    s.threads[s.threads_no++] = new;
    enqueue(new);
    if(s.running!=NULL){
        s.running->time_slice--;
        if(priority > s.running->prio || s.running->time_slice <=0 || s.running->state == TERMINATED)
            context_switch();
    }
    else
        context_switch();
    return new->id;
}
int so_wait(unsigned int io)
{
    if(io >= s.io || s.running == NULL)
        return -1;
    s.running->time_slice--;
    s.running->state = WAITING;
    s.running->waiting_on = io;
    context_switch(); /// va trebui sa rulam un alt thread
    return 0;
}
int so_signal(unsigned int io)
{
    int max_prio = -1;
    int nr_wakes = 0;
    if(io >= s.io)
        return -1;
    for(int i=0; i< s.threads_no ; i++){
        if(s.threads[i]->state == WAITING && s.threads[i]->waiting_on == io){
            nr_wakes++;
            enqueue(s.threads[i]);
            s.threads[i]->waiting_on = -1;
            if(s.threads[i]->prio > max_prio)
                max_prio = s.threads[i]->prio;
    }
    }
    if(s.running != NULL){
        s.running->time_slice--;
        if(max_prio > s.running->prio || s.running->time_slice <= 0 || s.running->state == TERMINATED) /// verificam
                 context_switch(); /// daca este nevoie sa rulam un alt thread
    }
        else
            context_switch();
    
    return nr_wakes;
}
void so_exec(void)
{
    s.running->time_slice--; /// executam o instructiune
    if(s.running->time_slice <= 0)
        context_switch();
}
void so_end(void)
{
    s.started = 0;
    for(int i=0; i < s.threads_no; i++){
        pthread_join(s.threads[i]->id, NULL);
        pthread_cond_destroy(&s.threads[i]->cond);
        pthread_mutex_destroy(&s.threads[i]->lock);
    }
    for(int i=0 ; i< s.threads_no; i++)
        free(s.threads[i]);
    free(s.threads);
}
