#include <stdio.h>
#include <stdlib.h>
#include "gantt.h"

/*
 * NOTE: This is a simplified Gantt chart (Fixing Finding #15).
 * For non-preemptive algorithms (FCFS, SJF), it is 100% accurate.
 * For preemptive algorithms (STCF, RR), it currently shows the process 
 * from its first start to its final completion. 
 * * TODO: To perfectly handle preemption, the SchedulerState would need 
 * an array of "ExecutionSlices" to track every time a process was 
 * swapped in and out.
 */

/**
 * Renders a visual timeline of process execution to the console.
 */
void print_gantt_chart(SchedulerState *state) {
    // Safety guard: ensure there is history to print
    if (!state || state->history_count <= 0) {
        printf("\nGantt Chart: No execution history to display.\n");
        return;
    }

    printf("\n=== Gantt Chart (Execution Timeline) ===\n");
    printf("(Scale: each '-' is roughly 4ms of CPU time)\n\n");

    // Iterate through the recorded history slices chronologically
    for (int i = 0; i < state->history_count; i++) {
        ExecutionSlice *s = &state->history[i];
        
        // Print the PID and the specific time this slice started
        printf("[%3d] %-5s: ", s->start_time, s->pid);

        // Calculate dashes based only on THIS slice's duration
        int duration = s->end_time - s->start_time;
        int dashes = duration / 4;
        if (dashes < 1) dashes = 1; 

        for (int d = 0; d < dashes; d++) {
            printf("-");
        }

        // Print the time this specific slice ended
        printf(" [%3d]\n", s->end_time);
    }
    
    printf("\n");
}