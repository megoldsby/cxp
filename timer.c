
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

#include "alt.h"
#include "hardware.h"
#include "memory.h"
#include "sched.h"
#include "timer.h" 
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbg.h"

/**
 * We assume there are two interval timers,
 * one for program-requested timeouts and
 * the other for updating the elapsed time.
 */

// define interval between elapsed time timer interrupts
static Time Tick = 1000000000ULL;   // time units (nsec) per tick
//static Time Tick = 1000000000000ULL;   // time units (nsec) per tick

/** Timer list entry */
#define TMO_AFTER   0
#define TMO_ALTING  1
typedef struct TimeoutDesc {

    Time time;               // time of expiration
    Process *proc;           // process expecting timeout
    TimeoutDesc *next;       // next timeout in timer's list
    uint16 type;             // AFTER or ALTING

} TimeoutDesc;

/** Timer queue descriptor */
typedef struct TimerQDesc {

    TimeoutDesc *head;

} TimerQDesc;

/** current elapsed time */
static _Atomic(Time) currentTime = ATOMIC_VAR_INIT(0);

/** queue of timeout requests, in time order */
static TimerQDesc timeQue[NPUN];

/** Returns max of two times. */
//static inline Time max(Time a, Time b) { return (a > b ? a : b); }

/** Returns min of two times. */
//static inline Time min(Time a, Time b) { return (a < b ? a : b); }

/** Returns current time. */
Time GetCurrentTime()
{
    Time t, c, cc;
    c = atomic_load_explicit(&currentTime, memory_order_acquire); 
    do {
        t = read_timer(TIMER_ELAPSED);
        cc = c;
        c = atomic_load_explicit(&currentTime, memory_order_acquire); 
    } while (c != cc);
    // same currentTime twice in a row ensures t is good

    return (Tick - t) + c;
}

/** Synonum for GetCurrentTime */
Time Now() {
    return GetCurrentTime();
}

/** Inserts timeout descriptor in timer queue in time order */
static void insertInQueue(TimerQDesc *tque, TimeoutDesc *desc)
{
    disable();

    // find timeout's place in the queue
    TimeoutDesc *prev = NULL;
    TimeoutDesc *curr = tque->head;
    while (curr != NULL && desc->time >= curr->time) {
        prev = curr;
        curr = curr->next;
    } 

    // insert timeout into the queue
    if (prev == NULL && curr == NULL) {
        // only entry in queue
        tque->head = desc;
        desc->next = NULL;
        Time now = GetCurrentTime();
        Time diff = desc->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);
//should I use absolute time for this?

    } else if (prev == NULL && curr != NULL) {
        // entry is first in queue but has a successor
        tque->head = desc;
        desc->next = curr;
        Time now = GetCurrentTime();
        Time diff = desc->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);

    } else if (prev != NULL && curr != NULL) {
        // entry has a predecessor and a successor in the queue
        prev->next = desc;
        desc->next = curr;

    } else /* prev != NULL && curr == NULL */ {
        // entry is last but has a predecessor
        prev->next = desc;
        desc->next = NULL;
    }

    enable();
}

/** Removes an entry from the timeout queue and returns it */
static TimeoutDesc *removeFromQueue(TimerQDesc *tque, Time time, Process *proc)
{
    // ensure exclusive access on this processor
    disable();

    // find entry in queue
    TimeoutDesc *prev = NULL;
    TimeoutDesc *curr = tque->head;            
    while (curr != NULL && curr->time < time) {
        prev = curr;
        curr = curr->next;    
    }
    // curr == NULL || curr->time >= time
    while (curr != NULL && curr->time == time && curr->proc != proc) {
        prev = curr;
        curr = curr->next;
    }
    // curr == NULL || curr->time > time || curr->proc == proc

    // remove entry from queue
    if (curr != NULL && curr->time == time && curr->proc == proc) 
    {
        if (prev != NULL)  
            prev->next = curr->next;
        if (curr == tque->head)
            tque->head = curr->next;
        // could restart timer if remove head entry..
    } 

    // release exclusive access on this processor
    enable();

    // return removed item (null if none)
    return curr;
}

/** Removes head entry from timer queue and returns it. */
static TimeoutDesc *removeHead(TimerQDesc *tque)
{
    // called with interrupts disabled
    TimeoutDesc *desc = tque->head;
    tque->head = tque->head->next;
    return desc;
}

