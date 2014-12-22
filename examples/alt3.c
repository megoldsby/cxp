

// Tests alternation with consumer and two producers

#include "alt.h"
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
    printf("consumer\n");
    Guard guards[2];
    init_channel_guard(&guards[0], &chan1);
    init_channel_guard(&guards[1], &chan2);

    Alternation alt;
    init_alt(&alt, guards, 2);

    while (true) {
        int selection = priSelect(&alt);
        //int selection = fairSelect(&alt);
        int x;
        int y;
        switch (selection) {
        case 0:
            in(&chan1, &x, sizeof(x)); 
            if (x % 10000 == 0) {
                printf("Consumer input %d from chan1\n", x);
            }
            break;
        case 1:
            in(&chan2, &y, sizeof(y)); 
            if (y % 10000 == 1) {
                printf("Consumer input %d from chan2\n", y);
            }
            break;
        default:
            printf("Invalid selection %d\n", selection);
            break;
        }
    }
}

static void child2()
{
    printf("producer 1\n");
    int x = 0;
    while (true) {
        int expr = 2*x;
        out(&chan1, &expr, sizeof(expr));
        x += 1;
    }
}

static void child3()
{
    printf("producer 2\n");
    int x = 0;
    while (true) {
        int expr = 2*x+1;
        //printf("Producer 2 sending %d on chan2\n", expr);
        out(&chan2, &expr, sizeof(expr));
        x += 1;
    }
}

int main(int argc, char **argv)
{
    printf("alt3: alternation with consumer and two producers\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    init_channel(&chan1);
    init_channel(&chan2);

    code_p children[] = { child1, child2, child3 };
    void *args[] = { NULL, NULL, NULL };
    uint stacksize[] = { 3000, 2000, 2000 };

    par(children, args, stacksize, 3);
    printf("After par\n");

    return 0;
}
