
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

#include "sched.h"
#include "alt.h"
#include "comm.h"
#include "hardware.h"
#include "interrupt.h"
#include "memory.h"
#include "run.h"
#include "timer.h"
#include "types.h"
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbg.h"

/** External assembler routines. */
extern void switch_context(Process *old, Process *new);
extern void restore_context(Process *new);

/** priority of current executing process on each unit */
static _Atomic(int) current_pri[NPUN];

/** currently executing process on each unit */
static Process_p current[NPUN];       

/** interprocessor queue logic */
#include "ipq.m"

/** the ready queues */
static RdyQDesc rdyQues[NPUN];

// data structures used in process termination
#define TERM_STACK_SIZE 4096
typedef struct Termination {
    Mutex mutex;
    Word stack[TERM_STACK_SIZE];
} Termination;
static Termination termination[NPUN];

/** process associated with scheduling interrupt */
static Process_p pending[NPUN];

/** Returns current process on this processor */
inline Process *get_current()
{
    return current[getcpu()];
}
//can inline work for this function?  It accesses a static variable
//that exists only in the sched.c module..

/** Sets the current (executing) process on this processor.
 *  Interrupts must be disabled when calling this function. */
inline void set_current(Process *proc)
{
     int pun = getcpu();
     if (proc->pun != pun)  
         plotz("set_current non-matching cpu#");
     // the above test is unnecessarily if the code is correct...
     current[pun] = proc;
     atomic_store_explicit(&current_pri[pun], proc->pri, memory_order_release);
}
//can inline work for this function?  It accesses a static variable
//that exists only in the sched.c module..

/** Moves given process to the preparing-to-wait state */
inline void prepare_to_wait()
{
    // enter preparing-to-wait state
    Process *curr = get_current();
    atomic_store_explicit(&curr->sched_state,
        PROC_PREPARING_TO_WAIT, memory_order_release);
}

/** Handles scheduling interrupt */
static void handle_interprocessor_interrupt()
{
    // consume all processes in this processor's interprocessor queue
    // and schedule them (processes made ready by another processor)
    Process *proc = ipq_remove();
    while (proc != NULL) {
        schedule0(proc);
        proc = ipq_remove();
    }

    // preempt if necessary
    schedule_from_interrupt();
}

/** Handles given application-level interrupt */
void handle_user_interrupt(int uintr)
{
    // fire user interrupt
    Process *receiver = transmit(uintr);

    // preempt if necessary
    if (receiver != NULL) {
        schedule_from_interrupt();
    }
}

/** Defines interrupt handlers for current processor. */
void define_interrupt_handlers()
{
    // if this is processor 0, define handler for elapsed time
    // interrupts and initialize and start elapsed time timer
    int pun = getcpu();
    if (pun == 0) {
        define_handler(INTR_ELAPSED, handle_elapsed_time_interrupt);
        init_timer(TIMER_ELAPSED);
        set_timer_repeating(TIMER_ELAPSED, getTick());
    }

    // define handler for timeout interrupt and initialize timeout timer
    define_handler(INTR_TIMEOUT, handle_timeout_interrupt);
    init_timer(TIMER_TIMEOUT);

    // define handler for interprocessor interrupt
    define_handler(INTR_INTERPROC, handle_interprocessor_interrupt);

    // define handlers for user-level interrutps
    define_handler(INTR_USER0, handle_user_interrupt);
    define_handler(INTR_USER1, handle_user_interrupt);
}

/** Builds a process record. */
static Process_p build_process(uint stacksize, uint pri, uint pun)
{
    // allocate process record, including stack
    int index = find_mem_index(stacksize + sizeof(Process));
    Process_p proc = (Process_p)allocate_mem(index);

    // fill in process record
    proc->next = NULL;
    proc->index = index;
    proc->pri = pri;
    proc->pun = pun;
    proc->alt_state = ATOMIC_VAR_INIT(ALT_NONE);
    proc->sched_state = ATOMIC_VAR_INIT(PROC_WAITING);  
    return proc;
}

