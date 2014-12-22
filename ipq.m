
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
#include <stddef.h>

/**
 *  Interprocessor queues.
 *  These circular queues convey pointers to Process records
 *  from one processing unit to another.  When unit A makes
 *  a process P that runs on unit B ready, it puts a pointer
 *  to P in B's queue, so that B will become aware that 
 *  it should schedule P.
 */  

/*------------------------------------------------------------------------
 |  ipq_add may spin waiting for queue to be nonfull, so call it
 +  with interrupts enabled.  ipq_remove does not spin, so can
 |  call it with interrupts disabled.
 *----------------------------------------------------------------------*/

typedef _Atomic(Process*) Atomic_Process_p;

/** Interprocessor queue descriptor */
#define QSIZE 5
typedef struct IPQueue {
    _Atomic(Atomic_Process_p*) nextAdd;    // ptr to slot of next add
    _Atomic(Atomic_Process_p*) nextRem;    // ptr to slot of next removal
    Atomic_Process_p que[QSIZE];          // queue entries
} IPQueue;

// nextAdd = nextRem means the queue is empty
// nextRem = (nextAdd + 1) mod QSIZE means the queue is full
// (the queue is considered full when it has QSIZE-1 entries)

/** the interprocessor queues */
static IPQueue ipQues[NPUN];

/** Initializes the interprocessor queues. */
static void ipq_init()
{
    int pun;
    for (pun = 0; pun < NPUN; pun++) {
        IPQueue *ipq = &ipQues[pun];
        ipq->nextAdd = ATOMIC_VAR_INIT(&ipq->que[0]);
        ipq->nextRem = ATOMIC_VAR_INIT(&ipq->que[0]);
        int s;
        for (s = 0; s < QSIZE; s++) {
            ipq->que[s] = ATOMIC_VAR_INIT(NULL);
        }
    }
}

/** Add an entry to an interprocessor queue */
static void ipq_add(int pun, Process *proc)
{
    // get queue for given processor
    IPQueue *ipq = &ipQues[pun];

    Atomic_Process_p *a;
    Atomic_Process_p *r;
    Atomic_Process_p *a1;
ipq_try:
    // make sure queue nonfull (at least for an instant..)
    do {
        a = atomic_load_explicit(&ipq->nextAdd, memory_order_acquire);
        r = atomic_load_explicit(&ipq->nextRem, memory_order_acquire);
        if (a != &ipq->que[QSIZE-1]) {
            a1 = a + 1;
        } else {
            a1 = &ipq->que[0];
        }
    } while (r == a1);
    // queue was nonfull

    _Bool success = atomic_compare_exchange_strong_explicit(
        &ipq->nextAdd, &a, a1, 
        memory_order_acq_rel, memory_order_acquire);

    if (success) {
        // store proc into the queue at slot pointed to by a
        atomic_store_explicit(a, proc, memory_order_release);

        // inform the target processor
        send_interprocessor_interrupt(pun);
        // current_pri is now a superfluous variable, unless or until
        // I implement a me-first mutex and revise this logic
        // could this swamp the target processor?
    } else {
        goto ipq_try;
    }
}

/** Remove the next entry from an interprocessor queue and return it */
static Process *ipq_remove()
{
    Process* proc = NULL;     // returned value

    // get queue for this processor
    IPQueue *ipq = &ipQues[getcpu()];

    // if nonempty, take value from slot (and set slot null)
    Atomic_Process_p *r = atomic_load_explicit(&ipq->nextRem, memory_order_acquire);
    // can I use memory_order_relaxed on the above load?
    Atomic_Process_p *a = atomic_load_explicit(&ipq->nextAdd, memory_order_acquire);
    if (a != r) {
        proc = atomic_exchange_explicit(r, NULL, memory_order_acquire);
    }
    // The add operation increments nextAdd before storing the value
    // into the queue slot, so slot may momentarily be null

    // if got a value, increment ptr to next slot
    if (proc != NULL) {
        Atomic_Process_p *r1;
        if (r != &ipq->que[QSIZE-1]) {
            r1 = r + 1;
        } else {
            r1 = &ipq->que[0];
        }
        atomic_store_explicit(&ipq->nextRem, r1, memory_order_release);
        // releases slot to receive a new entry
dbg("  nextRem now %x\n", (uint)r1);
    }

    return proc;
}
