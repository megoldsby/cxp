
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

#include "comm.h"
#include "alt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbg.h"

/** Initializes given channel */
void init_channel(Channel *chan)
{
    init_mutex(&chan->mutex);
    chan->waiting = NULL;
    chan->src = NULL;
}

/** Reads from channel. */
void in(Channel *chan, Word *paramDest, uint len)
{
    Process *curr = get_current();
    claim_mutex(&chan->mutex);   // claim exclusive access to this channel
    if (chan->waiting != NULL)
    {
        Process *wasWaiting;

        // writer is ready: transfer data
        memcpy(paramDest, chan->src, len); 
        wasWaiting = chan->waiting;
        chan->waiting = NULL;
        chan->src = NULL;
        release_mutex(&chan->mutex);   // release exclusive access

        // make the sending process ready
        int old_state = atomic_exchange_explicit(
            &wasWaiting->sched_state, PROC_READY, memory_order_acq_rel);

        // if sending process was waiting, schedule it
        // (otherwise, state was PROC_PREPARING_TO_WAIT, and sender
        // is either executing or already in its scheduling queue)
        if (old_state == PROC_WAITING) {
            schedule(wasWaiting);
        }
    }
    else
    {
        // writer not ready, so relinquish processor and wait
        chan->dest = paramDest;
        chan->waiting = curr;
        PREPARE_TO_WAIT(curr);
        release_mutex(&chan->mutex);   // release exclusiv access
        relinquish();
        // when this process resumes, the io is done
        // and the process can continue from this point
    }
}

/** Reads from channel and returns true if can do so without waiting. */
_Bool try_in(Channel *chan, Word *paramDest, uint len)
{
    Process * wasWaiting;
    claim_mutex(&chan->mutex);   // claim exclusive access to this channel
    if (chan->waiting != NULL)
    {
        // writer is ready: transfer data and return true
        memcpy(paramDest, chan->src, len); 
        wasWaiting = chan->waiting;
        chan->waiting = NULL;
        chan->src = NULL;
        release_mutex(&chan->mutex);   // release exclusive access
        return true;
    }
    else
    {
        // writer not ready: just return false
        release_mutex(&chan->mutex);   // release exclusive access
        return false;
    }
}

/** Returns true if read would complete. */
_Bool chan_pending(Channel *chan)
{
    // pending if writer is waiting
    claim_mutex(&chan->mutex);     // claim exclusive access to this channel
    _Bool pending = (chan->waiting != NULL && chan->src != NULL);
    release_mutex(&chan->mutex);   // release exclusive access
    return pending;
}

/** Writes to channel */
void out(Channel *chan, Word *paramSrc, uint len)
{
    Process *curr = get_current();
    claim_mutex(&chan->mutex);  // claim exclusive access to this channel
    if (chan->waiting != NULL)
    {
        // reader is ready
        Process *wasWaiting;
        if (chan->dest != NULL) 
        {
            // normal output: transfer the data
            memcpy(chan->dest, paramSrc, len);
            wasWaiting = chan->waiting; 
            chan->waiting = NULL;
            chan->dest = NULL;
            release_mutex(&chan->mutex);  // release exclusive access

            // make the receiving process ready
            int old_state = atomic_exchange_explicit(
                &wasWaiting->sched_state, PROC_READY, memory_order_acq_rel);

            // if sending process was waiting, schedule it
            // (otherwise, state was PROC_PREPARING_TO_WAIT, and sender
            // is either executing or already in its scheduling queue)
            if (old_state == PROC_WAITING) {
                schedule(wasWaiting);
            }
        }
        else
        {
            // alternation (or extended input), so wait for
            // receiver to complete the io
            wasWaiting = chan->waiting;
            chan->waiting = curr;
            PREPARE_TO_WAIT(curr);
            chan->src = paramSrc;
            release_mutex(&chan->mutex);  // release exclusive access
            freeProcessMaybe(wasWaiting);
            relinquish();
            // when this process resumes, the io is done
            // and the process can continue from this point
        }
    }
    else
    {
        // receiver not ready, so relinquish processor and wait
        chan->src = paramSrc;
        chan->waiting = curr;
        PREPARE_TO_WAIT(curr);
        release_mutex(&chan->mutex);      // release exclusive access
        relinquish();
        // when this process resumes, the io is done
        // and the process can continue from this point
    }
}


/** 
 *  Enables channel for alt, returns true if channel ready. 
 *  input:  chan    the channel
 *          proc    the alting process
 *  output: true if writer ready on channel
 */
_Bool enable_channel(Channel *chan, Process *proc)
{
    claim_mutex(&chan->mutex);     // claim exclusive access to this channel 
    if (chan->waiting != NULL) {
        if (chan->waiting == proc) {
            // channel appears multiple times in alt,
            // the waiting process is us waiting to read
            release_mutex(&chan->mutex);   // release exclusive access
            return false;
        } else {
            // writer is ready
            release_mutex(&chan->mutex);   // release exclusive access
            return true;
        }
    } else {
        // put proc into channel 
        chan->waiting = proc;
        chan->dest = NULL;
        release_mutex(&chan->mutex);       // release exclusive access
        return false;
    }
}

/** 
 *  Disables channel for alt, returns true if channel ready. 
 *  input:  chan    the channel
 *          proc    the alting process
 *  output: true if writer ready on channel
 */
_Bool disable_channel(Channel *chan, Process *proc)
{
    claim_mutex(&chan->mutex);     // claim exclusive access to this channel 
    if (chan->waiting != NULL && chan->waiting != proc) {
        // writer ready for channel
        release_mutex(&chan->mutex);       // release exclusive access
        return true;
    } else {
        // either no one waiting or just us again
        chan->waiting = NULL;
        release_mutex(&chan->mutex);       // release exclusive access
        return false;
    }
}
