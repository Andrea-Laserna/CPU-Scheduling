#include <stdio.h>
#include "scheduler.h"

/**
 * MLFQ Picker: Scans the independent priority queues from top to bottom
 * and returns the next FIFO process from the first non-empty level.
 */
int schedule_mlfq(SchedulerState *state, MLFQConfig *config) {
    (void)config;
    if (!state) return -1;

    for (int level = 0; level < state->num_levels; level++) {
        FIFOQueue *queue = &state->mlfq_queues[level];
        if (queue->count > 0) {
            return queue->queue[queue->head];
        }
    }

    return -1;
}