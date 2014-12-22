
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

#include "par_barrier.h"
#include "sched.h"
#include <stdlib.h>
#include <stdio.h>
#include "dbg.h"

/** Initializes Par barrier (called by parent) */
static void par_barr_init(ParBarrier *barr, Process *parent, int nc)
{
    barr->parent = parent;
    barr->children = ATOMIC_VAR_INIT(nc);
}

/** Announces child is done (called by child) */
static void par_barr_sync(ParBarrier *barr)
{
    // if this is last child, awaken parent
    int pre = atomic_fetch_sub_explicit(
                &barr->children, 1, memory_order_acq_rel);
    if (pre == 1) {
        schedule(barr->parent);
    }
}
