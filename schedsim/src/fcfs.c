#include "scheduler.h"

int schedule_fcfs(SchedulerState *state) { //magreturn lng head 
    if (!state || state->ready_count == 0) return -1; // Added NULL check
    return state->ready_queue[state->ready_head];
}