/** Constructs a process and returns a pointer to the process record  */
Process_p make_process(code_p code, void *arg1, void *arg2, 
    uint stacksize, uint pri, uint pun, void *userArgs)
{
    Process *proc = build_process(stacksize, pri, pun);
    build_context(proc, stacksize, code, arg1, arg2, userArgs);
    return proc;
}

/**-------------------------------------------------------------
 *  Inserts process into the scheduling queue for its processor.
 *  Interrupts must be disabled when call this function.
 *-------------------------------------------------------------*/
void enqueue0(Process *proc)
{
    /*---------------------------------------------------------------------
     |  find proc's place in the queue,
     |  preserving fifo order in case of tie
     |  for now just use a linear search, later do something more efficient
     ---------------------------------------------------------------------*/
    RdyQDesc *que = &rdyQues[proc->pun];
    Process *prev = NULL;
    Process *curr = que->head;
    while (curr != NULL && pri_le(proc->pri, curr->pri)) {
    //while (curr != NULL && pri_lt(proc->pri, curr->pri)) {
        prev = curr;
        curr = curr->next;
    }
 
    /*----------------------------------
     |  insert proc into the queue
     ----------------------------------*/
    if (prev == NULL && curr == NULL) {
        // proc is only process in queue!
        que->head = proc;
        proc->next = NULL;

    } else if (prev == NULL && curr != NULL) {
        // proc is first process in queue but has a successor
        //proc->next = que->head;
        que->head = proc;
        proc->next = curr;
    
    } else if (prev != NULL && curr != NULL) {
        // proc has a predecessor and a successor in the queue
        prev->next = proc;
        proc->next = curr;

    } else /* prev != NULL && curr == NULL */ {
        // proc is last but has a predecessor
        prev->next = proc;
        proc->next = NULL;
    }
}

/**-------------------------------------------------------------
 *  Inserts process into the scheduling queue for its processor.
 *-------------------------------------------------------------*/
void enqueue(Process *proc)
{
    disable();
    enqueue0(proc);
    enable();
}

/**------------------------------------------------------
 |  Removes head (highest priority) process from given 
 |  processor's scheduling queue and returns it.
 |  Must be called with interrupts disabled.
 ------------------------------------------------------*/
static Process *take(int pun)
{
    // consume all processes in this processor's interprocessor queue
    // and schedule them (processes made ready by another processor)
    Process *proc = ipq_remove();
    while (proc != NULL) {
        enqueue0(proc);
        proc = ipq_remove();
    }

    // remove head entry from ready queue and return it
    RdyQDesc *que = &rdyQues[pun];
    proc = que->head;
    if (proc != NULL) {
        que->head = proc->next;
    }
    return proc; 
}

/**--------------------------------------------------------
 |  Removes head (highest priority) process from given 
 |  processor's scheduling queue and returns it, provided
 |  there are at least two processes in the ready queue
 |  (never returns the idle process).
 |  Must be called with interrupts disabled.
 --------------------------------------------------------*/
static Process *take1(int pun)
{
    // consume all processes in this processor's interprocessor queue
    // and schedule them (processes made ready by another processor)
    Process *proc = ipq_remove();
    while (proc != NULL) {
        enqueue0(proc);
        proc = ipq_remove();
    }

    // provided there are at least two processes in the ready
    // queue (that is, one process besides the idle process),
    // remove head entry and return it (so can't yield to the idle
    // process--and, for that matter, idle process can't yield--
    // see yield())
    RdyQDesc *que = &rdyQues[pun];
    if (que->head != NULL && que->head->next != NULL) {
        proc = que->head;
        que->head = proc->next;
    }

    // return process or null
    return proc;
}

/**-------------------------------------------------------------
 *  Makes the given process ready to execute, possibly
 *  preempting the currently executing process.
 *  Interrupts must be disabled when calling this function. 
 *-------------------------------------------------------------*/
