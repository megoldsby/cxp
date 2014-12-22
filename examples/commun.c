

// Tests communcations

#include "comm.h"
#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

static void child1(void *arg)
{
    Channel *input = (Channel *)arg;
    int x;
    in(input, &x, sizeof(int));
    printf("child1 read %d\n", x);
}

static void child2(void *arg)
{
    Channel *output = (Channel *)arg;
    int z = 72;
    out(output, &z, sizeof(int));  
    printf("child2 wrote %d\n", z);
}

int main(int argc, char **argv)
{
    printf("commun: simple send/receive\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    Channel chan;
    init_channel(&chan);

    code_p children[] = { child1, child2 };
    void *args[] = { &chan, &chan };
    uint stacksize[] = { 2000, 2000 };

    par(children, args, stacksize, 2);
    printf("After par\n");

    return 0;
}
