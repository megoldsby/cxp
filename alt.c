
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
#include "timer.h"
#include "types.h"
#include <stdio.h>
#include <stdatomic.h>
#include "dbg.h"


// forward references
static void altEnabling(Process *proc);
static _Bool altShouldWait(Process *proc);
static void altFinish(Process *proc);

// earliest timeout in alt
#define NO_TIME MAX_TIME

/** Initializes channel guard */
inline void init_channel_guard(Guard *guard, Channel *chan) {
    guard->type = GUARD_CHAN;
    guard->channel = chan;
}

/** Initializes skip guard */
inline void init_skip_guard(Guard *guard) {
    guard->type = GUARD_SKIP;
}

/** Initializes timer guard */
inline void init_timer_guard(Guard *guard, Time time) {
    guard->type = GUARD_TIMER;
    guard->time = time;
}

/** Initializes interrupt guard */
inline void init_interrupt_guard(Guard *guard, Interrupt *interrupt) {
    guard->type = GUARD_INTERRUPT;
    guard->interrupt = interrupt;
}

/** Initializes alternation */
inline void init_alt(Alternation *alt, Guard *guards, int size) {
    alt->favorite = 0;
    alt->guards = guards;
    alt->nrGuards = size;
}

/** Select first ready alternative */
int priSelect(Alternation *alt)
{  
    int selected = -1;
    Process *proc = get_current();

    // mark process 'enabling'
    altEnabling(proc);

    // keep track of earliest timeout (if any) in Alt
    Time earliest = NO_TIME;

    int i;
    for (i = 0; i < alt->nrGuards; i++)
    {
        _Bool ready;
        Guard *guard = &(alt->guards[i]);
        switch (guard->type) 
        {
            case GUARD_CHAN:
                ready = enable_channel(guard->channel, proc);
                if (ready) goto Found_pri;
                break;
            case GUARD_SKIP:
                // skip guard always ready
                goto Found_pri;  
                break;
            case GUARD_TIMER:
                if (guard->time < earliest) {
                    earliest = guard->time;
                }
                ready = enable_timeout(guard->time, proc);
                if (ready) goto Found_pri;
                break;
        }
    }
    i--;   // step back one

    // have not found ready guard

    // if there was a timeout guard, enable the timeout
    if (earliest != NO_TIME) 
    {
        _Bool ready = enable_timeout(earliest, proc);
        if (ready) goto Found_pri;
    }

    // still no ready guard

    // if alt not ready, relinquish processor
    if (altShouldWait(proc)) {
        relinquish_unconditional();
    }
    // when reach here, alt is ready 

Found_pri:
    for (; i >=0; i--)
    {
        _Bool ready;
        Guard *guard = &(alt->guards[i]);
        switch (guard->type)
        {
            case GUARD_CHAN:
                ready = disable_channel(guard->channel, proc);
                if (ready) selected = i;
                break;
            
            case GUARD_SKIP:
                // skip guard always ready
                selected = i;   
                break;

            case GUARD_TIMER:
                ready = disable_timeout(guard->time, proc);
                if (ready) selected = i;
                break;
        }
    }

    // mark this process finished with the alt
    altFinish(proc);

    // save starting point in case next use of
    // this alt is for a fairSelect
    alt->favorite = selected + 1;
    if (alt->favorite >= alt->nrGuards) 
        alt->favorite -= alt->nrGuards;

    // return index of selected branch of alt
    return selected;
}