void schedule0(Process *proc)
{
    // if process is on this processor..
    int pun = getcpu();
    if (proc->pun == pun) 
    {
        // if priority of process being scheduled is not higher than
        // that of current process, just put in on scheduing queue
        Process *curr = get_current();
        if (pri_le(proc->pri, curr->pri)) {
            enqueue0(proc);

        // if priority is higher, preempt current process
        } else {

            // make proc the current process
            set_current(proc);

            // put current process on scheduling queue
            enqueue0(curr);

            // switch to proc's context
            switch_context(curr, proc);
            //enable();
            // not necessary to enable here--if resume here,
            // caller of schedule0 will enable
        }
    }
    else // process is on another processor
    {
        // put process into interprocessor queue and 
        // notify other processor
        ipq_add(proc->pun, proc);
    }
}

/**-------------------------------------------------------------
 *  Makes the given process ready to execute, possibly
 *  preempting the currently executing process.
 *-------------------------------------------------------------*/
void schedule(Process *proc)
{
    disable();
    schedule0(proc);
    enable();
}

/**-------------------------------------------------------------
 *  //Returns the highest priority ready process if it
 *  //has priority higher than currently executing process
 *  //and null otherwise.
 
 *  Preempts the current process if there is a higher-priority
 *  ready process.  Called from an interrupt handler with
 *  interrupts disabled.
 *-------------------------------------------------------------*/
void schedule_from_interrupt()
{
    // get current process and select ready queue
    Process *curr = get_current();
    RdyQDesc *que = &rdyQues[curr->pun];

    // if there is a process in the queue and its prioriry
    // is higher than the current process's..
    if (que->head != NULL && pri_gt(que->head->pri, curr->pri)) {

        // remove it from the ready queue
        Process *proc = take(curr->pun);

        // put current process on ready queue
        enqueue0(curr);
    
        // make new process the current process
        set_current(proc);
    
        // switch to proc's context
        switch_interrupt_context(curr, proc);
        // not necessary to enable here--if resume here,
        // caller of this function will enable
    }
    // (if there was no process on the ready queue, it means
    // that this is the idle process, if anyone cares)
}

/**-------------------------------------------------------------
 *  Preempts the current process with the given process if
 *  the latter has higher priority.
 *-------------------------------------------------------------*/
void schedule_from_interrupt_single(Process *proc)
//Process *schedule_from_interrupt()
{
    // get current process 
    Process *curr = get_current();

    // if priority of given process is not higher than that of
    // current process, just put given process on scheduing queue
    if (pri_le(proc->pri, curr->pri)) {
        enqueue0(proc);

    // if priority is higher, preempt current process
    } else {

        // make proc the current process
        set_current(proc);

        // put current process on scheduling queue
        enqueue0(curr);

        // switch to proc's context
        switch_interrupt_context(curr, proc);
        // not necessary to enable here--if resume here,
        // caller of this function will enable
    }
}

/** ------------------------------------------------------
 |  Saves current process's state and gives processsor to
 |  highest priority ready process, provided current
 |  process successfully enters waiting state; otherwise
 |  allows current process to continue.
  -------------------------------------------------------*/
void relinquish()
{
    // get current process and processor
    Process *oldproc = get_current();
    int pun = oldproc->pun;

    // try to set current process's state to 'waiting'
    uint8 expected = PROC_PREPARING_TO_WAIT;
    _Bool successful = atomic_compare_exchange_strong_explicit(
        &oldproc->sched_state, 
        &expected, 
        PROC_WAITING,
        memory_order_acq_rel,
        memory_order_relaxed);
    // if fail, state was 'ready', and current process can
    // proceed without relinquishing

    // if process state now 'waiting'...
    if (successful) {

        // disable interrupts
        disable();

        // get highest-priority ready process for this processor
        Process *newproc = take(pun);

        // make it the current process on this processor
        set_current(newproc);

        // switch contexts from old proc to new proc
        switch_context(oldproc, newproc);

        enable();
        // if new process lost processor here,
        // it will resume here, so also need to reenable 
    }

}

