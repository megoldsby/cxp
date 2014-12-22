
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

#ifndef COMM_H
#define COMM_H

#include "sched.h"
#include "types.h"
#include "mutex.h"

typedef struct Channel {  

    Mutex mutex;
    Process * waiting;
    union {
        Word *src;
        Word *dest;
    };
    uint len;

} Channel;

/** Reads from channel */
void in(Channel *chan, Word *paramDest, uint len);

/** Initializes given channel */
void init_channel(Channel *chan);

/** Writes to channel */
void out(Channel *chan, Word *paramSrc, uint len);

/** Reads from channel and returns true if can do so without waiting */
_Bool try_in(Channel *chan, Word *paramSrc, uint len);

/** Returns True if read would complete */
//_Bool chan_pending(Channel *chan);

/** Enables channel for alt, returns True if channel ready. */
_Bool enable_channel(Channel *chan, Process *proc);

/** Disables channel for alt, returns True if channel ready. */
_Bool disable_channel(Channel *chan, Process *proc);


#endif
