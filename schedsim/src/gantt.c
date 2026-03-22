// src/gantt.c
#include <stdio.h>
#include <stdlib.h>
#include "gantt.h"
/*
TODO: handle preemptive cases
*/
static void sort_by_start_time(SchedulerState *state, int *order) {
    for (int i = 0; i < state->num_processes; i++) order[i] = i;

    for (int i = 0; i < state->num_processes - 1; i++) {
        for (int j = i + 1; j < state->num_processes; j++) {
            Process *a = &state->processes[order[i]];
            Process *b = &state->processes[order[j]];
            if (a->start_time > b->start_time) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }
}

void print_gantt_chart(SchedulerState *state) {
    if (!state || state->num_processes <= 0) return;

    int *order = malloc(sizeof(int) * state->num_processes);
    if (!order) return;

    sort_by_start_time(state, order);

    printf("\n=== Gantt Chart ===\n");
    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[order[i]];
        int dashes = p->burst_time / 16;
        if (dashes < 1) dashes = 1;

        printf("[%s", p->pid);
        for (int k = 0; k < dashes; k++) printf("-");
        printf("]");
    }
    printf("\n");

    printf("Time: ");
    printf("%d", state->processes[order[0]].start_time);
    for (int i = 0; i < state->num_processes; i++) {
        Process *p = &state->processes[order[i]];
        printf("%10d", p->finish_time);
    }
    printf("\n");

    free(order);
}
