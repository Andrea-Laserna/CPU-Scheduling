#include <stdio.h>
#include "scheduler.h"

/**
 * MLFQ Picker: Scans the ready_queue (array of indices) 
 * and selects the process with the highest priority.
 */
int schedule_mlfq(SchedulerState *state, MLFQConfig *config) {
    (void)config; // Unused parameter
    
    // Safety guard
    if (!state || state->ready_count == 0) return -1;

    int best_idx = 0;
    int best_priority = state->processes[state->ready_queue[0]].priority;

    // Scan all processes in ready_queue and find highest priority (lowest number)
    for (int i = 1; i < state->ready_count; i++) {
        int p_idx = state->ready_queue[i]; 
        Process *p = &state->processes[p_idx];

        if (p->priority < best_priority) {
            best_priority = p->priority;
            best_idx = i;
        }
    }

    // Return the actual process index (not the position in ready_queue)
    return state->ready_queue[best_idx];
}