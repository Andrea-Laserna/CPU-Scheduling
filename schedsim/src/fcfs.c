#include "scheduler.h"

int schedule_fcfs(SchedulerState *state) { //magreturn lng head 
    if (state->ready_count == 0) return -1;
    return state->ready_queue[state->ready_head];
}
