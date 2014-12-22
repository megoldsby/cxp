
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

#ifndef HARDWARE_H
#define HARDWARE_H

#include "sched.h"

/*-----------------------------------------------------------------------
 |  The hardware module contains logic implemented in inline assembler
 +  and serves as a temporary stand-in by providing stubs for routines 
 |  that will later be implemented in assembler.
 *---------------------------------------------------------------------*/

/** disable/enable interrupts */
inline void disable();
inline void enable();
inline void phantom_enable();

#define INTR_ELAPSED   0    // elapsed time interrupt number
#define INTR_TIMEOUT   1    // timeout interrupt number
#define INTR_INTERPROC 2    // interprocessor interrupt number
#define INTR_USER0     3    // interprocessor interrupt number
#define INTR_USER1     4    // interprocessor interrupt number
#define NINTR   5           // number of distinct interrupts

// timer types
#define TIMER_ELAPSED  0    // elapsed-time timer
#define TIMER_TIMEOUT  1    // timeout timer
#define NTIMER         2    // number of distinct timer types

/** Sets handler for given processing unit and interrupt number */
typedef void (*interrupt_handler_t)(int);
interrupt_handler_t define_handler(int nintr, interrupt_handler_t handler);

/** Switch processor from one process to another. */
void switch_context(Process *oldproc, Process *newproc);

/** Switches the processor from one process to another, where the
 *  process losing the processor is in an interrupt handler. */
void switch_interrupt_context(Process *interrupted, Process *preempting);

/** Make current (terminating) process use given stack. */
void set_stack(Process *proc, byte *stack, uint stacksize, code_p code);

/** Give processor to a given process. */
void restore_context(Process *new);

/** Sets up process to begin executing with the given code and argument */
void build_context(Process *proc, uint stacksize, code_p code, 
                       void *arg1, void *arg2, void *userArgs);

/** Returns the processor number (0..NPUN-1) */
inline int getcpu();

/** Returns the address of a region of memory of the specified length */
char *acquire_memory(int bytes);

/** Halts processor, waiting for an interrupt. */
void halt_processor();

/** Sends interprocessor interrupt to given processor */
void send_interprocessor_interrupt(int pun);

/** Starts a processor, giving it a process to run. */
void activate_processor(int cpu, Process *proc);

/** Initialize the given timer. */
void init_timer(int timerId);

/** Set interval timer for a single interval. */
void set_timer_single(int timerId, Time time);

/** Set interval timer for a repeating interval. */
void set_timer_repeating(int timerId, Time time);

/** Read timer. */
Time read_timer(int timerId);

/** Send application-level interrupt as software interrupt */
void send_interrupt(int intr);

/** Synchonrizes all processors at barrier. */
void synchronize_processors();

/** Initializes this module. */
void hardware_init();

#endif

