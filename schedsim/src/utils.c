#include <stdlib.h>
#include "scheduler.h"
#include "utils.h"
// Event - node in the queue that records the process
// State - processes, num_processes, current_time

Event* initialize_events(SchedulerState *state) {
    /*
    issue to fix: check malloc return value first incase of failure
    */

    Event *head = NULL;

    // Create one ARRIVAL event for each process
    for (int i = 0; i < state->num_processes; i++) {
        Event *new_event = malloc(sizeof(Event));
        new_event->time = state->processes[i].arrival_time;
        new_event->type = EVENT_ARRIVAL;
        new_event->process = &state->processes[i];
        new_event->next = NULL;

        // Insert into event queue in sorted order by time
        if (!head || new_event->time < head->time) {
            new_event->next = head;
            head = new_event;
        } else {
            Event *curr = head;
            while (curr->next && curr->next->time <= new_event->time) {
                curr = curr->next;
            }
            new_event->next = curr->next;
            curr->next = new_event;
        }
    }
    
    return head;
}

// Pulls the next event from the queue
Event* pop_event(Event **head) {
    // Guard against NULL pointer input or empty list
    if (!head || !*head) return NULL;
    
    Event *temp = *head;
    *head = (*head)->next;

    return temp;
}

// Enqueue a process index into the ready queue
int enqueue_ready(SchedulerState *state, int idx) { 
    if (state->ready_count == state->ready_capacity) return -1;
    state->ready_queue[state->ready_tail] = idx;
    state->ready_tail = (state->ready_tail + 1) % state->ready_capacity;
    state->ready_count++;
    return 0;
}

int dequeue_ready(SchedulerState *state) {
    if (state->ready_count == 0) return -1;
    int idx = state->ready_queue[state->ready_head];
    state->ready_head = (state->ready_head + 1) % state->ready_capacity;
    state->ready_count--;
    return idx;
}

int select_next_process(SchedulerState *state, SchedulingAlgorithm algorithm) {
    switch (algorithm) {
        case SCHED_FCFS:
            return schedule_fcfs(state);
        case SCHED_SJF:
            return schedule_sjf(state);
        case SCHED_STCF:
            return schedule_stcf(state);
        // case SCHED_RR:
        //     // For RR, we would also need to pass the quantum, but let's assume it's a fixed value for now
        //     return schedule_rr(state, 4); // Example quantum of 4
        // case SCHED_MLFQ:
        //     // For MLFQ, we would need to pass the configuration, but let's assume it's predefined for now
        //     return schedule_mlfq(state, NULL); // Placeholder for MLFQ
        default:
            return -1; // Invalid algorithm
    }
}

// Remove a specific process index from the ready queue 
int remove_ready_by_process_idx(SchedulerState *state, int process_idx) {
    if (!state || state->ready_count <= 0) return -1;

    int found_pos = -1;
    for (int i = 0; i < state->ready_count; i++) {
        int phys = (state->ready_head + i) % state->ready_capacity;
        if (state->ready_queue[phys] == process_idx) {
            found_pos = i;
            break;
        }
    }
    if (found_pos == -1) return -1;

    int cap = state->ready_capacity;
    int head = state->ready_head;
    for (int i = found_pos; i < state->ready_count - 1; i++) {
        int from_phys = (head + i + 1) % cap;
        int to_phys = (head + i) % cap;
        state->ready_queue[to_phys] = state->ready_queue[from_phys];
    }

    state->ready_count--;
    state->ready_tail = (state->ready_head + state->ready_count) % cap;
    return process_idx;
}