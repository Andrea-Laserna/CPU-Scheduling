#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"

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
        // state.processes = load_processes(input_file, &state.num_processes);
    } else {
        // Input file is required to run a simulation
        fprintf(stderr, "Error: No input file specified.\n");
        return 1;
    }

    // Run the Discrete-Event Simulator
    // Confirms that argument parsing completed and run is about to start
    printf("Starting Simulation...\n");
    // Dispatch into the simulation engine
    simulate_scheduler(&state, selected_algo);

    // Cleanup
    // Safe even when state.processes is NULL
    free(state.processes);
    
    return 0;
}

// Core Simulation Engine: orchestrates the events

void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    // Suppress warnings for currently unused parameters 
    (void)state;
    (void)algorithm;

    // initialize_events will be in utils.c
    // Event *event_queue = initialize_events(state);

    /* while (event_queue != NULL) {
        Event *current = pop_event(&event_queue);
        state->current_time = current->time;

        switch (current->type) {
            case EVENT_ARRIVAL:
                // handle_arrival(state, current->process);
                break;
            case EVENT_COMPLETION:
                // handle_completion(state, current->process);
                break;
            // ... handle other events [cite: 299]
        }
        free(current);
    }
    */

    // calculate_metrics and print_results will be in metrics.c and gantt.c
    // calculate_metrics(state);
    // print_results(state);
}