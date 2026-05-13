#include "scheduler.h"
/**
 * Round Robin Scheduler: Picks the next process to run.
 * Always pick the process at the head of the ready queue.
 */
int schedule_rr(SchedulerState *state) {
    // Safety Guard (Prevents SEGFAULT and empty queue errors)
    if (!state || state->ready_count == 0) {
        return -1;
    }

    // Round Robin Logic:
    // Because we use a circular ready queue, the person who "arrived first" 
    // or was most recently moved to the back is always at state->ready_head.
    
    int next_process_idx = state->ready_queue[state->ready_head];

    return next_process_idx;
}