
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

#ifndef MEMORY_H
#define MEMORY_H

#include "mutex.h"
#include "types.h"

/* 
 * We use Brinch Hansen's memory allocation algorithm.
 * Assume that the program is small, so that the size of every
 * possible storage allocation is known beforehand, and each
 * possible size can be assigned an index.  Thus there
 * no need for a fallback "hard" allocation scheme.
 */

// memory index of timeout descriptor
#define TMO_INDEX 0
 
// fwd decl of 'struct ChainedBlock' as type 'ChainedBlock'
typedef struct ChainedBlock ChainedBlock;

typedef struct ChainedBlock * ChainedBlock_p;

typedef struct ChainedBlock {

    ChainedBlock_p next;    
    byte data[];   // blocks can be various lengths

} ChainedBlock;

/*
 * Find index of smallest allocation >= given size 
 */
int find_mem_index(uint size);

/* 
 * Allocate block of length implied by index
 * input:   index    1..NALLOC-1
 * output:  array of words of size procmemlen[index]
 */
byte *allocate_mem(uint16 index);

/*
 * Release allocated block.
 * input:   index     1..NALLOC-1
 *          addr      ptr to allocated block
 */
void release_mem(uint16 index, byte *addr);

/*
 * Initializes the memory system.
 * input:    total   size of total dynamnic memory allocation, in bytes
 */
void memory_init(uint total);

#endif


