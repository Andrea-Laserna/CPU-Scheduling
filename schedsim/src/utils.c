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
void push_event_sorted(Event **head, Event *new_event) {
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

int enqueue_priority(SchedulerState *state, int level, int idx) {
    if (!state || level < 0 || level >= state->num_levels) return -1;

    PriorityQueue *queue = &state->mlfq_queues[level];
    if (!queue->queue || queue->count == queue->capacity) return -1;

    queue->queue[queue->tail] = idx;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    return 0;
}

int dequeue_priority(SchedulerState *state, int level) {
    if (!state || level < 0 || level >= state->num_levels) return -1;

    PriorityQueue *queue = &state->mlfq_queues[level];
    if (!queue->queue || queue->count == 0) return -1;

    int idx = queue->queue[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return idx;
}

void clear_priority_queues(SchedulerState *state) {
    if (!state) return;

    for (int i = 0; i < state->num_levels && i < MAX_LEVELS; i++) {
        state->mlfq_queues[i].head = 0;
        state->mlfq_queues[i].tail = 0;
        state->mlfq_queues[i].count = 0;
    }
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
        case SCHED_MLFQ:
            return schedule_mlfq(state, NULL);
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
    if (start >= end) return; // Don't log zero-length slices

    // Check if we need to allocate or resize the history array
    if (state->history == NULL) {
        state->history_capacity = 100; // Start with 100 slices
        state->history = malloc(sizeof(ExecutionSlice) * state->history_capacity);
    } else if (state->history_count >= state->history_capacity) {
        state->history_capacity *= 2;
        state->history = realloc(state->history, sizeof(ExecutionSlice) * state->history_capacity);
    }

    // Log the data
    ExecutionSlice *slice = &state->history[state->history_count++];
    strncpy(slice->pid, pid, sizeof(slice->pid) - 1);
    slice->pid[sizeof(slice->pid) - 1] = '\0';
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
    if (algo == SCHED_FCFS || algo == SCHED_RR) {
        dequeue_ready(state);
    } else if (algo == SCHED_MLFQ) {
        dequeue_priority(state, state->processes[next_idx].priority);
    } else {
        remove_ready_by_process_idx(state, next_idx);
    }

    Process *p = &state->processes[next_idx];
    state->running_index = next_idx;
    state->last_dispatch_time = state->current_time;

    if (p->start_time == -1) p->start_time = state->current_time;

    Event *next_event = malloc(sizeof(Event));
    if (!next_event) return -1;

    // --- UPDATED LOGIC HERE ---
    
    // 1. Determine the effective quantum for this process
    int current_quantum = -1;
    if (algo == SCHED_RR) {
        current_quantum = state->quantum;
    } else if (algo == SCHED_MLFQ) {
        // Use the quantum for this process's priority level
        current_quantum = state->quantums[p->priority];
    }

    // Only schedule a Quantum Expire if current_quantum is positive
    if (current_quantum > 0 && p->remaining_time > current_quantum) {
        next_event->time = state->current_time + current_quantum;
        next_event->type = EVENT_QUANTUM_EXPIRE;
    } else {
        next_event->time = state->current_time + p->remaining_time;
        next_event->type = EVENT_COMPLETION;
    }
    // --------------------------

    next_event->process = p;
    next_event->next = NULL;

    // Maintain stable event ordering
    if (next_event->type == EVENT_QUANTUM_EXPIRE) {
        push_event_sorted_before_equal(event_queue, next_event);
    } else {
        push_event_sorted(event_queue, next_event);
    }

    return 1;
}