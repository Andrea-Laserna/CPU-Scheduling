#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "utils.h"

/**
 * NEW: Cancels a specific type of event for a specific process.
 * Crucial for Round Robin to remove "Completion" events when a process is preempted.
 */
void cancel_event(Event **head, Process *p, EventType type) {
    if (!head || !*head) return;
    Event *curr = *head;
    Event *prev = NULL;

    while (curr) {
        if (curr->process == p && curr->type == type) {
            if (prev) prev->next = curr->next;
            else *head = curr->next;
            Event *temp = curr;
            curr = curr->next;
            free(temp);
            return; // Found and removed
        }
        prev = curr;
        curr = curr->next;
    }
}

/**
 * Helper: Inserts an event into the linked list while keeping the time sorted.
 * This ensures the simulation always processes the "earliest" event next.
 */
static void push_event_sorted(Event **head, Event *new_event) {
    if (!head || !new_event) return;

    if (!*head || new_event->time < (*head)->time) {
        new_event->next = *head;
        *head = new_event;
        return;
    }

    Event *curr = *head;
    while (curr->next && curr->next->time <= new_event->time) {
        curr = curr->next;
    }

    new_event->next = curr->next;
    curr->next = new_event;
}

/**
 * Inserts an event before existing events with the same timestamp.
 * Used for RR quantum-expire events so they can run before same-time arrivals.
 */
static void push_event_sorted_before_equal(Event **head, Event *new_event) {
    if (!head || !new_event) return;

    if (!*head || new_event->time < (*head)->time) {
        new_event->next = *head;
        *head = new_event;
        return;
    }

    Event *curr = *head;
    while (curr->next && curr->next->time < new_event->time) {
        curr = curr->next;
    }

    new_event->next = curr->next;
    curr->next = new_event;
}

/**
 * Initializes the simulation by creating an ARRIVAL event for every process.
 */
Event* initialize_events(SchedulerState *state) {
    if (!state) return NULL;

    Event *head = NULL;

    for (int i = 0; i < state->num_processes; i++) {
        Event *new_event = malloc(sizeof(Event));

        if (new_event == NULL) {
            fprintf(stderr, "Error: malloc failed during event initialization\n");
            while (head != NULL) {
                Event *temp = head;
                head = head->next;
                free(temp);
            }
            return NULL;
        }
        
        new_event->time = state->processes[i].arrival_time;
        new_event->type = EVENT_ARRIVAL;
        new_event->process = &state->processes[i];
        new_event->next = NULL;

        push_event_sorted(&head, new_event);
    }
    
    return head;
}

/**
 * Pulls the next event from the head of the sorted event queue.
 */
Event* pop_event(Event **head) {
    if (!head || !*head) return NULL;
    
    Event *temp = *head;
    *head = (*head)->next;

    return temp;
}

/**
 * Adds a process index to the tail of the circular ready queue.
 */
int enqueue_ready(SchedulerState *state, int idx) { 
    if (!state || state->ready_count == state->ready_capacity) return -1;
    
    state->ready_queue[state->ready_tail] = idx;
    state->ready_tail = (state->ready_tail + 1) % state->ready_capacity;
    state->ready_count++;
    return 0;
}

int enqueue_ready_front(SchedulerState *state, int idx) {
    if (!state || state->ready_count == state->ready_capacity) return -1;

    state->ready_head = (state->ready_head - 1 + state->ready_capacity) % state->ready_capacity;
    state->ready_queue[state->ready_head] = idx;
    state->ready_count++;
    return 0;
}

/**
 * Removes and returns the process index at the head of the circular ready queue.
 */
int dequeue_ready(SchedulerState *state) {
    if (!state || state->ready_count == 0) return -1;
    
    int idx = state->ready_queue[state->ready_head];
    state->ready_head = (state->ready_head + 1) % state->ready_capacity;
    state->ready_count--;
    return idx;
}

/**
 * The "Switchboard": Routes the scheduling decision to the specific algorithm logic.
 */
int select_next_process(SchedulerState *state, SchedulingAlgorithm algorithm) {
    if (!state) return -1;

    switch (algorithm) {
        case SCHED_FCFS:
            return schedule_fcfs(state);
        case SCHED_SJF:
            return schedule_sjf(state);
        case SCHED_STCF:
            return schedule_stcf(state);
        case SCHED_RR:
            return schedule_rr(state);
        default:
            return -1; 
    }
}

/**
 * Removes a specific process index from anywhere in the circular ready queue.
 * Used primarily by SJF and STCF to "pick" a specific job out of the line.
 */
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
    
    // Shift elements forward to fill the gap
    for (int i = found_pos; i < state->ready_count - 1; i++) {
        int from_phys = (head + i + 1) % cap;
        int to_phys = (head + i) % cap;
        state->ready_queue[to_phys] = state->ready_queue[from_phys];
    }

    state->ready_count--;
    state->ready_tail = (state->ready_head + state->ready_count) % cap;
    return process_idx;
}

void log_execution(SchedulerState *state, char *pid, int start, int end) {
    if (!state || !pid || !state->history || state->history_capacity <= 0) return;

    if (state->history_count >= state->history_capacity) {
        int new_capacity = state->history_capacity * 2;
        ExecutionSlice *new_history = realloc(state->history, sizeof(ExecutionSlice) * new_capacity);
        if (!new_history) return;
        state->history = new_history;
        state->history_capacity = new_capacity;
    }

    ExecutionSlice *slice = &state->history[state->history_count++];
    strncpy(slice->pid, pid, 15);
    slice->pid[15] = '\0';
    slice->start_time = start;
    slice->end_time = end;
}

/**
 * Attempts to move a process from the ready queue to the CPU.
 * Handle Completion or Quantum
 */
int try_dispatch(SchedulerState *state, Event **event_queue, SchedulingAlgorithm algo) {
    if (!state || !event_queue || state->running_index != -1) return 0;

    int next_idx = select_next_process(state, algo);
    if (next_idx == -1) return 0; 

    // Remove from ready queue
    if (algo == SCHED_FCFS || algo == SCHED_RR) dequeue_ready(state);
    else remove_ready_by_process_idx(state, next_idx);

    Process *p = &state->processes[next_idx];
    state->running_index = next_idx;
    state->last_dispatch_time = state->current_time;

    if (p->start_time == -1) p->start_time = state->current_time;

    Event *next_event = malloc(sizeof(Event));
    if (!next_event) return -1;

    // RR Logic: Schedule quantum expire if burst is longer than quantum
    if (algo == SCHED_RR && p->remaining_time > state->quantum) {
        next_event->time = state->current_time + state->quantum;
        next_event->type = EVENT_QUANTUM_EXPIRE;
    } else {
        next_event->time = state->current_time + p->remaining_time;
        next_event->type = EVENT_COMPLETION;
    }

    next_event->process = p;
    next_event->next = NULL;
    if (algo == SCHED_RR && next_event->type == EVENT_QUANTUM_EXPIRE) {
        push_event_sorted_before_equal(event_queue, next_event);
    } else {
        push_event_sorted(event_queue, next_event);
    }

    return 1;
}