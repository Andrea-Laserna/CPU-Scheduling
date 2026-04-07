#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "metrics.h"
#include "gantt.h"
#include "utils.h"

/**
 * Helper to clear any leftover events in the linked list.
 */
void clear_event_queue(Event **head) {
    while (*head != NULL) {
        Event *temp = *head;
        *head = (*head)->next;
        free(temp);
    }
}

/**
 * Maps a Process pointer to its index in the processes array.
 */
static int process_index_from_ptr(SchedulerState *state, Process *process) {
    if (!state || !state->processes || !process) return -1;
    for (int i = 0; i < state->num_processes; i++) {
        if (&state->processes[i] == process) return i;
    }
    return -1;
}

/**
 * Handles process completion.
 */
static void handle_completion(SchedulerState *state, Process *process) {
    if (!state || !process || state->running_index == -1) return;

    int idx = process_index_from_ptr(state, process);
    if (state->running_index != idx) return; // Guard against stale events

    // Record the final slice for the Gantt chart
    log_execution(state, process->pid, state->last_dispatch_time, state->current_time);

    process->remaining_time = 0;
    process->finish_time = state->current_time;
    state->running_index = -1;
    state->completed_count++;
}

/**
 * Handles a new process arrival.
 */
static int handle_arrival(SchedulerState *state, Process *process) {
    int idx = process_index_from_ptr(state, process);
    if (idx == -1) return -1;
    
    if (enqueue_ready(state, idx) == -1) {
        fprintf(stderr, "Fatal Error: Ready queue overflow.\n");
        return -1; 
    }
    return 0;
}

/**
 * STCF Preemption logic.
 */
static void maybe_preempt_stcf(SchedulerState *state) {
    if (!state || state->running_index == -1) return;

    int candidate_idx = schedule_stcf(state);
    if (candidate_idx == -1) return;

    Process *running = &state->processes[state->running_index];
    Process *candidate = &state->processes[candidate_idx];

    if (candidate->remaining_time < running->remaining_time) {
        int elapsed = state->current_time - state->last_dispatch_time;
        running->remaining_time -= (elapsed > 0) ? elapsed : 0;
        
        // Record the partial slice
        log_execution(state, running->pid, state->last_dispatch_time, state->current_time);
        
        enqueue_ready(state, state->running_index);
        state->running_index = -1;
    }
}

/**
 * Round Robin Preemption logic.
 * Fixed: Re-queues the process BEFORE new arrivals are handled in the main loop.
 */
static void maybe_preempt_rr(SchedulerState *state, Event **event_queue) {
    if (!state || state->running_index == -1) return;

    int elapsed = state->current_time - state->last_dispatch_time;

    // Check if the process has reached or exceeded its quantum
    if (elapsed >= state->quantum) {
        Process *running = &state->processes[state->running_index];
        
        // Remove the future completion event since we are preempting now
        cancel_event(event_queue, running, EVENT_COMPLETION);

        // Record the execution slice for the Gantt Chart
        log_execution(state, running->pid, state->last_dispatch_time, state->current_time);

        running->remaining_time -= elapsed;
        if (running->remaining_time < 0) running->remaining_time = 0;

        if (running->remaining_time > 0) {
            if (event_queue && *event_queue && (*event_queue)->time == state->current_time && (*event_queue)->type == EVENT_ARRIVAL) {
                enqueue_ready_front(state, state->running_index);
            } else {
                enqueue_ready(state, state->running_index);
            }
        } else {
            running->finish_time = state->current_time;
            state->completed_count++;
        }

        state->running_index = -1;
    }
}

/**
 * Core Simulation Engine.
 */
void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    Event *event_queue = initialize_events(state);
    
    while (event_queue != NULL) {
        Event *current = pop_event(&event_queue);
        state->current_time = current->time;

        int status = 0;
        switch (current->type) {
            case EVENT_ARRIVAL:
                status = handle_arrival(state, current->process);
                break;
            case EVENT_COMPLETION:
                handle_completion(state, current->process);
                break;
            case EVENT_QUANTUM_EXPIRE:
                maybe_preempt_rr(state, &event_queue);
                break;
            default: break;
        }

        if (status == -1) {
            free(current);
            break; 
        }

        if (algorithm == SCHED_STCF) {
            maybe_preempt_stcf(state);
        }

        // Try to put a process on the CPU if it's idle
        try_dispatch(state, &event_queue, algorithm);
        
        free(current);
    }

    calculate_metrics(state);
    print_gantt_chart(state);
    print_metrics(state);
    clear_event_queue(&event_queue);
}

int main(int argc, char *argv[]) {
    SchedulerState state;
    state.current_time = 0;
    state.num_processes = 0;
    state.processes = NULL;
    state.ready_queue = NULL; 
    state.quantum = 4; 
    
    // Initialize History for Gantt Chart
    state.history_count = 0;
    state.history_capacity = 100;
    state.history = malloc(sizeof(ExecutionSlice) * state.history_capacity);

    SchedulingAlgorithm selected_algo = SCHED_FCFS; 
    char *input_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--algorithm=", 12) == 0) {
            char *algo_name = argv[i] + 12;
            if (strcmp(algo_name, "FCFS") == 0) selected_algo = SCHED_FCFS;
            else if (strcmp(algo_name, "SJF") == 0) selected_algo = SCHED_SJF;
            else if (strcmp(algo_name, "STCF") == 0) selected_algo = SCHED_STCF;
            else if (strcmp(algo_name, "RR") == 0) selected_algo = SCHED_RR;
            else if (strcmp(algo_name, "MLFQ") == 0) selected_algo = SCHED_MLFQ;
        } else if (strncmp(argv[i], "--input=", 8) == 0) {
            input_file = argv[i] + 8;
        } else if (strncmp(argv[i], "--quantum=", 10) == 0) {
            state.quantum = atoi(argv[i] + 10);
        }
    }

    if (!input_file) return -1;

    state.processes = load_processes(input_file, &state.num_processes);
    if (!state.processes) return -1;

    state.ready_capacity = state.num_processes + 1;
    state.ready_queue = malloc(sizeof(int) * state.ready_capacity);
    
    state.ready_head = state.ready_tail = state.ready_count = 0;
    state.running_index = -1;
    state.completed_count = 0;
    state.last_dispatch_time = 0;

    if (selected_algo == SCHED_MLFQ) {
        if (schedule_mlfq(&state, NULL) != 0) {
            free(state.processes);
            free(state.ready_queue);
            free(state.history);
            return -1;
        }
    } else {
        simulate_scheduler(&state, selected_algo);
    }

    // Cleanup
    free(state.processes);
    free(state.ready_queue);
    free(state.history);
    
    return 0;
}