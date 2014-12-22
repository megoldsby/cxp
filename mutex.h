
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

#ifndef MUTEX_H
#define MUTEX_H

#include "types.h"
#include <stdatomic.h>

typedef struct Mutex { 
    atomic_bool claimed;
} Mutex;

void init_mutex(Mutex *mutex);

void claim_mutex(Mutex *mutex);

inline void release_mutex(Mutex *mutex);


#endif
