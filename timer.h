
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

#ifndef TIMER_H
#define TIMER_H

#include "types.h"
#include "comm.h"

// forward declaration
typedef struct TimeoutDesc TimeoutDesc;

/** Get elapsed time */
Time GetCurrentTime();

/** Get elapsed time */
Time Now();   // synonym for GetCurrentTime

/** Returns at given time */
void After(Time when);

/** Returns true if timeout ready */
_Bool timeout_ready(Time time);

/** Enables timeout for alternation */
_Bool enable_timeout(Time time, Process *proc);

/** Disables timeout for alternation */
_Bool disable_timeout(Time time, Process *proc);

/** Handles timer interrupt for elapsed time */
void handle_elapsed_time_interrupt();

/** Handles timer interrupt for timeouts */
void handle_timeout_interrupt();

/** Returns tick value */
Time getTick();

/** Initializes the timer module. */
void timer_init();



#endif

