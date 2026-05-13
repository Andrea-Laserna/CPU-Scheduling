#include <stdio.h>
#include "scheduler.h"

// Performs all math and stores results in the process structs
void calculate_metrics(SchedulerState *state) {
    if (state->num_processes <= 0) return;

    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[i];
        
        // 1. Calculate and STORE Turnaround Time
        p->turnaround_time = p->finish_time - p->arrival_time;
        
        // 2. Calculate and STORE Waiting Time
        // Formula: Turnaround Time - Burst Time
        p->waiting_time = p->turnaround_time - p->burst_time;
        
        // 3. Calculate and STORE Response Time
        // Handle edge case where process never ran (start_time remains -1)
        if (p->start_time != -1) {
            p->response_time = p->start_time - p->arrival_time;
        } else {
            p->response_time = 0; 
        }
    }
}

// Prints a per-process metrics table plus averages
void print_metrics(SchedulerState *state) {
    if (!state || state->num_processes == 0) return;

    int total_tt = 0;
    int total_wt = 0;
    int total_rt = 0;

    printf("\n=== Detailed Metrics ===\n");
    
    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[i];

        // Calculate the metrics based on the recorded timestamps
        int tt = p->finish_time - p->arrival_time;
        int wt = tt - p->burst_time;
        int rt = p->start_time - p->arrival_time;

        // Accumulate totals for the averages
        total_tt += tt;
        total_wt += wt;
        total_rt += rt;

        // Print per-process detailed formulas matching Listing 8
        printf("Process %s:\n", p->pid);
        printf("  Arrival Time:    %d\n", p->arrival_time);
        printf("  Burst Time:      %d\n", p->burst_time);
        printf("  Finish Time:     %d\n", p->finish_time);
        printf("  Turnaround Time: %d - %d = %d\n", p->finish_time, p->arrival_time, tt);
        printf("  Waiting Time:    %d - %d = %d\n", tt, p->burst_time, wt);
        printf("  Response Time:   %d - %d = %d\n\n", p->start_time, p->arrival_time, rt);
    }

    // Print the calculated averages at the bottom
    printf("=== Average Metrics ===\n");
    printf("Average Turnaround Time : %.2f\n", (double)total_tt / state->num_processes);
    printf("Average Waiting Time    : %.2f\n", (double)total_wt / state->num_processes);
    printf("Average Response Time   : %.2f\n", (double)total_rt / state->num_processes);
}