
/**
 *  CXP   C eXecutive Program
 *  Copyright (c) 2014 Michael E. Goldsby
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hardware.h"
#include "timer.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include "dbg.h"

#define NS_PER_SEC  1000000000

// thread-local key for cpu number
static pthread_key_t cpu_key;

// for allocation of signal numbers
static int SIGBASE;                // first (lowest) signal used here
_Atomic static int next_signo;

/** Map from processor number to thread id */
static pthread_t thread_id[NPUN];

/** Map from signal number to processor and interrupt number 
 *  (use (signo - SIGBASE) as index) */
typedef struct IntrDesc {
    uint16 pun;      // processing unit number
    uint16 intr;     // interrupt number
} IntrDesc;
static IntrDesc interrupt[NPUN*NINTR];

/** Map from processor and interrupt number to signal number */
static int signal_no[NPUN][NINTR];

// set of blocked signals for each thread
static sigset_t sigmask[NPUN];

// clock ids for elapsed time timer and the timeout timers
static timer_t timer[NPUN][NTIMER];

/** Arguments for pthread threads */
typedef struct ProcArg {
    int cpu;    	    // processor number
    ucontext_t *context;    // ptr to initial context
} ProcArg;
ProcArg procArg[NPUN];

/** end-of-initialization barrier */
_Atomic int barrier = ATOMIC_VAR_INIT(NPUN);

/** Disables interrupts on the current thread */
inline void disable()
{
    // give the thread a signal mask that blocks everything
    sigset_t blocked;  
    if
    (sigfillset(&blocked)) plotz("disable sigfillset");
    if 
    (pthread_sigmask(SIG_SETMASK, &blocked, NULL)) plotz("disable sigmask");
    //maybe block only the RT signals
}

/** Enables interrupts on the current thread */
inline void enable()
{
    // restore the thread's proper signal mask
    if 
    (pthread_sigmask(SIG_SETMASK, &sigmask[getcpu()], NULL)) plotz("enable sigmask");
    // note that only processor p reads sigmask[p]
}

/** Not necessary in a hardware implementation. */
inline void phantom_enable() 
{
    enable();
}

/** for testing */
void display_sigmask(char *descrip, ucontext_t *context)
{
    printf("  %s", descrip);
    int i;
    for (i = SIGRTMIN; i <= SIGRTMAX; i++) {
        if (sigismember(&context->uc_sigmask, i)) {
            printf(" %d", i);
        }
    } 
    printf("\n");
}

/** The signal handler */
static void handle_signal(int signo, siginfo_t *info, void *ucontext)
{
    // disable "interrupts"
    disable();
    // signo is already disabled for the duration of this handler;
    // this disables the rest of them

    // save interrupted context in case need to preempt
    Process *curr = get_current();    
    memcpy(curr->stackptr, ucontext, sizeof(ucontext_t));

    // learn interrupt number
    int index = signo - SIGBASE;
    IntrDesc desc = interrupt[index];
    int intr = desc.intr;
    // note that only the processor to which signo is
    // assigned reads this array element

    // call interrupt handler
    handle_interrupt(intr);

    // reenable "interrupts"
    enable();
}

/** Sets handler for given interrupt number on this processing unit */
interrupt_handler_t define_handler(int intr, interrupt_handler_t handler)
{
    // get signal number to be used for this interrupt
    int signo = atomic_fetch_add_explicit(&next_signo, 1, memory_order_acq_rel);
    
    // map signal number to processor and interrupt number
    int pun = getcpu();
    int index = signo - SIGBASE;
    interrupt[index].intr = intr;
    interrupt[index].pun = pun;     
    // note that only the processor to which this signo is 
    // assigned writes this array element

    // map processor and interrupt number to signal number
    atomic_store_explicit(&signal_no[pun][intr], signo, memory_order_release);
    // signal_no[p][i] written only by p

    // allow the signal on this thread
    sigdelset(&sigmask[pun], signo);
    // note that only processor p writes sigmask[p]

    // set handler for the signal
    sigset_t all;                   
    sigfillset(&all);
    struct sigaction action;
    action.sa_handler = NULL;             // call sigaction, not signal       
    action.sa_sigaction = handle_signal;  // signal handler
    action.sa_mask = all;                 // block all signals when in handler
    action.sa_flags = SA_SIGINFO;         // call sigaction, not signal
    //action.sa_restorer = NULL;            // not used
    sigaction(signo, &action, NULL);
}

