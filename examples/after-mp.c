
// Tests interval timer and elapsed time timer (After, Now)

#include "sched.h"
#include "timer.h"
#include "types.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define NS_PER_SEC  1000000000

void child() 
{
    printf("After-mp: tests interval timer and elapsed time timer on processor 1\n");

    // print current time
    Time t = Now();
    printf("current time at beginning is %llu\n", t);

    //sleeper(2);
    After(2000000000ULL);
    
    t = Now();
    printf("current time after After(2 sec) is %llu\n", t);
 
    //sleeper(1);
    After(3000000000ULL);
    
    t = Now();
    printf("current time after After(3 sec) is  %llu\n", t);
 
    //sleeper(1);
    After(4000000000ULL);
    
    t = Now();
    printf("current time after After(4 sec) is  %llu\n", t);
}

int main(int argc, char **argv)
{
    initialize(0x40000000, 1024);    // 1 GB total allocatable memory
    printf("Initialized\n");

    code_p children[] = { child };
    void *args[] = { NULL };
    uint stacksize[] = { 8192 };
    uint place[] = { 1 };

    placed_par(children, args, stacksize, place, 1);

    return 0;
}
