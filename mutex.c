
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

#include "mutex.h"

#define TRIALS_BEFORE_YIELD 5

void init_mutex(Mutex *mutex)
{
    mutex->claimed = ATOMIC_VAR_INIT(false);
}

void claim_mutex(Mutex *mutex)
{
    while (true) {
        _Bool oldvalue;
        int i = 0;
        do {
            oldvalue = atomic_exchange_explicit(&mutex->claimed, true, memory_order_acq_rel); 
            i++;
        } while (oldvalue == true && i < TRIALS_BEFORE_YIELD);
        if (oldvalue == false) {
            break;
        }
        yield();
    } 
}


inline void release_mutex(Mutex *mutex)
{  
    atomic_store_explicit(&mutex->claimed, false, memory_order_release);
}
