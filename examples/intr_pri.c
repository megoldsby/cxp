
// Interrupts under priority scheduling.

#include "hardware.h"
#include "interrupt.h"
#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000
#define ONESEC  1000000000

static void child1()
{
    printf("child1\n");
    while (true) {
        receive(0);
        printf("Received USER0\n");
    }
}

static void child2()
{
    printf("child2\n");
    Time t0 = Now();
    while (true) {
        printf(".");  
        fflush(stdout);
        Time t = Now();
        while (t - t0 < ONESEC/2) {
            t = Now(); 
        }
        t0 = t;
        printf("About to send interrupt\n");
        send_interrupt(INTR_USER0);
    }
}

/** Note that without priority scheduling, child1 would never get control */

int main(int argc, char **argv)
{
    printf("intr_pri: interrupts with process priorities\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 3000, 3000 };

    par_pri(children, args, stacksize, 2);
    printf("After par\n");

    return 0;
}
