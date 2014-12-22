
// pass argument to child process

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

static void child1(void *arg)
{
    char *msg = (char *)arg;
    printf("child1 says %s\n", msg);
    After(5000000000ULL);
    Time t = GetCurrentTime();
    printf("******child1:  current time is %llu\n", t);
}

static void child2()
{
    printf("child2\n");
    After(2000000000ULL);
    Time t = GetCurrentTime();
    printf("******child2:  current time is %llu\n", t);
}

int main(int argc, char **argv)
{
    printf("proc1arg: pass argument to child process\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    code_p children[] = { child1, child2 };
    char *userArgs[] = { "yabbadoo", NULL };
    uint stacksize[] = { 2000, 2000 };

    par(children, userArgs, stacksize, 2);
    printf("After par\n");

    return 0;
}
