
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

//for testing
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>

#include "hardware.h"
#include "par_barrier.m"
#include "run.h"
#include "sched.h"
#include <stdatomic.h>
#include <stdio.h>
#include "dbg.h"


/** ----------------------------------------------------
 |  Runs a process in a par (parallel) construct 
 *----------------------------------------------------*/
static void run_in_par(void *code, void *barr, void *userArgs)
{
    // run process's code to completion, then sync on barrier
    (*(code_p)code)(userArgs);
    par_barr_sync((ParBarrier *)barr);
    terminate();        // does not return
}

/** -------------------------------------------------------------
 |  Runs child processes in parallel on same processor as parent
 |  and with same priority as parent. 
 *-------------------------------------------------------------*/
void par(code_p code[], void *userArgs[], uint stacksize[], uint nc)
{
    // current process is parent
    Process *parent = get_current();

    // initialize barrier
    ParBarrier barrier;
    par_barr_init(&barrier, parent, nc);

    // enter preparing-to-wait state
    PREPARE_TO_WAIT(parent);

    // build and schedule each child process
    int i;
    for (i = 0; i < nc; i++) {
        Process *proc = make_process(run_in_par, 
                                     code[i], 
                                     (void *)&barrier, 
                                     stacksize[i], 
                                     parent->pri, 
                                     parent->pun,
                                     userArgs[i]);
        enqueue(proc);
    }

    // barrier will awaken parent when all children are done
    relinquish();
}

/** ---------------------------------------------------------------------
 |   Runs child processes in parallel on same processing unit as parent
 |   with descending priorities starting with parent priority. 
 *---------------------------------------------------------------------*/
void par_pri(code_p code[], void *userArgs[], uint stacksize[], uint nc)
{
    // see if there are too many child processes to fit priority scheme
    if (nc > PRI_PROCS) {
        plotz("Too many child processes");
    }

    // current process is parent
    Process *parent = get_current();

    // initialize barrier
    ParBarrier barrier;
    par_barr_init(&barrier, parent, nc);

    // child's priority level is parent's plus 1
    int level = pri_level(parent->pri) + 1;

    // see if level exceeds allowable number of levels
    if (level > PRI_LEVELS) {
        plotz("Too many PRI levels");
    }

    // enter preparing-to-wait state
    PREPARE_TO_WAIT(parent);

    // compute gap between child priorities
    int delta = pri_delta(level);

    // build and schedule each child process
    int i;
    int pri = parent->pri;
    for (i = 0; i < nc; i++) {
        Process *proc = make_process(run_in_par, 
                                     code[i], 
                                     (void *)&barrier, 
                                     stacksize[i], 
                                     priority(level, pri),
                                     parent->pun,
                                     userArgs[i]);
        pri += delta;
        enqueue(proc);
    }

    // barrier will awaken parent when all children are done
    relinquish();
}

/** ------------------------------------------------------------
 |   Runs processes in parallel on designated processors
 |   at same priority as parent. 
  -------------------------------------------------------------*/
void placed_par(
    code_p code[], void *userArgs[], uint stacksize[], uint16 pun[], uint nc)
{
    // current process is parent
    Process *parent = get_current();

    // initialize barrier
    ParBarrier barrier;
    par_barr_init(&barrier, parent, nc);

    // enter preparing-to-wait state
    PREPARE_TO_WAIT(parent);

    // build and schedule each child process
    int i;
    for (i = 0; i < nc; i++) {
        Process *proc = make_process(run_in_par, 
                                     code[i], 
                                     (void *)&barrier, 
                                     stacksize[i], 
                                     parent->pri, 
                                     pun[i],
                                     userArgs[i]);

        if (pun[i] == parent->pun) {
            enqueue(proc);
        } else {
            schedule(proc);
        }
    }

    // barrier will awaken parent when all children are done
    relinquish();
}

/** -----------------------------------------------------------------
 |   Runs child processes in parallel on designated processors 
 |   with descending priorities starting with parent priority. 
 *-----------------------------------------------------------------*/
void placed_par_pri(
    code_p code[], void *userArgs[], uint stacksize[], uint16 pun[], uint nc)
{
    // see if there are too many child processes to fit priority scheme
    if (nc > PRI_PROCS) {
        plotz("Too many child processes");
    }

    // current process is parent
    Process *parent = get_current();

    // initialize barrier
    ParBarrier barrier;
    par_barr_init(&barrier, parent, nc);

    // child's priority level is parent's plus 1
    int level = pri_level(parent->pri) + 1;

    // see if level exceeds allowable number of levels
    if (level > PRI_LEVELS) {
        plotz("Too many PRI levels");
    }

    // enter preparing-to-wait state
    PREPARE_TO_WAIT(parent);

    // compute gap between child priorities
    int delta = pri_delta(level);

    // build and schedule each child process
    int i;
    int pri = parent->pri;
    for (i = 0; i < nc; i++) {
        Process *proc = make_process(run_in_par, 
                                     code[i], 
                                     (void *)&barrier, 
                                     stacksize[i], 
                                     priority(level, pri),
                                     pun[i],
                                     userArgs[i]);
        pri += delta;

        if (pun[i] == parent->pun) {
            enqueue(proc);
        } else {
            schedule(proc);
        }
    }

    // barrier will awaken parent when all children are done
    relinquish();
}

