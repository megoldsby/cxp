
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

#ifndef SCHED_H
#define SCHED_H

#include "mutex.h"
#include "par_barrier.h"
#include "types.h"
#include <stdatomic.h>

#define NPUN  2             // number of processing units

// priority manipulation
// Priorities are assigned dynamically by the PRI PAR construct (see run.[hc]).
//     In order to fit the priority information into 16 bits, we place
// some restrictions on PRI PAR.  There can be up to four nested levesl of
// PRI PAR constructs, each of which can contain up to eight processes.
// (If these restrictions prove too restrictive, we can always increase the 
// priority field to 32 bits.)
//     The topmost PRI PAR operation is at level 0, the next PRI PAR done 
// by a descendant process is at level 1, and so forth.  Note that in any 
// such nesting, normal (non-PRI) PAR operations are transparent and do not 
// consume a level.
//     The priority field in the process record is 16 bits long,
// the high-order 3 bits of which are the level, and the low-order
// 12 bits of which are the actual priority value.  
#define PRI_LVL_MASK  0xe000 
#define PRI_VAL_MASK  0x0fff
#define pri_gt(p1,p2)  (((p1) & PRI_VAL_MASK) < ((p2) & PRI_VAL_MASK))
#define pri_lt(p1,p2)  (((p1) & PRI_VAL_MASK) > ((p2) & PRI_VAL_MASK))
#define pri_le(p1,p2)  (((p1) & PRI_VAL_MASK) >= ((p2) & PRI_VAL_MASK))
#define pri_ge(p1,p2)  (((p1) & PRI_VAL_MASK) <= ((p2) & PRI_VAL_MASK))
#define pri_eq(p1,p2)  (((p1) & PRI_VAL_MASK) == ((p2) & PRI_VAL_MASK))
#define priority(lvl,val)  (((lvl) << 13) | ((val) & PRI_VAL_MASK))
#define pri_level(p)   (((uint16)(p)) >> 13)
#define pri_value(p)   ((p) & PRI_VAL_MASK)
#define PRI_LEVELS     4
#define PRI_PROCS      8
#define PRI_LOW        0x0fff
#define PRI_HIGH       0
#define pri_delta(lvl) ((PRI_VAL_MASK+1)/(1 << (3*(lvl))))

#define INIT_PRI  PRI_HIGH    // priority of initial process (highest)
#define IDLE_PRI  PRI_LOW     // priority of idle process (lowest)
#define IDLE_STACK_SIZE 8192  // stack size of idle process

#define PREPARE_TO_WAIT(p) \
  atomic_store_explicit(&(p)->sched_state,  \
    PROC_PREPARING_TO_WAIT, memory_order_release); 

// forward declarations
typedef struct RdyQDesc RdyQDesc;
typedef struct Process Process;

/** pointers */
typedef Process * Process_p;
typedef RdyQDesc * RdyQDesc_p;
typedef void (*code_p)();

/** process scheduling states  */
#define PROC_READY              0
#define PROC_PREPARING_TO_WAIT  1
#define PROC_WAITING            2

/** process descriptor */
typedef struct Process  {
    Word *stackptr;             // pointer to top of stack 
    Process *next;              // next process in ready queue
    uint16 index;               // memory class of this process record  
    uint16 pri;                 // priority of this process               
    uint16 pun;                 // processor on which this process runs 
    _Atomic(uint8) alt_state;    // state when alting
    _Atomic(uint8) sched_state;  // scheduling state
    Word stack[];               // stack
} Process;
//Note: could get rid of 4 bytes in process record
//by limiting number of memory indexes to 256 and
//removing pun.

/** ready queue descriptor */
typedef struct RdyQDesc {
    Process *head;       // first process in queue
} RdyQDesc;

/** Starts the run. 
 *  total:  size of total allocatable memory (bytes) 
 *  stacksize: stack size of initial process */
void initialize(uint total, uint stacksize);

/** Potentially puts current process into waiting state and
 *  gives process to highest priority ready process.  */
void relinquish();

/** Saves current process's state and gives processor
 *  to highest priority ready process. */
void relinquish_unconditional();

/** Handles scheduling interrupt */
void handle_interprocesor_interrupt(int ipr);

/** Returns current process */
inline Process *get_current();

/** Moves current process to preparing-to-wait state */
inline void prepare_to_wait();

/** Sets current process */
void set_current();

/** Defines interrupt handlers for current processor */
void define_interrupt_handlers();

/** Handle incoming interrupt */
void handle_interrupt(int ilvl);

/** Inserts process into its scheduling queue */
void schedule(Process *proc);

/** Inserts process into its scheduling queue. 
 *  Interrupts must be disabled when calling this function. */
void schedule0(Process *proc);

/** Returns the highest priority ready process if it
 *  has priority higher than currently executing process
 *  and null otherwise. */
void schedule_from_interrupt();

/** Constructs a process and returns a pointer to the process record */
Process_p make_process(code_p code, void *arg1, void *arg2, 
    uint stacksize, uint pri, uint pun, void *userArgs);

/** Enqueue given process on scheduling queue */
void enqueue(Process *proc);

/** Enqueue given process on scheduling queue 
 *  Interrupts must be disabled when calling this function. */
void enqueue0(Process *proc);

/** Terminates the calling process and does not return. */
void terminate();

/** Does nothing */
inline void noop();

/** Should not happen */
void plotz(char *what);

/** Should not happen */
void plotz2(char *what, int r);

#endif