/** Returns after given time */
void After(Time when) 
{
    if (when > GetCurrentTime()) {
        int pun = getcpu();
        Process *proc = get_current();
        TimerQDesc *tque = timeQue + pun;
        TimeoutDesc desc;
        desc.time = when;
        desc.proc = proc;
        desc.type = TMO_AFTER;
        desc.next = NULL;
        PREPARE_TO_WAIT(proc);
        insertInQueue(tque, &desc);
        relinquish();
        // when resume here, timeout has expired
    }
}

/** Returns true if timeout is ready */
_Bool timeout_ready(Time time) 
{
    return (GetCurrentTime() >= time);
}

/** Enables timer for alternation */
_Bool enable_timeout(Time time, Process *proc)
{
    _Bool ready = (GetCurrentTime() >= time);    
    if (!ready) {
        TimeoutDesc *desc = (TimeoutDesc *)allocate_mem(TMO_INDEX);
        desc->time = time;
        desc->proc = proc;
        desc->type = TMO_ALTING;
        insertInQueue(timeQue + proc->pun, desc);
    }
    return ready;
}

/** Disables timer for alternation */
_Bool disable_timeout(Time time, Process *proc)
{
    _Bool ready = (GetCurrentTime() >= time);    
    TimeoutDesc *desc = removeFromQueue(timeQue + proc->pun, time, proc);
    if (desc != NULL) {
        release_mem(TMO_INDEX, (byte *)desc);
    }
    return ready;
}

/** Handles timer interrupt for elapsed time */
void handle_elapsed_time_interrupt()
{
    /**----------------------------------------------
     | reach this routine with interrupts disabled
      ---------------------------------------------*/

    // increment current time
    atomic_fetch_add_explicit(&currentTime, Tick, memory_order_acq_rel);
}

/** Frees non-alting process */
static void freeProcess(Process *proc)
{
    // make it ready
    int old_state = atomic_exchange_explicit(
        &proc->sched_state, PROC_READY, memory_order_acq_rel);

    // if process was waiting, put it on its scheduling queue
    if (old_state == PROC_WAITING) {
        enqueue0(proc);
    }
}

/** Frees alting process if appropriate */
static void maybeFreeAltingProcess(Process *proc)
{
    // attempt transition from Enabling to Ready
    uint8 expected = ALT_ENABLING;
    _Bool successful = atomic_compare_exchange_strong_explicit(
        &proc->alt_state, &expected, ALT_READY,
        memory_order_acq_rel, memory_order_relaxed);
    // if successful, the process is running, so can just return
    // if not successful, the actual value may be Ready, Waiting
    // or None.  If Ready, someone beat us to it and we can just
    // return.  If None, the process is finished with the Alt
    // and we can just return.
            
    // if found process waiting, attempt transition from
    // Waiting to Ready
    if (expected == ALT_WAITING) {
        successful = atomic_compare_exchange_strong_explicit(
            &proc->alt_state, &expected, ALT_READY,
            memory_order_acq_rel, memory_order_relaxed);

        // if successful, we found process waiting and need
        // to put it on its ready queue
        if (successful) {
            enqueue0(proc);
        }
        // if not successful, someone already put the process
        // in the ready state, so just return
    }
}

/** Handles timer interrupt for timeouts */
void handle_timeout_interrupt()
{
    /**----------------------------------------------
     | reach this routine with interrupts disabled
      ---------------------------------------------*/

    // get timeout request queue
    Process *curr = get_current();
    int pun = curr->pun;
    TimerQDesc *tque = timeQue + pun;

    // learn current time
    Time now = GetCurrentTime();

    // ready processes whose timeouts are due
    TimeoutDesc *head = tque->head;
    while (head != NULL && head->time <= now)
    {
        // this one is due, so remove it from queue
        removeHead(tque);    

        // if an After requested this timeout, make waiting process ready
        if (head->type == TMO_AFTER) {

            freeProcess(head->proc);

        // if an Alt requested this timeout, make waiting process ready
        // if it isn't already ready
        } else if (head->type == TMO_ALTING) {

            maybeFreeAltingProcess(head->proc);

        }  else  plotz("handle_timeout_interrupt invalid type");

        // resume at head of queue
        head = tque->head;
    }
    // done with timeouts

    // set time for next interrupt, if any
    if (tque->head != NULL) {
        Time diff = tque->head->time - now;
        set_timer_single(TIMER_TIMEOUT, diff);
    }

    // perform preemption if necessary
    schedule_from_interrupt();
}

/** Returns tick value (time units per tick). */
Time getTick() 
{
    return Tick;
}

void timer_init()
{
    // initialize queue of timeout requests
    int pun;
    for (pun = 0; pun < NPUN; pun++) {
        timeQue[pun].head = NULL;
    }
}

