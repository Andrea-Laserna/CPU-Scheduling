#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "metrics.h"
#include "gantt.h"
#include "utils.h"

// Entry point
int main(int argc, char *argv[]) {

    SchedulerState state;
    state.current_time = 0;
    state.num_processes = 0;
    state.processes = NULL;

    SchedulingAlgorithm selected_algo = SCHED_FCFS; // Default
    char *input_file = NULL;

    // Parse Command Line Arguments
    for (int i = 1; i < argc; i++) {
        
        if (strncmp(argv[i], "--algorithm=", 12) == 0) {
            // Pointer to the text after "--algorithm="
            char *algo_name = argv[i] + 12;
            
            if (strcmp(algo_name, "FCFS") == 0) selected_algo = SCHED_FCFS;
            else if (strcmp(algo_name, "SJF") == 0) selected_algo = SCHED_SJF;
            else if (strcmp(algo_name, "STCF") == 0) selected_algo = SCHED_STCF;
            else if (strcmp(algo_name, "RR") == 0) selected_algo = SCHED_RR;
            else if (strcmp(algo_name, "MLFQ") == 0) selected_algo = SCHED_MLFQ;
        
        } else if (strncmp(argv[i], "--input=", 8) == 0) {
            input_file = argv[i] + 8;
        }
    }

    // Load Process Workload
    if (input_file) {

        state.processes = load_processes(input_file, &state.num_processes);
        
        if (!state.processes) {
            fprintf(stderr, "Error: Failed to load processes from %s\n", input_file);
            return -1;
        }

    } else {
        // Input file is required to run a simulation
        fprintf(stderr, "Error: No input file specified.\n");
        return -1;
    }

    state.ready_capacity = state.num_processes;
    state.ready_queue = malloc(sizeof(int) * state.ready_capacity);
    state.ready_head = 0;
    state.ready_tail = 0;
    state.ready_count = 0;
    state.running_index = -1;
    state.completed_count = 0;

    printf("Loaded %d processes from %s\n", state.num_processes, input_file);
    printf("Starting Simulation...\n");
    
    // Run the Discrete-Event Simulator
    simulate_scheduler(&state, selected_algo);

    // Cleanup
    free(state.processes);
    free(state.ready_queue);
    
    return 0;
}




static int process_index_from_ptr(SchedulerState *state, Process *p) {
    return (int)(p - state->processes);
}

static int push_completion_event(Event **event_queue, int when, Process *p) {
    Event *done = malloc(sizeof(Event));
    if (!done) return -1;

    done->time = when;
    done->type = EVENT_COMPLETION;
    done->process = p;
    done->next = NULL;

    if (!*event_queue || done->time < (*event_queue)->time) {
        done->next = *event_queue;
        *event_queue = done;
    } else {
        Event *cur = *event_queue;
        while (cur->next && cur->next->time <= done->time) cur = cur->next;
        done->next = cur->next;
        cur->next = done;
    }
    return 0;
}

static void try_dispatch(SchedulerState *state, Event **event_queue, SchedulingAlgorithm algorithm) {
    // If CPU idle, dispatch immediately
    if (state->running_index != -1) return; 

    int next = select_next_process(state, algorithm);
    if (next == -1) return; // No process ready to run
    
    if (remove_ready_by_process_idx(state, next) == -1) return;
    
    Process *p = &state->processes[next];
    if (p->start_time == -1) p->start_time = state->current_time;
    state->running_index = next;

    int completion_time = state->current_time + p->remaining_time;
    (void) push_completion_event(event_queue, completion_time, p); //TODO: Check return value in case of malloc failure.

    
}

static void handle_arrival(SchedulerState *state, Process *process, Event **event_queue, SchedulingAlgorithm algorithm) { // doesn't handle preemption yet
    int idx = process_index_from_ptr(state, process);
    enqueue_ready(state, idx); // TODO: Check  return value in case queue is full.

    try_dispatch(state, event_queue, algorithm);
}

static void handle_completion(SchedulerState *state, Process *process, Event **event_queue, SchedulingAlgorithm algorithm) {
    int idx = process_index_from_ptr(state, process);
    Process *p = &state->processes[idx];

    p->remaining_time = 0;
    p->finish_time = state->current_time;
    state->completed_count++;
    state->running_index = -1;

    try_dispatch(state, event_queue, algorithm);
}



// Core Simulation Engine: orchestrates the events

void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    
    Event *event_queue = initialize_events(state);

    while (event_queue != NULL) {
        Event *current = pop_event(&event_queue);
        state->current_time = current->time;

        switch (current->type) {
            case EVENT_ARRIVAL:
                handle_arrival(state, current->process, &event_queue, algorithm);
                break;
            case EVENT_COMPLETION:
                handle_completion(state, current->process, &event_queue, algorithm);
                break;
            // ... handle other events 
        }
        free(current);
    }
    

    // calculate_metrics and print_results will be in metrics.c and gantt.c
    calculate_metrics(state);
    print_gantt_chart(state);
    print_metrics(state);
}