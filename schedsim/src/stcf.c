#include "scheduler.h"

int schedule_stcf(SchedulerState *state){
    if (!state || state->ready_count == 0) return -1;

    // Assume that the first process in head is the best candidate
    int best_idx = state->ready_queue[state->ready_head];
    // Store index and add pointer to their data
    Process *best = &state->processes[best_idx];

    // Look at every other process in the queue
    for (int i = 1; i < state->ready_count; i++) {
        // Circular array to find the next process in line
        int qidx = state->ready_queue[(state->ready_head + i) % state->ready_capacity];
        // Get a pointer to that current process
        Process *cur = &state->processes[qidx];

        // Shortest time to completion rule
        // A: Is the current process' remaining time shorter than the current best, if yes wins
        // B: Tie-breaker: if same time left, check arrival time - whoever arrived first wins
        if (cur->remaining_time < best->remaining_time ||
            (cur->remaining_time == best->remaining_time && cur->arrival_time < best->arrival_time)) {
            // Save new best
            best_idx = qidx;
            best = cur;
        }
    }

    return best_idx;
}
