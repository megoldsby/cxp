
#ifndef PAR_BARRIER
#define PAR_BARRIER

typedef struct Process Process;

typedef struct ParBarrier {
    _Atomic(int) children;
    Process *parent;
} ParBarrier;

#endif

