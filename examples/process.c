
// Tests creation and execution of child processes

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

static void child1()
{
    printf("child1\n");
}

static void child2()
{
    printf("child2\n");
}

int main(int argc, char **argv)
{
    printf("process: create and execute child processs\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 2000, 2000 };

    par(children, args, stacksize, 2);
    printf("After par\n");

    return 0;
}