/** Sets up process to begin executing with the given code and argument */
void build_context(
    Process *proc, uint stacksize, code_p code, void *arg1, void *arg2,
    void *userArgs)
{
    // in this implementation, put context on at start of stack
    // and let stackptr point to it
    proc->stackptr = proc->stack;
    ucontext_t *context = (ucontext_t *)proc->stackptr;

    // allocate context record on the stack and populate it
    if 
    (getcontext(context)) plotz("build_context getcontext");

    // set stack descriptors
    context->uc_link = NULL;
    context->uc_stack.ss_sp = (char *)proc->stackptr + sizeof(ucontext_t);
    context->uc_stack.ss_size = stacksize - sizeof(ucontext_t);

    // note context->uc_link is irrelevant since no process (except initial
    // process) ever exits (see run_in_par in run.c and idle process)

    // plug in location of code to execute and its arguments
    makecontext(context, code, 3, arg1, arg2, userArgs);
}

/** Restores the context of a process, giving it the processor */
void restore_context(Process *proc)
{
    if
    (setcontext((ucontext_t *)&proc->stack)) plotz("restore_context setcontext");
}

/** Enables interrupts then resumes given process */
static void enable_and_resume(Process *curr)
{
    // enable interrupts, then resume saved context
    enable();
    if 
    (setcontext((ucontext_t *)&curr->stack)) 
        plotz("enable_and_resume setcontext");
}

/** Switches the processor from one process to another */
void switch_context(Process *oldproc, Process *newproc)
{
    // in this implementation, stackptr points to the context
    // (see build_context, above)
    if
    (swapcontext((ucontext_t *)oldproc->stackptr, (ucontext_t *)newproc->stackptr))
        plotz("switch_context swapcontext");
//}

}

/** Switches the processor from one process to another, where the
 *  process losing the processor is in an interrupt handler. */
void switch_interrupt_context(Process *interrupted, Process *preempting)
{
    // establish context of preempting process, giving it the processor
    if
    (setcontext((ucontext_t *)&preempting->stack)) plotz("switch_interrupt_context swapcontext");
    // note have already saved context of preempted process in handle_signal
}

/** Make current (terminating) process use given stack. */
void set_stack(Process *proc, byte *stack, uint stacksize, code_p code)
{
    // for new context, use context area at start of stack
    // (though could really put it anywhere..)
    ucontext_t *context = (ucontext_t *)proc->stackptr;

    // establish a base context
    if
    (getcontext(context)) plotz("set_stack getcontext");

    // fill in the stack info
    context->uc_stack.ss_sp = stack;
    context->uc_stack.ss_size = stacksize;

    // specify where to continue execution
    makecontext(context, code, 0);

    // and do so..
    if 
    (setcontext(context)) plotz("set_stack getcontext");
}

/** Returns the processor number (0..NPUN-1) */
inline int getcpu()
{
    void *cpu = pthread_getspecific(cpu_key);
    if (cpu == NULL) plotz("getcpu getspecifiic");
    return ((int)cpu - 1);
    // store cpu+1 in case value of NULL is zero
}

/** Returns the address of a region of memory of the specified length */
char *acquire_memory(int bytes)
{
    char *p = malloc(bytes);
    if (p == NULL) plotz("acquire_memory malloc");
    return p;
}

