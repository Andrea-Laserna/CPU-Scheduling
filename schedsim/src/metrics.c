#include <stdio.h>
#include "scheduler.h"

void calculate_metrics(SchedulerState *state) {

    for (int i = 0; i < state->num_processes; i++) {
        
        Process *p = &state->processes[i];
        int turnaround_time = p->finish_time - p->arrival_time;
        p->waiting_time = turnaround_time - p->burst_time;
        int response_time = p->start_time - p->arrival_time;
        
        (void)response_time;
    }
}

// Prints a per-process metrics table plus averages
void print_metrics(SchedulerState *state) {
    /*
    issue to fix: divide by zero
    */
    printf("\n=== Metrics ===\n");
    printf("PID\tArrival\tBurst\tFinish\tTurnaround\tWaiting\tResponse\n");
    

    double total_tt = 0, total_wt = 0, total_rt = 0;

    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[i];
        int tt = p->finish_time - p->arrival_time;
        int rt = p->start_time - p->arrival_time;
        
        // Row output for one process
        printf("%s\t%d\t%d\t%d\t%d\t\t%d\t%d\n", 
               p->pid, p->arrival_time, p->burst_time, p->finish_time, tt, p->waiting_time, rt);
        
        total_tt += tt;
        total_wt += p->waiting_time;
        total_rt += rt;
    }

    printf("-----------------------------------------------------------------\n");
    printf("Average\t\t\t\t%.1f\t\t%.1f\t%.1f\n", 
           total_tt / state->num_processes, 
           total_wt / state->num_processes, 
           total_rt / state->num_processes);
}