/** ------------------------------------------------------
 |  Saves current process's state and gives processsor to
 |  highest priority ready process.
  -------------------------------------------------------*/
void relinquish_unconditional()
{
    // get current process and processor
    Process *oldproc = get_current();
    int pun = oldproc->pun;

    disable();

    // get highest-priority ready process for this processor
    Process *newproc = take(pun);

    // make it the current process on this processor
    set_current(newproc);

    // switch contexts from old proc to new proc
    switch_context(oldproc, newproc);

    enable();
    // new process will resume here, so also need to reenable 
}

/**---------------------------------------------------------------------
 *  The current process gives up the processor but remains ready.
 *--------------------------------------------------------------------*/
void yield()
{  
    // get current process and processor
    Process *oldproc = get_current();
    int pun = oldproc->pun;

    // disable interrupts 
    disable();

    // get highest-priority ready process for this processor
    // besides the idle process
    Process *newproc = take1(pun);
    if (newproc != NULL) {

        // make it the current process on this processor
        set_current(newproc);

        // put yielding process on its ready queue
        enqueue0(oldproc);

        // switch contexts from old proc to new proc
        switch_context(oldproc, newproc);
    } 

    // re-enable interrupts
    enable();
    // if new process lost processor here, it will 
    // resume here, so it will need to reenable
}

/** Handle incoming interrupt */
void handle_interrupt(int inum)
{
    // handle interprocessor interrupt
    if (inum == INTR_INTERPROC) {
        handle_interprocessor_interrupt();

    // handle timeout interrupt
    } else if (inum == INTR_TIMEOUT) {
        handle_timeout_interrupt();

    // handle elapsed time interrupt
    } else if (inum == INTR_ELAPSED) {
        handle_elapsed_time_interrupt();

    } else {
        handle_user_interrupt(inum - INTR_USER0);
    }
}

/**-----------------------------------------
 |  Logic of the idle process.
 -----------------------------------------*/
void idle()
{
    enable();
    while (true)
    {
        //phantom_enable();
        halt_processor();
    }
}

/**---------------------------------------------
 |  Completes the termination of a process. 
 |  (Reached from 'terminate' function.)
  ---------------------------------------------*/
static void finish_termination()
{
    // the current process is the terminating process
    // and is running with a temporary termination stack

    // get current process and its processor, locate
    // termination data structure
    Process *oldproc = get_current();
    int pun = oldproc->pun;
    Termination *term = &termination[pun];

    // free the process record (can do this now that
    // process is using termination stack)
    release_mem(oldproc->index, (byte *)oldproc);

    // disable interrupts
    disable();

    // release termination stack's mutex
    release_mutex(&term->mutex);

    // still have exclusive access to the termination stack
    // because interrupts are disabled; for same reason, have
    // exclusive access to the ready queue for this processor
    
    // get highest-priority ready process on this processor
    Process *newproc = take(pun);

    // make new process the current process
    set_current(newproc);

    // establish new process's context (which will restore the processor
    // flags, potentially reenabling interrupts)
    restore_context(newproc);

    // We needn't enable interrupts here, since it is the
    // terminating process that had them disabled, and it has
    // passed from existence.  But in general:
    //
    // A process can lose the processor when it tries to claim a
    // mutex, when it readies a higher-priority process and allows
    // it to preempt, and when it gets interrupted.  We never
    // try to claim a mutex with interrupts disabled, and
    // a process calls 'preempt' only with interrupts enabled,
    // so when this disruption of the process's execution occurs,
    // it is running with interrupts enabled in all cases.
    // Therefore when the process gains the processor again,
    // it needs to be restored with interrupts enabled.  However,
    // interrupts get disabled during the context switch in which
    // the processor loses the processor.  Thus we must reenable
    // interrupts whenever the process gets restored.
}

/**------------------------------------------------------
 |  Terminates the current process and does not return. 
  -----------------------------------------------------*/
