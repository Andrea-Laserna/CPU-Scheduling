#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "metrics.h"

// Entry point
int main(int argc, char *argv[]) {
    // Initialize simulation state
    SchedulerState state;
    // Simulator starts at time 0
    state.current_time = 0;
    // No processes loaded yet
    state.num_processes = 0;
    // Process array is empty until load_processes succeeds
    state.processes = NULL;

    // Default algorithm if user does not pass --algorithm
    SchedulingAlgorithm selected_algo = SCHED_FCFS; // Default
    // Will store value passed through --input=... argument
    char *input_file = NULL;

    // Parse Command Line Arguments
    for (int i = 1; i < argc; i++) {
        // Parse --algorithm=<NAME>
        if (strncmp(argv[i], "--algorithm=", 12) == 0) {
            // Pointer to the text after "--algorithm="
            char *algo_name = argv[i] + 12;
            // Map algorithm string to enum used internally.
            if (strcmp(algo_name, "FCFS") == 0) selected_algo = SCHED_FCFS;
            else if (strcmp(algo_name, "SJF") == 0) selected_algo = SCHED_SJF;
            else if (strcmp(algo_name, "STCF") == 0) selected_algo = SCHED_STCF;
            else if (strcmp(algo_name, "RR") == 0) selected_algo = SCHED_RR;
            else if (strcmp(algo_name, "MLFQ") == 0) selected_algo = SCHED_MLFQ;
        // Parse --input=<PATH>
        } else if (strncmp(argv[i], "--input=", 8) == 0) {
            // Pointer to the text after "--input=".
            input_file = argv[i] + 8;
        }
    }

    // Load Process Workload
    if (input_file) {
        // This is where workload loading should happen
        // TODO: enable once integrated with the rest of the simulator
        state.processes = load_processes(input_file, &state.num_processes);
    } else {
        // Input file is required to run a simulation
        fprintf(stderr, "Error: No input file specified.\n");
        return 1;
    }

    state.ready_capacity = state.num_processes;
    state.ready_queue = malloc(sizeof(int) * state.ready_capacity);
    state.ready_head = 0;
    state.ready_tail = 0;
    state.ready_count = 0;
    state.running_index = -1;
    state.completed_count = 0;

    // Run the Discrete-Event Simulator
    // Confirms that argument parsing completed and run is about to start
    printf("Loaded %d processes from %s\n", state.num_processes, input_file);
    printf("Starting Simulation...\n");
    // Dispatch into the simulation engine
    simulate_scheduler(&state, selected_algo);

    // Cleanup
    // Safe even when state.processes is NULL
    free(state.processes);
    
    return 0;
}




static int process_index_from_ptr(SchedulerState *state, Process *p) {
    return (int)(p - state->processes);
}

static int enqueue_ready(SchedulerState *state, int idx) {
    if (state->ready_count == state->ready_capacity) return -1;
    state->ready_queue[state->ready_tail] = idx;
    state->ready_tail = (state->ready_tail + 1) % state->ready_capacity;
    state->ready_count++;
    return 0;
}

static int dequeue_ready(SchedulerState *state) {
    if (state->ready_count == 0) return -1;
    int idx = state->ready_queue[state->ready_head];
    state->ready_head = (state->ready_head + 1) % state->ready_capacity;
    state->ready_count--;
    return idx;
}




static void handle_arrival(SchedulerState *state, Process *process, Event **event_queue) { // doesn't handle preemption yet
    int idx = process_index_from_ptr(state, process);
    enqueue_ready(state, idx); 

    // If CPU idle, dispatch immediately
    if (state->running_index == -1) {
        int next = schedule_fcfs(state);
        if (next != -1) {
            Process *p = &state->processes[next];
            if (p->start_time == -1) p->start_time = state->current_time;
            state->running_index = next;

            Event *done = malloc(sizeof(Event));
            if (!done) return; // handle allocation failure better in final version : Sometimes, if the computer is completely out of RAM, malloc() fails and returns NULL
            done->time = state->current_time + p->remaining_time;
            done->type = EVENT_COMPLETION;
            done->process = p;
            done->next = NULL;

            // insert in time order (same style as initialize_events)
            if (!*event_queue || done->time < (*event_queue)->time) {
                done->next = *event_queue;
                *event_queue = done;
            } else {
                Event *cur = *event_queue;
                while (cur->next && cur->next->time <= done->time) cur = cur->next;
                done->next = cur->next;
                cur->next = done;
            }
        }
    }
}

static void handle_completion(SchedulerState *state, Process *process, Event **event_queue) {
    int idx = process_index_from_ptr(state, process);
    Process *p = &state->processes[idx];

    p->remaining_time = 0;
    p->finish_time = state->current_time;
    state->completed_count++;
    state->running_index = -1;

    // Dispatch next FCFS if any
    int next = schedule_fcfs(state);
    if (next != -1) {
        Process *n = &state->processes[next];
        if (n->start_time == -1) n->start_time = state->current_time;
        state->running_index = next;

        Event *done = malloc(sizeof(Event));
        if (!done) return;
        done->time = state->current_time + n->remaining_time;
        done->type = EVENT_COMPLETION;
        done->process = n;
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
    }
}



// Core Simulation Engine: orchestrates the events

void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    // Suppress warnings for currently unused parameters 
    // (void)state;
    // (void)algorithm;


    // for (int i = 0; i < state->num_processes; i++) {
    //     Process *p = &state->processes[i];
    //     printf("Process %s: arrival=%d, burst=%d\n", p->pid, p->arrival_time, p->burst_time);
    // }

    // initialize_events will be in utils.c
    Event *event_queue = initialize_events(state);

    while (event_queue != NULL) {
        Event *current = pop_event(&event_queue);
        state->current_time = current->time;

        switch (current->type) {
            case EVENT_ARRIVAL:
                handle_arrival(state, current->process, &event_queue);
                break;
            case EVENT_COMPLETION:
                handle_completion(state, current->process, &event_queue);
                break;
            // ... handle other events [cite: 299]
        }
        free(current);
        // free(state->ready_queue);
    }
    

    // calculate_metrics and print_results will be in metrics.c and gantt.c
    // calculate_metrics(state);
    // print_metrics(state);
}