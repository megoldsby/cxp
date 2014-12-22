
// Send token around ring (1 process arg)

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

#define RING_SIZE  100

Channel chan[RING_SIZE];

static void child(void *arg)
{
    int i = (int)arg;
    int iout = i;
    int iin = (i-1+RING_SIZE) % RING_SIZE;
    Channel *output = &chan[iout];
    Channel *input = &chan[iin];

    int x = 0;
    if (i == 0) {
        out(output, &x, sizeof(x));
    }
    while (true) {
        in(input, &x, sizeof(x));
        if (x % 100000 == 0) {
            printf("x = %d\n", x);
        }
        x += 1;
        out(output, &x, sizeof(x));
    }
}

int main(int argc, char **argv)
{
    printf("ring1arg-mp: send token around a ring (1 process arg)\n");
    printf("with processes on alternating processors\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    // initialize the channels
    int i;
    for (i = 0; i < RING_SIZE; i++) {
        init_channel(&chan[i]);
    }

    code_p children[RING_SIZE];
    uint stacksize[RING_SIZE];
    void *args[RING_SIZE];
    uint place[RING_SIZE];
    for (i = 0; i < RING_SIZE; i++) {
        children[i] = child;
        args[i] = (void *)i;
        stacksize[i] = 3000;
        place[i] = i % 2; 
    }

    placed_par(children, args, stacksize, place, RING_SIZE);
    printf("After par\n");

    return 0;
}
