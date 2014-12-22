
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

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "mutex.h"
#include "sched.h"
#include <stdatomic.h>


typedef struct Interrupt {
    _Atomic(Process *) waiting;
} Interrupt;

/** Initializes an Interrupt structure. */
void init_interrupt(Interrupt *interrupt);

/** Receiver waits for interrupt. */
void receive(int intr_no);

/** Called by interrupt handler to inform receiver
 *  of interrupt, returning process to resume if
 *  different from interrupted process. */
Process *transmit(int intr_no);

/** Initializes the interrupts */
void interrupt_init();

#endif
