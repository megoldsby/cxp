
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

#include "interrupt.h"
#include "hardware.h"
#include <stdlib.h>

/*-------------------------------------------------------------------
 |  A process that wishes to react to a certain interrupt calls
 |  'receive' with that interrupt number.  When the
 |  interrupt fires, the interrupt handler calls 'transmit'
 |  on the same interrupt number.
 |
 |  A process may receive interrupts only from its own processor.
 |
 |  This module uses no mutex, so the interrupt handler cannot
 |  lose the processor trying to claim a mutex.
 |    
 |  If the process does not complete the two atomic actions
 |  in 'receive' before the interrupt fires, it will miss
 |  the interrupt.
 +-----------------------------------------------------------------*/

static Interrupt interrupts[NPUN][NINTR];

/** Initialize the interrupt module */
void interrupt_init()
{
    // initialize interrupt structures
    int pun, intr;
    for (pun = 0; pun < NPUN; pun++) {
        for (intr = 0; intr < NINTR; intr++) {
            interrupts[pun][intr].waiting = ATOMIC_VAR_INIT(NULL);
        }
    }
}

/** Initializes an interrupt structure. */
void init_interrupt(Interrupt *interrupt) 
{
    interrupt->waiting = ATOMIC_VAR_INIT(NULL);
}


/** Receiver waits for specified interrupt. */
void receive(int intr_no)
{
    // current process
    Process *curr = get_current();

    // get the interrupt's Interrupt struct
    Interrupt *interrupt = &interrupts[curr->pun][intr_no];
 
    // enter preparing-to-wait state
    PREPARE_TO_WAIT(curr);

    // let it be known that this process is waiting to receive the interrupt
    atomic_store_explicit(&interrupt->waiting, curr, memory_order_release);

    // relinquish the processor
    relinquish();
}

/** Informs receiver of specified interrupt and returns
 *  the process to be resumed when the handler returns  
 *  if it is different from the interrupted process.  
 *  Called by interrupt handler with interrupts disabled. */
Process *transmit(int intr_no)
{
    // get the interrupt's Interrupt struct
    Process *curr = get_current();
    Interrupt *interrupt = &interrupts[curr->pun][intr_no];

    // find out if anyone was waiting to receive interrupt
    Process *receiver = (Process *)atomic_exchange_explicit(
                      &interrupt->waiting, NULL, memory_order_acq_rel);

    // if so..
    if (receiver != NULL) {

        // set receiving process's state to ready
        int old_state = atomic_exchange_explicit(
            &receiver->sched_state, PROC_READY, memory_order_acq_rel);

        // if receiver was waiting..
        if (old_state == PROC_WAITING) {

            // put it on ready queue
            enqueue0(receiver);

            // preempt if necessary
            schedule_from_interrupt();
        }
    }

    // return interrupt receiving process (null if none)
    return receiver;
}
