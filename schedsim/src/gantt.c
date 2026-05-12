#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gantt.h"

void print_gantt_chart(SchedulerState *state) {
    if (!state || state->history_count <= 0) {
        printf("\n=== Gantt Chart ===\nNo history to display.\n");
        return;
    }

    const int SCALE = 10; 
    printf("\n=== Gantt Chart ===\n(scaled: each char = %d time units)\n", SCALE);

    // Dynamically allocate positions to match history_count + 1
    int *positions = malloc(sizeof(int) * (state->history_count + 1));
    if (!positions) return; 

    int current_offset = 0;

    // Row 1: Blocks
    for (int i = 0; i < state->history_count; i++) {
        ExecutionSlice *s = &state->history[i];
        int duration = s->end_time - s->start_time;
        int width = duration / SCALE;
        
        // Ensure width is at least as long as the PID string
        int pid_len = (int)strlen(s->pid);
        if (width < pid_len) width = pid_len;

        positions[i] = current_offset;
        
        printf("[%s", s->pid);
        // Fill the rest of the block width with dashes
        for (int j = 0; j < (width - pid_len); j++) {
            printf("-");
        }
        printf("]");
        
        current_offset += (width + 2); // +2 for the brackets '[' and ']'
    }
    
    // Store final end position for the last timestamp
    positions[state->history_count] = current_offset;
    printf("\n");

    // Row 2: Time Markers
    int printed_until = 0;
    for (int i = 0; i <= state->history_count; i++) {
        // Space out to the start of the current block
        while (printed_until < positions[i]) {
            printf(" ");
            printed_until++;
        }
        
        int time = (i < state->history_count) ? 
                    state->history[i].start_time : 
                    state->history[i-1].end_time;
                    
        int len = printf("%d", time);
        printed_until += len;
    }
    printf("\n\n");

    free(positions);
}