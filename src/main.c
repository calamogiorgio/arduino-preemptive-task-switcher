#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "tcb.h"
#include "tcb_list.h"
#include "uart.h"
#include "atomport_asm.h"
#include "scheduler.h"
#include "semaphore.h"
#include "message_queue.h"

#define THREAD_STACK_SIZE 256
#define IDLE_STACK_SIZE   128
#define NUM_PROCESSES     20

/* Global OS resources */
Semaphore sem[NUM_PROCESSES];
MessageQueue my_queue;

/* Thread descriptors and stacks */
TCB tcb[NUM_PROCESSES];
uint8_t tcb_stack[NUM_PROCESSES][THREAD_STACK_SIZE];

TCB idle_tcb;
uint8_t idle_stack[IDLE_STACK_SIZE];

/**
 * Lightweight integer-to-ASCII converter for serial output
 */
void my_itoa(uint32_t num) {
    char buf[11];
    int i = 0;

    if (num == 0) {
        usart_putchar('0');
        return;
    }

    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0) {
        usart_putchar(buf[--i]);
    }
}

/**
 * Background Idle Thread operating as an Echo Server
 * Captures characters typed on the PC and echoes them back dynamically.
 */
void idle_fn(uint32_t thread_arg __attribute__((unused))) {
    while(1) {
        /* Check if a character has arrived in the RX buffer without blocking */
        if (usart_kbhit()) {
            char c = usart_getchar();
            
            /* Print a clean indicator and echo the character back to the PC */
            usart_pstr(" [Echo: ");
            usart_putchar(c);
            usart_pstr("]\n");
        } else {
            /* If no data is available, yield or execute a nop to save CPU */
            asm volatile("nop");
        }
    }
}

/**
 * Producer Thread
 */
void producer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_consumer = id + half;
    char* msg = "Hello World!";

    if (id == 0) {
        usart_pstr("Kernel Initialized Successfully!\n");
    }

    while(1) {
        /* Sync execution turn */
        sem_wait(&sem[id]);

        /* Inter-process communication */
        msg_send(&my_queue, msg);

        /* Wake up paired consumer */
        sem_post(&sem[target_consumer]);
        
        /* Pacing delay to prevent buffer flooding */
        //_delay_ms(50); 
    }
}

/**
 * Consumer Thread
 */
void consumer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_producer = id - half; 
    char* received_msg = NULL;

    while(1) {
        /* Sync execution turn */
        sem_wait(&sem[id]);

        /* Retrieve data from the shared queue */
        msg_receive(&my_queue, (void**)&received_msg);

        /* Sequential UART output without brackets */
        usart_pstr("C");
        my_itoa(id);
        usart_pstr(" received from P");
        my_itoa(target_producer);
        usart_pstr(": ");
        usart_pstr(received_msg);
        usart_pstr("\n");

        /* Compute and trigger the next producer token in round-robin sequence */
        uint8_t next_producer = target_producer + 1;
        if (next_producer >= half) {
            next_producer = 0;
        }
        sem_post(&sem[next_producer]);
    }
}

int main(void) {
    uint8_t i;
    uint8_t half = NUM_PROCESSES >> 1;

    /* Peripherals and OS subsystems initialization */
    uart_init();
    msg_queue_init(&my_queue);

    /* Setup sync token ring: Producer 0 starts active, others blocked */
    sem_init(&sem[0], 1);
    for (i = 1; i < NUM_PROCESSES; i++) {
        sem_init(&sem[i], 0);
    }

    /* Create background idle environment */
    TCB_create(&idle_tcb, idle_stack + IDLE_STACK_SIZE - 1, idle_fn, 0);

    /* Allocate runtime workers (Producers and Consumers) */
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (i < half) {
            TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], producer_fn, i);
        } else {
            TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], consumer_fn, i);
        }
    }

    /* Enqueue system threads inside the active queue */
    for (i = 0; i < NUM_PROCESSES; i++) {
        TCBList_enqueue(&running_queue, &tcb[i]);
    }
    TCBList_enqueue(&running_queue, &idle_tcb);

    /* Pass CPU ownership to kernel scheduler */
    startSchedule();
    
    return 0;
}