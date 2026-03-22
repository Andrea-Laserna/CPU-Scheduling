#include "scheduler.h"

int schedule_stcf(SchedulerState *state){
    if (!state || state->ready_count == 0) return -1;

    int best_idx = state->ready_queue[state->ready_head];
    Process *best = &state->processes[best_idx];

    for (int i = 1; i < state->ready_count; i++) {
        int qidx = state->ready_queue[(state->ready_head + i) % state->ready_capacity];
        Process *cur = &state->processes[qidx];

        if (cur->remaining_time < best->remaining_time ||
            (cur->remaining_time == best->remaining_time && cur->arrival_time < best->arrival_time)) {
            best_idx = qidx;
            best = cur;
        }
    }

    return best_idx;
}
