#include <avr/interrupt.h>
#include "semaphore.h"
#include "scheduler.h"
#include "tcb.h"

int8_t sem_init(Semaphore *sem, uint8_t value) {
    if (sem == NULL) return ERROR;
    sem->value = value;
    sem->wait_queue.first = NULL;
    sem->wait_queue.last = NULL;
    sem->wait_queue.size = 0;
    return OK;
}

int8_t sem_wait(Semaphore *sem) {
    if (sem == NULL) return ERROR;
    cli();
    if (sem->value > 0) {
        sem->value--;
    } else {
        current_tcb->status = Waiting;
        TCBList_enqueue(&sem->wait_queue, current_tcb);
        schedule();
    }
    sei();
    return OK;
}

int8_t sem_post(Semaphore *sem) {
    if (sem == NULL) return ERROR;
    cli();
    if(sem->wait_queue.first != NULL) {
        TCB* waiting_tcb = TCBList_dequeue(&sem->wait_queue);
        waiting_tcb->status = Ready;
        TCBList_enqueue(&running_queue,waiting_tcb);
        schedule();
    } else {
        sem->value++;
    }
    sei();
    return OK;
}