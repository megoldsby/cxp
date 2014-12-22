
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

#include "memory.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include "dbg.h"

// maximum number of memory allocation sizes
#define NALLOC  24

//blocks that have been allocated and released
static ChainedBlock_p procmemlist[NALLOC];  

// length of block for each index, in bytes
static uint16 procmemlen[] = 
  { 18, 32, 48, 96, 128, 192, 256, 384, 512, 768, 1024, 1536,
   2048, 3072, 4096, 6144, 8192, 10240, 12288, 16384, 24576, 0 };
// readjust these depending on the application.

// what's left, in one big block
static uint32 taillen;    // in bytes
static byte *tail;                 

// used to ensure mutually exclusive access
static Mutex mutex_mem;

/**
 * Find index of smallest allocation >= given size (bytes)
 */
int find_mem_index(uint size)
{
    int i;
    for (i = 0; procmemlen[i]; i++) {
        if (procmemlen[i] >= size)
            break;
    }
    if (procmemlen[i])
        return i;
    else
        plotz("No memory block large enough");
}

/** 
 * Allocate block of length implied by index
 * input:   index    1..NALLOC-1
 * output:  array of words of size corresponding to index
 */
byte *allocate_mem(uint16 index)
{
    claim_mutex(&mutex_mem);                
    ChainedBlock_p block = procmemlist[index];
    if (block != NULL)
    {
        //previously allocated block available, remove it from list
        procmemlist[index] = block->next;
    }
    else 
    {
        uint16 len = procmemlen[index];
        if (taillen >= len)
        {
            // tail is big enough, take block from tail
            block = (ChainedBlock_p)tail;
            tail = tail + len;
        }
        else
        {
            // out of memory
            block = NULL;
            plotz("Out of memory");
        }
    }
    
    release_mutex(&mutex_mem);
    return (byte *)block;
}

/**
 * Release allocated block.
 * input:   index     1..NALLOC-1
 *          addr      ptr to allocated block
 */
void release_mem(uint16 index, byte *addr)
{
    // put released block at head of list for its size
    claim_mutex(&mutex_mem);
    ChainedBlock_p block = (ChainedBlock_p)addr;
    block->next = procmemlist[index];
    procmemlist[index] = block;
    release_mutex(&mutex_mem);
}

/**
 * Initializes the memory system.
 * input:    total   size of total dynamic memory allocation, in bytes
 */
void memory_init(uint total)
{
    // set allocatable lengths, none allocated yet
    int i;
    for (i = 0; i < NALLOC; i++)
    {
        procmemlist[i] = NULL;
    }
    
    tail = acquire_memory(total); 
    taillen = total;
}


