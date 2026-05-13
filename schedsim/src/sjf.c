# include "scheduler.h"

int schedule_sjf(SchedulerState *state) {
    // Safety Guard (Prevents SEGFAULT and empty queue errors)
    if (!state || state->ready_count == 0) return -1;

    // Logic
    int shortest_idx = state->ready_queue[state->ready_head];
    Process *shortest = &state->processes[shortest_idx];

    for (int i = 1; i < state->ready_count; i++) {
        int current_idx = state->ready_queue[(state->ready_head + i) % state->ready_capacity];
        Process *current = &state->processes[current_idx];
        
        // Pick shorter burst, or earlier arrival if tied
        if (current->burst_time < shortest->burst_time ||
            (current->burst_time == shortest->burst_time && current->arrival_time < shortest->arrival_time)) {
            shortest_idx = current_idx;
            shortest = current;
        }
    }

    return shortest_idx;
}