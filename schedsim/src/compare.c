#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scheduler.h"
#include "utils.h"

typedef struct {
    char name[32];
    double avg_tt;
    double avg_wt;
    double avg_rt;
    int context_switches;
} AlgoMetricRow;

static void init_comparison_state(SchedulerState *state, Process *loaded_processes, int num_processes, SchedulingAlgorithm algo, int rr_quantum) {
    memset(state, 0, sizeof(*state));

    state->num_processes = num_processes;
    state->processes = malloc(sizeof(Process) * num_processes);
    state->ready_capacity = num_processes + 1;
    state->ready_queue = malloc(sizeof(int) * state->ready_capacity);
    state->history = NULL;
    state->history_count = 0;
    state->history_capacity = 0;
    state->running_index = -1;
    state->completed_count = 0;
    state->context_switches = 0;
    state->last_dispatch_time = 0;
    state->quantum = (algo == SCHED_RR) ? rr_quantum : -1;

    for (int i = 0; i < num_processes; i++) {
        state->processes[i] = loaded_processes[i];
        state->processes[i].remaining_time = loaded_processes[i].burst_time;
        state->processes[i].start_time = -1;
        state->processes[i].finish_time = -1;
        state->processes[i].priority = 0;
        state->processes[i].time_in_queue = 0;
        state->processes[i].waiting_time = 0;
        state->processes[i].response_time = 0;
        state->processes[i].turnaround_time = 0;
    }

    if (algo == SCHED_MLFQ) {
        state->num_levels = 3;
        state->boost_period = 200;
        state->quantums = malloc(sizeof(int) * state->num_levels);
        state->allotments = malloc(sizeof(int) * state->num_levels);

        for (int i = 0; i < state->num_levels && i < MAX_LEVELS; i++) {
            state->mlfq_queues[i].capacity = num_processes + 1;
            state->mlfq_queues[i].queue = malloc(sizeof(int) * state->mlfq_queues[i].capacity);
            state->mlfq_queues[i].head = 0;
            state->mlfq_queues[i].tail = 0;
            state->mlfq_queues[i].count = 0;
        }

        state->quantums[0] = 10;
        state->allotments[0] = 10;
        state->quantums[1] = 30;
        state->allotments[1] = 30;
        state->quantums[2] = -1;
        state->allotments[2] = -1;
    }
}

static void destroy_comparison_state(SchedulerState *state, SchedulingAlgorithm algo) {
    if (!state) return;

    free(state->processes);
    free(state->ready_queue);
    free(state->history);

    if (algo == SCHED_MLFQ) {
        for (int i = 0; i < state->num_levels && i < MAX_LEVELS; i++) {
            free(state->mlfq_queues[i].queue);
        }
        free(state->quantums);
        free(state->allotments);
    }
}

static void suppress_stdout_begin(int *saved_stdout_fd, FILE **null_file) {
    fflush(stdout);
    *saved_stdout_fd = dup(fileno(stdout));
    *null_file = fopen("/dev/null", "w");
    if (*saved_stdout_fd != -1 && *null_file != NULL) {
        dup2(fileno(*null_file), fileno(stdout));
    }
}

static void suppress_stdout_end(int saved_stdout_fd, FILE *null_file) {
    fflush(stdout);
    if (saved_stdout_fd != -1) {
        dup2(saved_stdout_fd, fileno(stdout));
        close(saved_stdout_fd);
    }
    if (null_file != NULL) {
        fclose(null_file);
    }
}

void run_comparative_analysis(Process *loaded_processes, int num_processes, int rr_quantum) {
    printf("\n=== Algorithm Comparison ===\n\n");
    printf("Algorithm | Avg TT | Avg WT | Avg RT | Context Switches\n");
    printf("----------|--------|--------|--------|-----------------\n");

    SchedulingAlgorithm algos[] = {SCHED_FCFS, SCHED_SJF, SCHED_STCF, SCHED_RR, SCHED_MLFQ};
    const char *algo_names[] = {"FCFS", "SJF", "STCF", "RR", "MLFQ"};

    for (int a = 0; a < 5; a++) {
        SchedulingAlgorithm algo = algos[a];
        SchedulerState state;
        init_comparison_state(&state, loaded_processes, num_processes, algo, rr_quantum);

        char dynamic_name[32];
        if (algo == SCHED_RR) {
            snprintf(dynamic_name, sizeof(dynamic_name), "RR (q=%d)", rr_quantum);
        } else {
            snprintf(dynamic_name, sizeof(dynamic_name), "%s", algo_names[a]);
        }

        int saved_stdout_fd = -1;
        FILE *null_file = NULL;
        suppress_stdout_begin(&saved_stdout_fd, &null_file);
        simulate_scheduler(&state, algo);
        suppress_stdout_end(saved_stdout_fd, null_file);

        int total_tt = 0;
        int total_wt = 0;
        int total_rt = 0;
        for (int i = 0; i < num_processes; i++) {
            int tt = state.processes[i].finish_time - state.processes[i].arrival_time;
            int wt = tt - state.processes[i].burst_time;
            int rt = state.processes[i].start_time - state.processes[i].arrival_time;
            total_tt += tt;
            total_wt += wt;
            total_rt += rt;
        }

        AlgoMetricRow row;
        snprintf(row.name, sizeof(row.name), "%s", dynamic_name);
        row.avg_tt = (double)total_tt / num_processes;
        row.avg_wt = (double)total_wt / num_processes;
        row.avg_rt = (double)total_rt / num_processes;
        row.context_switches = state.context_switches;

        printf("%-10s | %6.1f | %6.1f | %6.1f | %d\n",
               row.name,
               row.avg_tt,
               row.avg_wt,
               row.avg_rt,
               row.context_switches);

        destroy_comparison_state(&state, algo);
    }

    printf("\n");
}
