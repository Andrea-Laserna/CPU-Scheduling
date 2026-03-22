#ifndef SCHEDULER_H
#define SCHEDULER_H

// Pull in Process struct used by all schedulers
#include "process.h"

// One queue level in MLFQ
typedef struct {
	int level;              // Queue priority level (0 = highest)
	int time_quantum;       // Time slice for this queue (-1 for FCFS)
	int allotment;          // Max time before demotion (-1 for infinite)
	Process *queue;         // Array or linked list of processes
	int size;               // Current queue size
} MLFQQueue;

// Global container for all MLFQ queue levels and boost settings
typedef struct {
	MLFQQueue *queues;      // Array of queues
	int num_queues;         // Number of priority levels
	int boost_period;       // Period for priority boost (S)
	int last_boost;         // Last boost time
} MLFQScheduler;

// Forward declaration so we can reference MLFQConfig before its full definition
typedef struct MLFQConfig MLFQConfig;

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

	int last_dispatch_time;
} SchedulerState;

// Return 0 on success, -1 on error

int schedule_fcfs(SchedulerState *state);
int schedule_sjf(SchedulerState *state);
int schedule_stcf(SchedulerState *state);
int schedule_rr(SchedulerState *state, int quantum);
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

