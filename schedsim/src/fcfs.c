#include "scheduler.h"

int schedule_fcfs(SchedulerState *state) {
    // Stateless policy: pick next from ready queue
    if (state->ready_count == 0) return -1;

    int idx = state->ready_queue[state->ready_head];
    state->ready_head = (state->ready_head + 1) % state->ready_capacity;
    state->ready_count--;
    return idx;
}
