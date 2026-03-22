#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "metrics.h"
#include "gantt.h"
#include "utils.h"

// Entry point
int main(int argc, char *argv[]) {

    // Holds clock, list of processes, and ready queue
    SchedulerState state;
    // Clean starting state (no process yet)
    state.current_time = 0;
    state.num_processes = 0;
    state.processes = NULL;

    SchedulingAlgorithm selected_algo = SCHED_FCFS; // Default
    char *input_file = NULL;

    // Parse command line args: look for algorithm and input workload file
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

    // Load process workload
    if (input_file) {
        // Read input file and turn it into array of process structs
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
    // Allocate memory for ready queue (indices of waiting processes)
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

    // Initialize time to mark as completion
    done->time = when;
    done->type = EVENT_COMPLETION;
    // Attach pointer to finishing process
    done->process = p;
    done->next = NULL;

    // Case 1: Insert at head
    // If list is empty or new event happens soonest -> event = first event
    if (!*event_queue || done->time < (*event_queue)->time) {
        done->next = *event_queue;
        *event_queue = done;
    // Case 2: Find the gap
    // If there are events already scheduled,
    // keep moving the list until we find an event that is scheduled after our new event (done)
    } else {
        // Start at beginning of list
        Event *cur = *event_queue;
        // Search for correct spot to squeeze new event (done)
        // Look at next appointment time if their time is earlier or equal to new event time, keep moving
        while (cur->next && cur->next->time <= done->time) cur = cur->next;
        // New event (done) is now in between the cur and cur_next
        done->next = cur->next;
        cur->next = done;
    }
    return 0;
}

// Engine of the simulator that changes state of CPU
static void try_dispatch(SchedulerState *state, Event **event_queue, SchedulingAlgorithm algorithm) {
    // If CPU is busy, dispatch immediately
    if (state->running_index != -1) return; 

    // Ask algorithm which process in ready queue should go next
    int next = select_next_process(state, algorithm);
    // If ready queue is empty (next = -1), CPU stays idle
    if (next == -1) return; // No process ready to run
    
    // Process has been chosen to run -> remove from ready queue
    if (remove_ready_by_process_idx(state, next) == -1) return;
    
    Process *p = &state->processes[next];
    // If first time in CPU, record current time to use later for RT
    if (p->start_time == -1) p->start_time = state->current_time;
    // Update CPU is busy with process x
    state->running_index = next;

    // Schedule completion event
    int completion_time = state->current_time + p->remaining_time;
    (void) push_completion_event(event_queue, completion_time, p); //TODO: Check return value in case of malloc failure.

    
}

static void handle_arrival(SchedulerState *state, Process *process, Event **event_queue, SchedulingAlgorithm algorithm) { // doesn't handle preemption yet
    // Convert ptr to memory address into index
    int idx = process_index_from_ptr(state, process);
    // Process moved from "waiting to arrive" list into ready queue
    enqueue_ready(state, idx); // TODO: Check  return value in case queue is full.
    // Check if CPU is idle. If free, pull new process from ready queue and start
    try_dispatch(state, event_queue, algorithm);
}

static void handle_completion(SchedulerState *state, Process *process, Event **event_queue, SchedulingAlgorithm algorithm) {
    int idx = process_index_from_ptr(state, process);
    Process *p = &state->processes[idx];

    p->remaining_time = 0;
    // Record exact moment finished to calculate TT (finish - arrival) later
    p->finish_time = state->current_time;
    // To know when every process in workload is done
    state->completed_count++;
    // Free CPU (idle) 
    state->running_index = -1;

    // Check  ready queue to see who waited the longest/shortest and give them the CPU next
    try_dispatch(state, event_queue, algorithm);
}



// Core Simulation Engine: orchestrates the events

void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    // Create initial arrival events for each process and put into timeline
    Event *event_queue = initialize_events(state);
    // Run simulator while events in queue
    while (event_queue != NULL) {
        // Get soonest event
        Event *current = pop_event(&event_queue);
        // Jump clock directly to the time of next important event (popped event)
        state->current_time = current->time;

        switch (current->type) {
            // New process entered the system -> add to ready queue
            case EVENT_ARRIVAL:
                // Calculate when it will finish and add new EVENT_COMPLETION to queue
                handle_arrival(state, current->process, &event_queue, algorithm);
                break;
            // Process finished work -> CPU free
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