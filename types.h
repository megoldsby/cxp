
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

#ifndef TYPES_H
#define TYPES_H

#include <inttypes.h>
#include <stdbool.h>

// these definitions are architecture dependent
typedef unsigned char byte;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int64_t int64;
typedef uint32 Word;
typedef unsigned int uint; 
typedef Word Addr;
typedef int64 Time;
#define MAX_TIME 0xffffffffffffffff

/***
#define uint16 short
#define uint32 long int
#define uint64 long long int
#define uint unsigned int
#define Word uint64
#define Addr Word
***/

#define false  0
#define true   1

#endif
