
// pass multiple args to child process using void*

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

static void child1(void *args)
{
    char **a = (char **)args;
    printf("child1 says %s %s\n", a[0], a[1]);
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
    initialize(0x40000000, 8192);    // 1 GB total allocatable memory
    printf("Initialized\n");

    char *child1Args[] = { "hello", "there" };
    void *args[] = { child1Args, NULL };

    code_p children[] = { child1, child2 };
    uint stacksize[] = { 8192, 8192 };

    par(children, args, stacksize, 2);
    printf("After par\n");

    return 0;
}
