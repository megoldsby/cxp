
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

#ifndef RUN_H
#define RUN_H

#include "sched.h"

#define DEFAULT_STACKSIZE 64

/** Runs processes in parallel on same processing 
 *  unit as parent and at same priority as parent */
void par(code_p code[], void *userArgs[], uint size[], uint np);

/** Runs processes in parallel on same processing
 *  unit as parent with assigned priorities */
void par_pri(code_p code[], void *userArgs[], uint size[], uint np);

/** Runs processes in parallel on designated processors
 *  at same priority as parent. */
void placed_par(
    code_p code[], void *userArgs[], uint size[], uint16 pun[], uint np);

/** Runs processes in parallel on designated processors
 *  and with assigned priorities */
void placed_par_pri(
    code_p code[], void *userArgs[], uint size[], uint16 pun[], uint np); 

#endif
