#include <stdio.h>
#include "scheduler.h"

// Computes per-process scheduling metrics after simulation completes
void calculate_metrics(SchedulerState *state) {
    // Iterate through every process in the simulation
    for (int i = 0; i < state->num_processes; i++) {
        // Shortcut pointer for cleaner field access
        Process *p = &state->processes[i];
        
        // Formula: Turnaround Time = Finish Time - Arrival Time
        int turnaround_time = p->finish_time - p->arrival_time;
        
        // Formula: Waiting Time = Turnaround Time - Burst Time
        p->waiting_time = turnaround_time - p->burst_time;
        
        // Formula: Response Time = First Execution - Arrival Time
        // start_time must be set when the process first hits the CPU
        int response_time = p->start_time - p->arrival_time;
        // response_time is currently computed for clarity/reference
        (void)response_time;
    }
}

// Prints a per-process metrics table plus averages
void print_metrics(SchedulerState *state) {
    printf("\n=== Metrics ===\n");
    printf("PID\tArrival\tBurst\tFinish\tTurnaround\tWaiting\tResponse\n");
    
    // Running sums used to compute averages at the end
    double total_tt = 0, total_wt = 0, total_rt = 0;
    // Print each process row and accumulate totals
    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[i];
        // Turnaround and response are derived from recorded timestamps
        int tt = p->finish_time - p->arrival_time;
        int rt = p->start_time - p->arrival_time;
        
        // Row output for one process
        printf("%s\t%d\t%d\t%d\t%d\t\t%d\t%d\n", 
               p->pid, p->arrival_time, p->burst_time, p->finish_time, tt, p->waiting_time, rt);
        
        // Add to totals for average calculation
        total_tt += tt;
        total_wt += p->waiting_time;
        total_rt += rt;
    }

    // Divider and average row
    printf("-----------------------------------------------------------------\n");
    printf("Average\t\t\t\t%.1f\t\t%.1f\t%.1f\n", 
           total_tt / state->num_processes, 
           total_wt / state->num_processes, 
           total_rt / state->num_processes);
}