void terminate()
{
    //     We must protect the termination stack of this processor
    // from use by another process while this process is terminating.
    // We do so by using both a mutex and by disabling interrupts.
    // We use the mutex in order not to keep interrupts disabled
    // any longer than necessary.  
    //     As long as processes on only a single processor access a
    // shared data structure, disabling interrupts is sufficient to
    // provide exclusive access to it.
    //     Note that we never claim a mutex with interrupts disabled,
    // because we might be forced to yield the processor.  If the
    // swapped-in process has interrupts enabled (as is likely), it
    // could allow non-exclusive access to the variables that were
    // protected by the disabled interrupts.  (And even if the
    // swapped-in process had interrupts disabled, we might end
    // up holding them disabled for longer than we would like.)
    
    // get current process and processor
    Process *curr = get_current();
    int pun = curr->pun;

    // claim exclusive access to the termination strucure for this
    // processor, so this process has sole use of the termination stack
    Termination *term = &termination[pun];
    claim_mutex(&term->mutex);

    // switch to termination stack to finish termination
    set_stack(curr, (byte *)&term->stack[pun], TERM_STACK_SIZE, finish_termination);
    // continue in the finish_termination routine
}

/** Initialize this module */
static void sched_init()
{
    // initialize interprocessor message queues
    ipq_init();

    // initialize rdyQues
    int pun;
    for (pun = 0; pun < NPUN; pun++) {
        rdyQues[pun].head = NULL;
    }

    // initialize termination structures
    for (pun = 0; pun < NPUN; pun++) {
        init_mutex(&termination[pun].mutex);
    }

}

/** Starts the run 
 *  tsize: size of total allocatable memory (bytes) 
 *  istacksize: stack size of initial process  */
void initialize(unsigned int tsize, unsigned int istacksize)
{
    // disallow interrupts during initialization
    disable();
 
    // initialize code that emulates bare hardware
    // (this call not needed in a bare hardware implementation)
    hardware_init();

    /*--------------------------------------------
     | initialize memory module
     -------------------------------------------*/
    memory_init(tsize - istacksize);
    // the currently executing code is the initial process
    // and is already using its stack, at the top of the block
    // of allocatable memory

    /*--------------------------------------------
     | initialize interrupts
     -------------------------------------------*/

    // initialize interrupt module
    interrupt_init();

    /*--------------------------------------------
     | initialize timer
     -------------------------------------------*/

    timer_init();

    /*--------------------------------------------
     | initialize this sched module
     -------------------------------------------*/

    sched_init();

    /*--------------------------------------------------------------
     |  build process infrastructure for the initial process
     -------------------------------------------------------------*/

    // build a process record for this, the initial process
    Process *proc = make_process(
        NULL, NULL, NULL, istacksize, INIT_PRI, 0, NULL);
    // (initial process is already using its stack, which is not
    // part of its process record...)

    // activate the initial processor
    activate_processor(0, proc);

    // make it the current process
    set_current(proc);

    // make idle process for this processor and put it on scheduling queue
    Process *idler = make_process(
        idle, NULL, NULL, IDLE_STACK_SIZE, IDLE_PRI, 0, NULL);
    enqueue0(idler);

    /*--------------------------------------------
     | activate the other processors
     -------------------------------------------*/
   
    // activate the other processors, starting each with an idle process
    int pun;
    for (pun = 1; pun < NPUN; pun++) {
        idler =  make_process(
           idle, NULL, NULL, IDLE_STACK_SIZE, IDLE_PRI, pun, NULL);
        current[pun] = idler;
        atomic_store_explicit(
            &current_pri[pun], IDLE_PRI, memory_order_release);
        activate_processor(pun, idler);
    } 

    // synch all processors at barrier
    synchronize_processors();
    
    // allow interrupts on this processor
    enable();
}

/** Does nothing. */
inline void noop() 
{
    // does nothing
}

/** Should not happen */
void plotz(char *what)
{
    printf("%s %d\n", what, errno);
    exit(1);
}

/** Should not happen */
void plotz2(char *what, int r)
{
    printf("%s %d\n", what, r);
    exit(1);
}
