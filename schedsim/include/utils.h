#ifndef UTILS_H
#define UTILS_H
#include "scheduler.h"

int enqueue_ready(SchedulerState *state, int idx);
int enqueue_ready_front(SchedulerState *state, int idx);
int dequeue_ready(SchedulerState *state);
int remove_ready_by_process_idx(SchedulerState *state, int process_idx);
int select_next_process(SchedulerState *state, SchedulingAlgorithm algorithm);
int try_dispatch(SchedulerState *state, Event **event_queue, SchedulingAlgorithm algo);
void cancel_event(Event **head, Process *p, EventType type);
void log_execution(SchedulerState *state, char *pid, int start, int end);

#endif