/** Synchronize at barrier (all processors synch here) */
void synchronize_processors() 
{
    // decrement barrier value, then spin till it becomes zero
    atomic_fetch_sub_explicit(&barrier, 1, memory_order_acq_rel);
    while (atomic_load_explicit(&barrier, memory_order_acquire) > 0);
    // barrier is zero, all processors have reached here
}

/** Initializes a processor. */
static void init_processor(int cpu)
{
    // set cpu id (thread local)
    //int r = pthread_setspecific(cpu_key, (void *)(cpu+1));
    //if (r) plotz("init_processor setspecific");
    if
    (pthread_setspecific(cpu_key, (void *)(cpu+1))) plotz("init_processor setspecific");
    // add one in case the value of NULL is zero (see getcpu)

    // make an entry in the cpu id --> thread id map for this thread
    thread_id[cpu] = ATOMIC_VAR_INIT(pthread_self());
    // thread_id[cpu] written here during init only by cpu

    // start with all (RT) signals blocked 
    if
    (sigemptyset(&sigmask[cpu])) plotz("init_processor sigemptyset");
    int signo;
    for (signo = SIGRTMIN; signo <= SIGRTMAX; signo++) {
        if
        (sigaddset(&sigmask[cpu], signo)) plotz("init_processor sigaddset");
    }
    // note that only processor p writes sigmask[p]

    // define interrupt handlers for this processor
    define_interrupt_handlers();

    // set signal mask for the thread, enabling the signals
    // proper to this thread
    if
    (pthread_sigmask(SIG_BLOCK, &sigmask[cpu], NULL)) plotz("init_processor sigmask");
    // note that only processor p reads sigmask[p]
}

/** This routine is the first thing executed by a newly created thread. */
static void *run_process(void *arg)
{
    ProcArg *procArg = (ProcArg *)arg;

    // wait at barrier for all to initialize 
    init_processor(procArg->cpu);

    // synchronize all processors here
    synchronize_processors();
    // use this barrier so that thread_id and signal_no need
    // not be atomic.  Each acquires its value during initialization
    // and after that is read-only.

    // then run the process specified in proc..
    //ucontext_t *context = (ucontext_t *)(((Process *)proc)->stack);
    ucontext_t *context = (ucontext_t *)(procArg->context);
    if
    (setcontext(context)) plotz("run_process setcontext");
}

/** Activates a processor, giving it a process to run */
void activate_processor(int pun, Process *proc)
{
    // if this is the initial processor, initialize it in this thread
    if (pun == 0) {
        init_processor(0);

    // if this is any other processor, start a thread to represent it
    } else {
        // the thread first executes a function that initializes the processor
        // and then runs the process specified in the Process record
        pthread_t thread;
        procArg[pun].cpu = pun;
        procArg[pun].context = (ucontext_t *)proc->stackptr;
        if
        (pthread_create(&thread, NULL, run_process, &procArg[pun])) 
            plotz("activate_processor pthread_create");
        // note that "interrupts" are disabled when the processors
        // are activated, and the created thread inherits the
        // parent's signal mask, so it starts disabled, too
    }
}

/** Halts processor, waiting for an interrupt. */
void halt_processor()
{
    //enable();
    // a signal will interrupt sleep, allowing another process
    // to get scheduled preemptively
    while (true) 
    {
        sleep(10);
    }
}

/** Send interprocessor interrupt to given processor */
void send_interprocessor_interrupt(int pun)
{
    // find thread corresponding to processor
    pthread_t target = atomic_load_explicit(&thread_id[pun], memory_order_acquire);
    // thread_id[pun] read here by any processor

    // find signal number for interprocessor interrupt to that processor
    int signo = atomic_load_explicit(
        &signal_no[pun][INTR_INTERPROC], memory_order_acquire);
    // signal_no[pun][INTR_INTERPROC} read here by any processor

    // send the "interrupt"
    if
    (pthread_kill(target, signo)) plotz("send_interprocessor_interrupt pthread_kill");
}

