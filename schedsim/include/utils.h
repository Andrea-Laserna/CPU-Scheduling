#ifndef UTILS_H
#define UTILS_H
#include "scheduler.h"

int enqueue_ready(SchedulerState *state, int idx);
int dequeue_ready(SchedulerState *state);
int remove_ready_by_process_idx(SchedulerState *state, int process_idx);
int select_next_process(SchedulerState *state, SchedulingAlgorithm algorithm);

#endif