/** Select ready alternative */
int fairSelect(Alternation *alt)
{  
    int selected = -1;
    Process *proc = get_current();

    // allow favorite to have an uninitialized value on first fairSelect
    //alt->favorite = abs(alt->favorite) % alt->nrGuards;

    // mark process 'enabling'
    altEnabling(proc);

    // keep track of earliest timeout (if any) in Alt
    Time earliest = NO_TIME;

    int k;
    int i;
    for (k = 0, i = alt->favorite; 
         k < alt->nrGuards; 
         k++, i = (i + 1) % alt->nrGuards) 
    {
        _Bool ready;
        Guard *guard = &(alt->guards[i]);
        switch (guard->type) 
        {
            case GUARD_CHAN:
                ready = enable_channel(guard->channel, proc);
                if (ready) goto Found_fair;
                break;
            case GUARD_SKIP:
                // skip guard always ready
                goto Found_fair;  
                break;
            case GUARD_TIMER:
                if (guard->time < earliest) {
                    earliest = guard->time;
                }
                ready = enable_timeout(guard->time, proc);
                if (ready) goto Found_fair;
                break;
        }
    }
    k--;
    i = (i - 1 + alt->nrGuards) % alt->nrGuards;

    // have not found ready guard

    // if there was a timeout guard, enable the timeout
    if (earliest != NO_TIME) 
    {
        _Bool ready = enable_timeout(earliest, proc);
        if (ready) goto Found_fair;
    }

    // still no ready guard

    // if alt not ready, relinquish processor
    if (altShouldWait(proc)) {
        relinquish_unconditional();
    }
    // when reach here, alt is ready 

Found_fair:
    for (; k >= 0; k--, i = (i - 1 + alt->nrGuards) % alt->nrGuards)
    {
        _Bool ready;
        Guard *guard = &(alt->guards[i]);
        switch (guard->type)
        {
            case GUARD_CHAN:
                ready = disable_channel(guard->channel, proc);
                if (ready) selected = i;
                break;
            
            case GUARD_SKIP:
                // skip guard always ready
                selected = i;   
                break;

            case GUARD_TIMER:
                ready = disable_timeout(guard->time, proc);
                if (ready) selected = i;
                break;
        }
    }

    // mark this process finished with the alt
    altFinish(proc);

    // save starting point in case next use of
    // this alt is for a fairSelect
    alt->favorite = selected + 1;
    if (alt->favorite >= alt->nrGuards) 
        alt->favorite -= alt->nrGuards;

    // return index of selected branch of alt
    return selected;
}

void freeProcessMaybe(Process *proc)
{
    // attempt to transition from Enabling to Ready
    uint8 expected = ALT_ENABLING; 
    _Bool successful = atomic_compare_exchange_strong_explicit(
        &proc->alt_state, &expected, ALT_READY,
        memory_order_acq_rel, memory_order_relaxed);

    // The actual (old) value gets store in 'expected'
    // If were successful (actual value was Enabling),
    // the value is now Ready and the process is running,
    // so we can just return.

    // If were not successful, the actual value may
    // be Ready, Waiting, or None.  
    // If Ready, someone beat us to it and we can just return.
    // If None, the alting process has finished with the Alt
    // and we can just return.

    // If Waiting, try to transition form there to Ready
    if (expected == ALT_WAITING) {

        successful = atomic_compare_exchange_strong_explicit(
            &proc->alt_state, &expected, ALT_READY,
            memory_order_acq_rel, memory_order_relaxed);
        
        // If successful, we changed state from Waiting to Ready
        // and need to schedule the process for execution
        // (possibly preempting this one)
        if (successful) {
            schedule(proc);
        }
        // If not successful, someone beat us to it, so just return
    }
}

/** Transition to Enabling state. */
static void altEnabling(Process *proc)
{
    atomic_store_explicit(&proc->alt_state, ALT_ENABLING, memory_order_release);
}

/** Try to transition from Enabling to Waiting and return
 *  indication of success. */
static _Bool altShouldWait(Process *proc)
{
    uint8 expected = ALT_ENABLING;
    _Bool successful = atomic_compare_exchange_strong_explicit(
        &proc->alt_state, &expected, ALT_WAITING,
        memory_order_acq_rel, memory_order_relaxed);
    // if not successful, state must be READY
    return successful;   
}

/** Transition to not alting. */
static void altFinish(Process *proc)
{
    atomic_store_explicit(&proc->alt_state, ALT_NONE, memory_order_release);
}
