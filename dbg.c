
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

#include "dbg.h"
#include "timer.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FILENAME  "trace"
#define LOCKNAME  "lock"
#define TRACEMAX  0x40000000
#define MARGIN    1024

static int fd = -1;

static bool add_time = false;
static bool initial = true;

static pthread_mutex_t mutex;
static char *trace;
static int end;
static int pos = 0;
static bool wrapped = false;

void start_timed_trace()
{
    add_time = true;
}

void report(char *format, ...)                    
{                                          
    if (initial) {
        initial = false;
        trace = (char *)malloc(TRACEMAX); 
        end = TRACEMAX;
        pthread_mutex_init(&mutex, NULL);
    }

    pthread_mutex_lock(&mutex);

    // wrap around if necessary
    if (pos >= end - MARGIN) {
        end = pos;
        pos = 0; 
        wrapped = true;
    }

    // write time then given fields to trace buffer
    unsigned long long now;
    if (add_time) {
        now = Now();
        pos += sprintf(&trace[pos], "%llu ", now);
    }
    va_list va;
    va_start(va, format);
    pos += vsprintf(&trace[pos], format, va);
    va_end(va);

    pthread_mutex_unlock(&mutex);
}                                        

/** Dump accumulated trace to file and stdout */
void display()
{
    FILE *f = fopen(FILENAME, "a"); 
    int p, q;
    if (wrapped) {  // trace has wrapped around
        p = q = pos + 1;
        while (p < end) {
            p += printf("%s", &trace[p]); 
            q += fprintf(f, "%s", &trace[q]);
        }
    }
    p = q = 0;
    while (p < pos) {
        p += printf("%s", &trace[p]); 
        q += fprintf(f, "%s", &trace[q]);
    }
    fclose(f);
    pos = 0;
    end = TRACEMAX;
    wrapped = false;
}

