

// Tests timeout in alternation (1 process on processor 1).

#include "alt.h"
#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define ONESEC  1000000000LLU

static void child1()
{
    printf("child\n");
    Guard guards[1];

    Alternation alt;
    init_alt(&alt, guards, 1);

    while (true) {
        Time t = Now() + ONESEC;
        init_timer_guard(&guards[0], t);
        printf("Initialized timer guard to %llu\n", t);
        //int selection = priSelect(&alt);
        int selection = fairSelect(&alt);
        switch (selection) {
        case 0:
            printf("Timeout at %llu\n", Now());
            break;
        default:
            printf("Invalid selection %d\n", selection);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    printf("Timeout in alternation on processor 1\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    code_p children[] = { child1 };
    void *args[] = { NULL };
    uint stacksize[] = { 2000 };
    uint place[] = { 1 } ;

    placed_par(children, args, stacksize, place, 1);
    printf("After par\n");

    return 0;
}
