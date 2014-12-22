

// Communication performance

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
    Time t0 = GetCurrentTime();
    int x;
    while (true) {
        in(&chan2, &x, sizeof(int));
        if (x % 100000 == 0) {
            Time t = GetCurrentTime();
            double rate = (double) x / (t - t0);
            double per = 1 / rate;
            printf("x = %d, %g nsec per communication\n", x, per);
        }
    }
}

static void child2()
{
    printf("child2\n");
    int x;
    while (true) {
        x += 1;
        out(&chan2, &x, sizeof(int));
    }
}

int main(int argc, char **argv)
{
    printf("timing_mp: interprocessor communication performance\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory
    printf("Initialized\n");

    init_channel(&chan1);
    init_channel(&chan2);

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 4000, 3000 };
    uint place[] = { 1, 0 };

    placed_par(children, args, stacksize, place, 2);
    printf("After par\n");
    return 0;
}
