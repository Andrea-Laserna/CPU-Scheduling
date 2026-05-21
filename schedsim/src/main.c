#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "metrics.h"
#include "gantt.h"
#include "utils.h"

static Process *load_processes_inline(const char *arg, int *num_processes);
static int parse_mlfq_config(const char *path, MLFQConfig *config);

/**
 * Helper to clear any leftover events in the linked list.
 */
void clear_event_queue(Event **head) {
    while (*head != NULL) {
        Event *temp = *head;
        *head = (*head)->next;
        free(temp);
    }
}

/**
 * Maps a Process pointer to its index in the processes array.
 */
static int process_index_from_ptr(SchedulerState *state, Process *process) {
    if (!state || !state->processes || !process) return -1;
    for (int i = 0; i < state->num_processes; i++) {
        if (&state->processes[i] == process) return i;
    }
    return -1;
}

/**
 * Handles process completion.
 */
static void handle_completion(SchedulerState *state, Process *process) {
    if (!state || !process || state->running_index == -1) return;

    int idx = process_index_from_ptr(state, process);
    if (state->running_index != idx) return; // Guard against stale events

    // Add this guard right here:
    if (process->remaining_time <= 0) return;

    // Record the final slice for the Gantt chart
    log_execution(state, process->pid, state->last_dispatch_time, state->current_time);

    process->remaining_time = 0;
    process->finish_time = state->current_time;
    state->completed_count++;
    state->running_index = -1;
}

/**
 * Handles a new process arrival.
 */
static int handle_arrival(SchedulerState *state, Process *process, SchedulingAlgorithm algorithm) {
    int idx = process_index_from_ptr(state, process);
    if (idx == -1) return -1;
    
    if (algorithm == SCHED_MLFQ) {
        process->priority = 0;
        process->time_in_queue = 0;
        if (enqueue_priority(state, 0, idx) == -1) {
            fprintf(stderr, "Fatal Error: Priority queue overflow.\n");
            return -1;
        }
    } else if (enqueue_ready(state, idx) == -1) {
        fprintf(stderr, "Fatal Error: Ready queue overflow.\n");
        return -1; 
    }
    return 0;
}

/**
 * STCF Preemption logic.
 */
static void maybe_preempt_stcf(SchedulerState *state, Event **event_queue) {
    if (!state || state->running_index == -1) return;

    int candidate_idx = schedule_stcf(state);
    if (candidate_idx == -1) return;

    Process *running = &state->processes[state->running_index];
    Process *candidate = &state->processes[candidate_idx];

    // Preemption Condition
    if (candidate->remaining_time < running->remaining_time) {
        
        // Log what it did so far
        log_execution(state, running->pid, state->last_dispatch_time, state->current_time);
        
        // Cancel the old completion event
        // This stops the process from "finishing" in the future
        cancel_event(event_queue, running, EVENT_COMPLETION);

        // Update remaining time and put back in ready queue
        int elapsed = state->current_time - state->last_dispatch_time;
        running->remaining_time -= (elapsed > 0) ? elapsed : 0;
        
        enqueue_ready(state, state->running_index);
        state->context_switches++;
        state->running_index = -1;
    }
}

/**
 * Round Robin Preemption logic.
 * Fixed: Re-queues the process BEFORE new arrivals are handled in the main loop.
 */
static void maybe_preempt_rr(SchedulerState *state, Event **event_queue) {
    if (!state || state->running_index == -1) return;

    int elapsed = state->current_time - state->last_dispatch_time;

    // Check if the process has reached or exceeded its quantum
    if (elapsed >= state->quantum) {
        Process *running = &state->processes[state->running_index];
        
        // Remove the future completion event since we are preempting now
        cancel_event(event_queue, running, EVENT_COMPLETION);

        // Record the execution slice for the Gantt Chart
        log_execution(state, running->pid, state->last_dispatch_time, state->current_time);

        running->remaining_time -= elapsed;
        if (running->remaining_time < 0) running->remaining_time = 0;

        if (running->remaining_time > 0) {
            if (event_queue && *event_queue && (*event_queue)->time == state->current_time && (*event_queue)->type == EVENT_ARRIVAL) {
                enqueue_ready_front(state, state->running_index);
            } else {
                enqueue_ready(state, state->running_index);
            }
            state->context_switches++;
        } else {
            running->finish_time = state->current_time;
            state->completed_count++;
        }

        state->running_index = -1;
    }
}

