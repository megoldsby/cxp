

// Tests alternation with producer and consumer

#include "alt.h"
#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define ONESEC  1000000000LLU

static Channel chan1;
static Channel chan2;

static void child1()
{
    printf("consumer\n");
    Guard guards[2];
    init_channel_guard(&guards[0], &chan1);

    Alternation alt;
    init_alt(&alt, guards, 2);

    while (true) {
        init_timer_guard(&guards[1], Now() + ONESEC);
        //int selection = priSelect(&alt);
        int selection = fairSelect(&alt);
        int x;
        int y;
        switch (selection) {
        case 0:
            in(&chan1, &x, sizeof(x)); 
            printf("*****Consumer input %d from chan1\n", x);
            break;
        case 1:
            printf("*****Consumer times out at time %llu\n", Now());
            out(&chan2, &x, 0);
            break;
        default:
            printf("Invalid selection %d\n", selection);
            break;
        }
    }
}

static void child2()
{
    printf("producer\n");
    int x = 0;
    while (true) {
        printf("Producer sending %d on chan1\n", x);
        out(&chan1, &x, sizeof(x));

        // wait for signal from consumer to proceed
        in(&chan2, &x, 0);
        x += 1;
    }
}

int main(int argc, char **argv)
{
    printf("alttime: alternation alternately receives and times out\n");
    printf("with producer and consumer on different processors\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    init_channel(&chan1);
    init_channel(&chan2);

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 2000, 2000 };
    uint place[] = { 0, 1 };

    placed_par(children, args, stacksize, place, 2);
    printf("After par\n");

    return 0;
}
