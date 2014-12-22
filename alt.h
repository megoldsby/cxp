
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

#ifndef ALT_H
#define ALT_H

#include "types.h"
#include "comm.h"
#include "interrupt.h"
#include "sched.h"

// guard types
#define GUARD_CHAN       0
#define GUARD_SKIP       1
#define GUARD_TIMER      2
#define GUARD_INTERRUPT  3

// alt states
#define ALT_NONE     0
#define ALT_ENABLING 1
#define ALT_WAITING  2
#define ALT_READY    3
    
/** Guard */
typedef struct Guard {

    uint16 type;
    union {
        Channel *channel;                 
        Time time;
        Interrupt *interrupt;
    };

} Guard;

/** State info for alternative */
typedef struct Alternation {
    uint16 favorite;
    uint16 nrGuards;
    Guard *guards;
} Alternation;

/** Initializes channel guard */
inline void init_channel_guard(Guard *guard, Channel *chan);

/** Initializes skip guard */
inline void init_skip_guard(Guard *guard);

/** Initializes timer guard */
inline void init_timer_guard(Guard *guard, Time time);

/** Initializes interrupt guard */
inline void init_interrupt_guard(Guard *guard, Interrupt *interrupt);

/** Initializes alternation */
inline void init_alt(Alternation *alt, Guard *guards, int size);

/** Select first ready alternative */
int priSelect(Alternation *alt);

/** Select ready alternative */
int fairSelect(Alternation *alt);

/** Frees alting process if necessary */
void freeProcessMaybe(Process *proc);

#endif
