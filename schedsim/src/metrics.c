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
    // Safety guard against empty workloads
    if (state->num_processes <= 0) {
        printf("No processes were simulated. No metrics to display.\n");
        return; 
    }

    printf("\n=== Metrics ===\n");
    printf("PID\tArrival\tBurst\tFinish\tTurnaround\tWaiting\tResponse\n");
    
    double total_tt = 0, total_wt = 0, total_rt = 0;

    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[i];
        
        // FIX: Use the PRE-CALCULATED values from the struct 
        // instead of re-calculating them inline.
        printf("%s\t%d\t%d\t%d\t%d\t\t%d\t%d\n", 
               p->pid, 
               p->arrival_time, 
               p->burst_time, 
               p->finish_time, 
               p->turnaround_time, 
               p->waiting_time, 
               p->response_time);
        
        total_tt += p->turnaround_time;
        total_wt += p->waiting_time;
        total_rt += p->response_time;
    }

    printf("-----------------------------------------------------------------\n");
    // Already safe from div-by-zero due to the guard at the top
    printf("Average\t\t\t\t%.1f\t\t%.1f\t%.1f\n", 
           total_tt / state->num_processes, 
           total_wt / state->num_processes, 
           total_rt / state->num_processes);
}