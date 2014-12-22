
// pass argument to child process,
// children on different processors

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

static void sleeper(int sec)
{
    struct timespec req;    
    struct timespec rem;    
    req.tv_sec = sec;
    req.tv_nsec = 0;
    int r = -1;
    while (r) {
        r = nanosleep(&req, &rem);
        req = rem;
    } 
}

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
    printf("proc1arg-mp: pass argument to child process\n");
    printf("children on different processors\n");
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory

    code_p children[] = { child1, child2 };
    char *userArgs[] = { "yabbadoo", NULL };
    uint stacksize[] = { 8192, 8192 };
    uint place[] = { 1, 0 };

    placed_par(children, userArgs, stacksize, place, 2);
    printf("After par\n");

    return 0;
}