/** Initialize a given timer */
void init_timer(int timerType)
{
    // get processor id
    int pun = getcpu();

    // set up timer to notify by signal
    struct sigevent se;
    se.sigev_notify = SIGEV_SIGNAL;

    // timer type implies interrupt number
    int intr;
    if (timerType == TIMER_ELAPSED) {
        intr = INTR_ELAPSED;
    } else if (timerType == TIMER_TIMEOUT) {
        intr = INTR_TIMEOUT;
    } else plotz("init_timer no such timer");

    // find signal number from processor number and interrupt number
    se.sigev_signo = signal_no[pun][intr];
    // signal_no[p][i] read here only by p

    // create timer
    if 
    (timer_create(CLOCK_REALTIME, &se, &timer[pun][timerType])) plotz("init_timer timer_create");
    // note that only processor p accesses the elements for
    // the timers assigned to processor p
}

/** Set interval timer for a single interval. */
void set_timer_single(int timerType, Time time)
{
    // get timer id for this processor and timer type
    int pun = getcpu();
    timer_t timerId = timer[pun][timerType];
    // note that only processor p accesses the elements for
    // the timers assigned to processor p

    // if interval is negative, just user zero
    time = (time > 0 ? time : 0);
        
    // break time value into seconds and nanoseconds
    struct timespec ts;
    ts.tv_sec = time / NS_PER_SEC;
    ts.tv_nsec = time % NS_PER_SEC;

    // set value to given time and interval to zero
    struct itimerspec spec;
    spec.it_value = ts;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 0;
    
    // set the timer
    if (
    timer_settime(timerId, 0, &spec, NULL)) plotz("set_timer_single timer_settime");
// should I used ABSTIME for this?
}

/** Set interval timer for a repeating interval. */
void set_timer_repeating(int timerType, Time time)
{
    // get timer id for this processor and timer type
    int pun = getcpu();
    timer_t timerId = timer[pun][timerType];
    // note that only processor p accesses the elements for
    // the timers assigned to processor p

    // break time value into seconds and nanoseconds
    uint64 t = (int64)time;
    struct timespec ts;
    ts.tv_sec = time / NS_PER_SEC;
    ts.tv_nsec = time % NS_PER_SEC;

    // set interval and initial expiration to same value
    struct itimerspec spec;
    spec.it_interval = ts;
    spec.it_value = ts;

    // set the timer
    if (
    timer_settime(timerId, 0, &spec, NULL)) plotz("set_timer_repeating timer_settime");
}

/** Read timer. */
Time read_timer(int timerType)
{
    // get timer id for this processor and timer type
    int pun = getcpu();
    timer_t timerId = timer[pun][timerType];
    // note that only processor p accesses the elements for
    // the timers assigned to processor p

    // read the timer
    struct itimerspec time;
    if 
    (timer_gettime(timerId, &time)) plotz("read_timer timer_gettime");

    // extract and return the time as nanoseconds
    struct timespec *ts = &time.it_value;
    Time t = ((Time)ts->tv_sec * NS_PER_SEC) + ts->tv_nsec;
    return t;
}

/** Send application-level interrupt as software interrupt. */
void send_interrupt(int intr)
{
    // make sure it's not system interrupt
    if (intr < INTR_USER0) plotz("send_interrupt not user interrupt");

    // get signal number for the interrupt
    int pun = getcpu();
    int signo = signal_no[pun][intr];

    // send interrupt
    pthread_t self = pthread_self();
    if
    (pthread_kill(self, signo)) plotz("send interrupt pthread_kill");
}

/** Initializes this module */
void hardware_init()
{
    // create key for thread-local cpu number 
    // (just do this once for all threads)
    if (
    pthread_key_create(&cpu_key, NULL)) plotz("hardware_init pthread_key_create");

    // initialize fields for signal number allocation
    SIGBASE = SIGRTMIN;
    next_signo = ATOMIC_VAR_INIT(SIGBASE);
}
