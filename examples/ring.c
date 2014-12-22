

// Tests communcation in a ring

#include "comm.h"
#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

#define RING_SIZE  100

typedef struct ElementArgs {
    int nr;
    Channel *input;
    Channel *output;
} ElementArgs;

static void element(void *args)
{
    // get the arguments
    ElementArgs *eargs = (ElementArgs *)args;
    int nr = eargs->nr;
    Channel *input = eargs->input;
    Channel *output = eargs->output;

    int x;  // the value sent around the ring

    // if this is the 0th element, start the ball rolling
    if (nr == 0) {
       x = 1; 
       out(output, &x, sizeof(int));
    } 

    // forever after..
    Time t0 = GetCurrentTime();
    while (true) {
        
        // read incoming value
        in(input, &x, sizeof(x));
        if (x % 100000 == 0) {
            Time t = GetCurrentTime();
            double per = (double)(t - t0) / x;
            // display value if this is element 0
            printf("After %d communciations, rate = %g ns/comm\n", x, per);
        }

        // increment value and forward it
        x += 1;
        out(output, &x, sizeof(x));
    }
}

int main(int argc, char **argv)
{
    printf("ring: %d processes pass messages around a ring\n", RING_SIZE);
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    // prepare the interelement channels
    Channel chan[RING_SIZE];
    int i;
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&chan[i]);
    }

    // prepare the arguments
    code_p elements[RING_SIZE];
    uint stacksize[RING_SIZE];
    ElementArgs elemArgs[RING_SIZE];
    ElementArgs *args[RING_SIZE];
    for (i = 0; i < RING_SIZE; i++) {
        elemArgs[i].nr = i;
        elemArgs[i].input = &chan[(i-1 + RING_SIZE) % RING_SIZE];
        elemArgs[i].output = &chan[i];
        args[i] = &elemArgs[i];
        elements[i] = element;
        stacksize[i] = 2000;
    } 
   

    par(elements, args, stacksize, RING_SIZE); 
    printf("After par\n");

    return 0;
}
