
// Ping pong between two processes w/o argument passing

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

static Channel chan1;
static Channel chan2;

static void child1()
{
    printf("child1\n");
    int x = 0;
    out(&chan1, &x, sizeof(int));
    while (true) {
        in(&chan2, &x, sizeof(int));
        x += 1;
        out(&chan1, &x, sizeof(x));
    }
}

static void child2()
{
    printf("child2\n");
    int x;
    while (true) {
        in(&chan1, &x, sizeof(int));
        if (x % 100000 == 0) {
            printf("x = %d\n", x);
        }
        x += 1;
        out(&chan2, &x, sizeof(int));
    }
}

int main(int argc, char **argv)
{
    printf("ring0arg2: two process ping pong, no process args\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    init_channel(&chan1);
    init_channel(&chan2);

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 2000, 2000 };

    par(children, args, stacksize, 2);
    printf("After par\n");

    return 0;
}