void maybe_preempt_mlfq(SchedulerState *state, Event **event_queue) {
    if (state->running_index == -1) return;

    // Find the highest-priority non-empty queue above the running process.
    int running_priority = state->processes[state->running_index].priority;
    int best_ready_priority = -1;
    for (int level = 0; level < running_priority; level++) {
        if (state->mlfq_queues[level].count > 0) {
            best_ready_priority = level;
            break;
        }
    }

    // ONLY preempt if a waiting process has a HIGHER priority (lower number)
    // than the one currently on the CPU.
    Process *running = &state->processes[state->running_index];
    if (best_ready_priority != -1) {
        // Preempt the current process
        log_execution(state, running->pid, state->last_dispatch_time, state->current_time);
        
        int elapsed = state->current_time - state->last_dispatch_time;
        running->remaining_time -= elapsed;
        running->time_in_queue += elapsed;

        // Cancel its original completion/expire event
        cancel_event(event_queue, running, EVENT_COMPLETION);
        cancel_event(event_queue, running, EVENT_QUANTUM_EXPIRE);

        // Put it back in the ready queue and clear CPU
        enqueue_priority(state, running->priority, state->running_index);
        state->context_switches++;
        state->running_index = -1;
    }
}

/**
 * Core Simulation Engine.
 */
void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm) {
    // 1. Initialize arrivals from the loaded process list
    Event *event_queue = initialize_events(state);
    const int mlfq_boost_interval = state->boost_period;
    
    // 2. For MLFQ, schedule the very first Priority Boost event
    if (algorithm == SCHED_MLFQ) {
        Event *boost = malloc(sizeof(Event));
        boost->time = mlfq_boost_interval;
        boost->type = EVENT_PRIORITY_BOOST;
        boost->process = NULL;
        boost->next = NULL;
        push_event_sorted(&event_queue, boost);
    }

    while (event_queue != NULL && state->completed_count < state->num_processes) {
        // Get the next earliest event
        Event *current = pop_event(&event_queue);
        state->current_time = current->time;

        int status = 0;
        switch (current->type) {
            case EVENT_ARRIVAL:
                // Put arriving process in ready queue
                status = handle_arrival(state, current->process, algorithm);
                
                // Check if the arrival should preempt the current running process
                if (algorithm == SCHED_STCF) {
                    maybe_preempt_stcf(state, &event_queue);
                } else if (algorithm == SCHED_MLFQ) {
                    maybe_preempt_mlfq(state, &event_queue);
                }
                break;

            case EVENT_COMPLETION:
                handle_completion(state, current->process);
                break;

            case EVENT_QUANTUM_EXPIRE:
                if (algorithm == SCHED_RR) {
                    maybe_preempt_rr(state, &event_queue);
                } else if (algorithm == SCHED_MLFQ && state->running_index != -1) {
                    Process *p = &state->processes[state->running_index];
                    
                    // Log the execution slice
                    log_execution(state, p->pid, state->last_dispatch_time, state->current_time);

                    int elapsed = state->current_time - state->last_dispatch_time;
                    p->remaining_time -= elapsed;
                    p->time_in_queue += elapsed; // Accumulate time spent at current level

                    // Check for Demotion: Have we consumed the allotment for this level?
                    if (p->time_in_queue >= state->allotments[p->priority]) {
                        if (p->priority < state->num_levels - 1) {
                            p->priority++;        // Demote to next lower queue
                        }
                        p->time_in_queue = 0;     // Reset counter for the new priority tier
                    }

                    // Re-queue if not done
                    if (p->remaining_time > 0) {
                        enqueue_priority(state, p->priority, state->running_index);
                        state->context_switches++;
                    } else {
                        p->finish_time = state->current_time;
                        state->completed_count++;
                    }
                    state->running_index = -1;
                }
                break;
                
            case EVENT_PRIORITY_BOOST: {
                int current_boost_time = current->time;

                if (state->completed_count >= state->num_processes) {
                    break;
                }

                // THE FIX: Do NOT preempt the running process!
                // The reference simulator leaves the active process on the CPU
                // during a boost so it can finish its current quantum slice.

                // Move all waiting processes from lower queues (1 and 2) up to Queue 0.
                // Queue 0 is left intact so existing processes keep their front-of-line spot.
                for (int level = 1; level < state->num_levels; level++) {
                    while (state->mlfq_queues[level].count > 0) {
                        int p_idx = dequeue_priority(state, level);
                        state->processes[p_idx].priority = 0;
                        state->processes[p_idx].time_in_queue = 0;
                        enqueue_priority(state, 0, p_idx);
                    }
                }
                
                // ANCHOR THE TIMING TO PREVENT INTERVAL SHIFTING
                Event *next_boost = malloc(sizeof(Event));
                next_boost->time = current_boost_time + mlfq_boost_interval;
                next_boost->type = EVENT_PRIORITY_BOOST;
                next_boost->process = NULL;
                next_boost->next = NULL;
                push_event_sorted(&event_queue, next_boost);
                break;
            }
            default: 
                break;
        }

        if (status == -1) {
            free(current);
            break; 
        }

        // 3. If CPU is idle, pick the next process to run
        try_dispatch(state, &event_queue, algorithm);
        
        free(current);
    }

    // 4. Wrap up and output results
    calculate_metrics(state);
    print_gantt_chart(state);
    print_metrics(state);
    
    // Final cleanup of any unused boost events
    clear_event_queue(&event_queue);
}

