#ifndef SCHEDULER_H
#define SCHEDULER_H

// Pull in Process struct used by all schedulers
#include "process.h"

#define MAX_LEVELS 3

typedef struct {
	int *queue;
	int head;
	int tail;
	int count;
	int capacity;
} PriorityQueue;

// Forward declaration so we can reference MLFQConfig before its full definition
typedef struct MLFQConfig {
	int num_levels;
	int *quantums;
	int *allotments;
	int boost_period;
} MLFQConfig;

typedef struct {
    char pid[16];
    int start_time;
    int end_time;
} ExecutionSlice;

// Shared simulation state used by every algorithm
typedef struct {
	Process *processes;     // Array of all processes
	int num_processes;      // Number of processes
	int current_time;       // Current simulation time
	/* additional fields for metrics, Gantt chart, etc. */
	/* Recall: CMSC 141 */

	int *ready_queue;      // stores process indices //TODO: change name to ready_indices_array para mas clear
    int ready_head;			// we'll implement ready queue as circular array for O(1) enqueue/dequeue
    int ready_tail;
    int ready_count;
    int ready_capacity;

    int running_index;     // -1 if CPU idle
    int completed_count;
	int quantum;

	int last_dispatch_time;
	
	ExecutionSlice *history;
    int history_count;
    int history_capacity;

	// MLFQ-specific fields
	int num_levels;
	int boost_period;
	int *quantums;
	int *allotments;
	PriorityQueue mlfq_queues[MAX_LEVELS];
} SchedulerState;

// Return 0 on success, -1 on error

int schedule_fcfs(SchedulerState *state);
int schedule_sjf(SchedulerState *state);
int schedule_stcf(SchedulerState *state);
int schedule_rr(SchedulerState *state);
int schedule_mlfq(SchedulerState *state, MLFQConfig *config);

// Identifies which high-level algorithm the simulator should run
typedef enum {
	SCHED_FCFS,
	SCHED_SJF,
	SCHED_STCF,
	SCHED_RR,
	SCHED_MLFQ
} SchedulingAlgorithm;

// Discrete event types used by the simulation engine
typedef enum {
	EVENT_ARRIVAL,
	EVENT_COMPLETION,
	EVENT_QUANTUM_EXPIRE,
	EVENT_PRIORITY_BOOST
} EventType;

// Linked-list node for the simulation event queue
typedef struct Event {
	int time;
	EventType type;
	Process *process;
	struct Event *next;
} Event;

// Event queue helpers (implemented in src/utils.c)
Event *initialize_events(SchedulerState *state);
Event *pop_event(Event **head);

// Top-level simulation entry point
void simulate_scheduler(SchedulerState *state, SchedulingAlgorithm algorithm);

#endif