int main(int argc, char *argv[]) {
    SchedulerState state;
    state.current_time = 0;
    state.num_processes = 0;
    state.processes = NULL;
    state.quantum = 4; 
    state.context_switches = 0;

    int compare_mode = 0;
    char *input_file = NULL;
    char *inline_processes = NULL;
    char *mlfq_config_path = NULL;
    int rr_quantum = 10; // Default fallback
    
    // Initialize History for Gantt Chart
    state.history_count = 0;
    state.history_capacity = 100;
    state.history = malloc(sizeof(ExecutionSlice) * state.history_capacity);

    SchedulingAlgorithm selected_algo = SCHED_FCFS; 

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--compare") == 0) {
            compare_mode = 1;
        } else if (strncmp(argv[i], "--algorithm=", 12) == 0) {
            char *algo_name = argv[i] + 12;
            if (strcmp(algo_name, "FCFS") == 0) selected_algo = SCHED_FCFS;
            else if (strcmp(algo_name, "SJF") == 0) selected_algo = SCHED_SJF;
            else if (strcmp(algo_name, "STCF") == 0) selected_algo = SCHED_STCF;
            else if (strcmp(algo_name, "RR") == 0) selected_algo = SCHED_RR;
            else if (strcmp(algo_name, "MLFQ") == 0) selected_algo = SCHED_MLFQ;
        } else if (strncmp(argv[i], "--input=", 8) == 0) {
            input_file = argv[i] + 8;
        } else if (strncmp(argv[i], "--processes=", 12) == 0) {
            inline_processes = argv[i] + 12;
        } else if (strncmp(argv[i], "--mlfq-config=", 14) == 0) {
            mlfq_config_path = argv[i] + 14;
        } else if (strncmp(argv[i], "--quantum=", 10) == 0) {
            state.quantum = atoi(argv[i] + 10);
            rr_quantum = state.quantum;
        }
    }

    if (input_file && inline_processes) {
        fprintf(stderr, "Error: Use either --input= or --processes=, not both.\n");
        return -1;
    }

    if (!input_file && !inline_processes) return -1;

    if (inline_processes) {
        state.processes = load_processes_inline(inline_processes, &state.num_processes);
    } else {
        state.processes = load_processes(input_file, &state.num_processes);
    }
    if (!state.processes) return -1;

    if (mlfq_config_path && selected_algo != SCHED_MLFQ) {
        fprintf(stderr, "Warning: --mlfq-config ignored for non-MLFQ algorithms.\n");
    }

    if (compare_mode) {
        run_comparative_analysis(state.processes, state.num_processes, rr_quantum);
        free(state.processes);
        free(state.history);
        return 0;
    }

    state.ready_capacity = state.num_processes + 1;
    state.ready_queue = malloc(sizeof(int) * state.ready_capacity);
    
    state.ready_head = state.ready_tail = state.ready_count = 0;
    state.running_index = -1;
    state.completed_count = 0;
    state.context_switches = 0;
    state.last_dispatch_time = 0;

    // MLFQ-specific initialization
    if (selected_algo == SCHED_MLFQ) {
        MLFQConfig config;
        int has_config = 0;

        if (mlfq_config_path) {
            if (parse_mlfq_config(mlfq_config_path, &config) != 0) {
                fprintf(stderr, "Error: Failed to parse MLFQ config file.\n");
                return -1;
            }
            has_config = 1;
        }

        state.num_levels = has_config ? config.num_levels : 3;
        state.boost_period = has_config ? config.boost_period : 200; // Requirement: Period S = 200
        
        state.quantums = malloc(sizeof(int) * state.num_levels);
        state.allotments = malloc(sizeof(int) * state.num_levels);
        state.mlfq_queues = malloc(sizeof(PriorityQueue) * state.num_levels);
        if (!state.mlfq_queues) {
            free(state.quantums);
            free(state.allotments);
            return -1;
        }

        for (int i = 0; i < state.num_levels; i++) {
            state.mlfq_queues[i].capacity = state.num_processes + 1;
            state.mlfq_queues[i].queue = malloc(sizeof(int) * state.mlfq_queues[i].capacity);
            state.mlfq_queues[i].head = 0;
            state.mlfq_queues[i].tail = 0;
            state.mlfq_queues[i].count = 0;
        }

        if (has_config) {
            for (int i = 0; i < state.num_levels; i++) {
                state.quantums[i] = config.quantums[i];
                state.allotments[i] = config.allotments[i];
            }
            free(config.quantums);
            free(config.allotments);
        } else {
            // Level 0: Quantum 10, Allotment 10 (Matches: [A][B][C][D][E] then demote)
            state.quantums[0] = 10;
            state.allotments[0] = 10; 

            // Level 1: Quantum 30, Allotment 30
            state.quantums[1] = 30;
            state.allotments[1] = 30;

            // Level 2: FCFS
            state.quantums[2] = -1;
            state.allotments[2] = -1;
        }

        for (int i = 0; i < state.num_processes; i++) {
            state.processes[i].priority = 0;
            state.processes[i].time_in_queue = 0;
        }
    }

    simulate_scheduler(&state, selected_algo);

    // Cleanup
    free(state.processes);
    free(state.ready_queue);
    free(state.history);
    if (selected_algo == SCHED_MLFQ) {
        for (int i = 0; i < state.num_levels; i++) {
            free(state.mlfq_queues[i].queue);
        }
        free(state.quantums);
        free(state.allotments);
        free(state.mlfq_queues);
    }
    
    return 0;
}

static Process *load_processes_inline(const char *arg, int *num_processes) {
    if (!arg || !num_processes) return NULL;

    int capacity = 10;
    int count = 0;
    Process *processes = malloc(sizeof(Process) * capacity);
    if (!processes) return NULL;

    char *input = strdup(arg);
    if (!input) {
        free(processes);
        return NULL;
    }

    char *saveptr = NULL;
    char *token = strtok_r(input, ",", &saveptr);
    while (token) {
        char pid[16];
        int arrival = 0;
        int burst = 0;

        if (sscanf(token, "%15[^:]:%d:%d", pid, &arrival, &burst) != 3) {
            fprintf(stderr, "Error: Invalid --processes token '%s'\n", token);
            free(input);
            free(processes);
            return NULL;
        }

        if (arrival < 0 || burst <= 0) {
            fprintf(stderr, "Warning: Skipping invalid process %s (Arrival: %d, Burst: %d)\n",
                    pid, arrival, burst);
            token = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        if (count >= capacity) {
            int new_capacity = capacity * 2;
            Process *temp = realloc(processes, sizeof(Process) * new_capacity);
            if (!temp) {
                free(input);
                free(processes);
                return NULL;
            }
            processes = temp;
            capacity = new_capacity;
        }

        Process *p = &processes[count++];
        strncpy(p->pid, pid, sizeof(p->pid) - 1);
        p->pid[sizeof(p->pid) - 1] = '\0';
        p->arrival_time = arrival;
        p->burst_time = burst;
        p->remaining_time = burst;
        p->start_time = -1;
        p->finish_time = 0;
        p->waiting_time = 0;
        p->priority = 0;
        p->time_in_queue = 0;

        token = strtok_r(NULL, ",", &saveptr);
    }

    free(input);
    *num_processes = count;
    return processes;
}

static char *trim_whitespace(char *text) {
    if (!text) return NULL;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }

    if (*text == '\0') return text;

    char *end = text + strlen(text) - 1;
    while (end > text && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    end[1] = '\0';
    return text;
}

static int parse_int_value(const char *text, int *out) {
    if (!text || !out) return -1;

    char *endptr = NULL;
    long value = strtol(text, &endptr, 10);
    if (endptr == text || *trim_whitespace(endptr) != '\0') return -1;

    *out = (int)value;
    return 0;
}

static int parse_int_list(const char *text, int count, int **out) {
    if (!text || count <= 0 || !out) return -1;

    int *values = malloc(sizeof(int) * count);
    if (!values) return -1;

    char *input = strdup(text);
    if (!input) {
        free(values);
        return -1;
    }

    int idx = 0;
    char *saveptr = NULL;
    char *token = strtok_r(input, ",", &saveptr);
    while (token && idx < count) {
        char *trimmed = trim_whitespace(token);
        if (parse_int_value(trimmed, &values[idx]) != 0) {
            free(values);
            free(input);
            return -1;
        }
        idx++;
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(input);
    if (idx != count || token != NULL) {
        free(values);
        return -1;
    }

    *out = values;
    return 0;
}

static int parse_mlfq_config(const char *path, MLFQConfig *config) {
    if (!path || !config) return -1;

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Failed to open MLFQ config");
        return -1;
    }

    int num_levels = -1;
    int boost_period = -1;
    char *quantums_text = NULL;
    char *allotments_text = NULL;
    int positional_index = 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        char *equals = strchr(trimmed, '=');
        if (equals) {
            *equals = '\0';
            char *key = trim_whitespace(trimmed);
            char *value = trim_whitespace(equals + 1);

            if (strcmp(key, "levels") == 0 || strcmp(key, "num_levels") == 0 || strcmp(key, "queues") == 0) {
                if (parse_int_value(value, &num_levels) != 0) {
                    fclose(file);
                    return -1;
                }
            } else if (strcmp(key, "boost") == 0 || strcmp(key, "boost_period") == 0) {
                if (parse_int_value(value, &boost_period) != 0) {
                    fclose(file);
                    return -1;
                }
            } else if (strcmp(key, "quantums") == 0 || strcmp(key, "quanta") == 0) {
                free(quantums_text);
                quantums_text = strdup(value);
            } else if (strcmp(key, "allotments") == 0 || strcmp(key, "allotment") == 0) {
                free(allotments_text);
                allotments_text = strdup(value);
            }
        } else {
            if (positional_index == 0) {
                if (parse_int_value(trimmed, &num_levels) != 0) {
                    fclose(file);
                    return -1;
                }
            } else if (positional_index == 1) {
                free(quantums_text);
                quantums_text = strdup(trimmed);
            } else if (positional_index == 2) {
                free(allotments_text);
                allotments_text = strdup(trimmed);
            } else if (positional_index == 3) {
                if (parse_int_value(trimmed, &boost_period) != 0) {
                    fclose(file);
                    return -1;
                }
            }
            positional_index++;
        }
    }

    fclose(file);

    if (num_levels <= 0) {
        free(quantums_text);
        free(allotments_text);
        return -1;
    }

    int *quantums = NULL;
    int *allotments = NULL;
    if (!quantums_text || !allotments_text) {
        free(quantums_text);
        free(allotments_text);
        return -1;
    }

    if (parse_int_list(quantums_text, num_levels, &quantums) != 0 ||
        parse_int_list(allotments_text, num_levels, &allotments) != 0) {
        free(quantums_text);
        free(allotments_text);
        free(quantums);
        free(allotments);
        return -1;
    }

    free(quantums_text);
    free(allotments_text);

    config->num_levels = num_levels;
    config->boost_period = (boost_period > 0) ? boost_period : 200;
    config->quantums = quantums;
    config->allotments = allotments;
    return 0;
}