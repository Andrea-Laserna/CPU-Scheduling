
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "metrics.h"
#include "gantt.h"
#include "utils.h"

typedef struct {
	int *items;
	int head;
	int tail;
	int count;
	int capacity;
} Queue;

typedef struct {
	int num_levels;
	int *quantums;
	int *allotments;
	int boost_period;
	Queue *queues;
	int next_boost_time;
} MLFQRuntime;

static void free_event_queue(Event **head) {
	while (head && *head) {
		Event *temp = *head;
		*head = (*head)->next;
		free(temp);
	}
}

static int process_index_from_ptr(SchedulerState *state, Process *process) {
	if (!state || !state->processes || !process) return -1;
	for (int i = 0; i < state->num_processes; i++) {
		if (&state->processes[i] == process) return i;
	}
	return -1;
}

static void push_event_sorted(Event **head, Event *new_event, int insert_before_equal) {
	if (!head || !new_event) return;

	if (!*head || new_event->time < (*head)->time) {
		new_event->next = *head;
		*head = new_event;
		return;
	}

	Event *curr = *head;
	if (insert_before_equal) {
		while (curr->next && curr->next->time < new_event->time) {
			curr = curr->next;
		}
	} else {
		while (curr->next && curr->next->time <= new_event->time) {
			curr = curr->next;
		}
	}

	new_event->next = curr->next;
	curr->next = new_event;
}

static int queue_init(Queue *queue, int capacity) {
	if (!queue || capacity <= 0) return -1;
	queue->items = malloc(sizeof(int) * capacity);
	if (!queue->items) return -1;
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	queue->capacity = capacity;
	return 0;
}

static void queue_destroy(Queue *queue) {
	if (!queue) return;
	free(queue->items);
	queue->items = NULL;
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	queue->capacity = 0;
}

static int queue_push_back(Queue *queue, int idx) {
	if (!queue || !queue->items || queue->count == queue->capacity) return -1;
	queue->items[queue->tail] = idx;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->count++;
	return 0;
}

static int queue_push_front(Queue *queue, int idx) {
	if (!queue || !queue->items || queue->count == queue->capacity) return -1;
	queue->head = (queue->head - 1 + queue->capacity) % queue->capacity;
	queue->items[queue->head] = idx;
	queue->count++;
	return 0;
}

static int queue_pop_front(Queue *queue) {
	if (!queue || !queue->items || queue->count == 0) return -1;
	int idx = queue->items[queue->head];
	queue->head = (queue->head + 1) % queue->capacity;
	queue->count--;
	return idx;
}

static int queue_is_empty(const Queue *queue) {
	return !queue || queue->count == 0;
}

static void runtime_destroy(MLFQRuntime *runtime) {
	if (!runtime) return;
	if (runtime->queues) {
		for (int i = 0; i < runtime->num_levels; i++) {
			queue_destroy(&runtime->queues[i]);
		}
		free(runtime->queues);
	}
	runtime->queues = NULL;
	runtime->num_levels = 0;
	runtime->quantums = NULL;
	runtime->allotments = NULL;
	runtime->boost_period = 0;
	runtime->next_boost_time = -1;
}

static int runtime_init(MLFQRuntime *runtime, SchedulerState *state, MLFQConfig *config) {
	static int default_quantums[] = {8, 16, -1};
	static int default_allotments[] = {16, 32, -1};
	MLFQConfig default_config = {
		.num_levels = 3,
		.quantums = default_quantums,
		.allotments = default_allotments,
		.boost_period = 0,
	};

	if (!runtime || !state || state->num_processes < 0) return -1;
	if (!config) config = &default_config;
	if (config->num_levels <= 0) return -1;

	runtime->num_levels = config->num_levels;
	runtime->quantums = config->quantums ? config->quantums : default_quantums;
	runtime->allotments = config->allotments ? config->allotments : default_allotments;
	runtime->boost_period = config->boost_period;
	runtime->next_boost_time = (runtime->boost_period > 0) ? runtime->boost_period : -1;
	runtime->queues = calloc((size_t)runtime->num_levels, sizeof(Queue));
	if (!runtime->queues) return -1;

	for (int i = 0; i < runtime->num_levels; i++) {
		if (queue_init(&runtime->queues[i], state->num_processes + 1) != 0) {
			runtime_destroy(runtime);
			return -1;
		}
	}

	return 0;
}

static void schedule_next_boost(Event **event_queue, MLFQRuntime *runtime) {
	if (!event_queue || !runtime || runtime->boost_period <= 0 || runtime->next_boost_time < 0) return;

	Event *event = malloc(sizeof(Event));
	if (!event) return;

	event->time = runtime->next_boost_time;
	event->type = EVENT_PRIORITY_BOOST;
	event->process = NULL;
	event->next = NULL;
	push_event_sorted(event_queue, event, 1);
	runtime->next_boost_time += runtime->boost_period;
}

static int highest_nonempty_queue(MLFQRuntime *runtime) {
	if (!runtime) return -1;
	for (int i = 0; i < runtime->num_levels; i++) {
		if (!queue_is_empty(&runtime->queues[i])) return i;
	}
	return -1;
}

static int should_preempt_for_arrival(SchedulerState *state, MLFQRuntime *runtime) {
	if (!state || !runtime || state->running_index < 0) return 0;
	int running_level = state->processes[state->running_index].priority;
	return running_level > 0;
}

static void dispatch_if_idle(SchedulerState *state, MLFQRuntime *runtime, Event **event_queue) {
	if (!state || !runtime || !event_queue || state->running_index != -1) return;

	int level = highest_nonempty_queue(runtime);
	if (level == -1) return;

	int idx = queue_pop_front(&runtime->queues[level]);
	if (idx < 0) return;

	Process *p = &state->processes[idx];
	state->running_index = idx;
	state->last_dispatch_time = state->current_time;
	p->priority = level;
	if (p->start_time == -1) p->start_time = state->current_time;

	int run_for = p->remaining_time;
	int quantum = runtime->quantums[level];
	int allotment = runtime->allotments[level];
	int allotment_remaining = (allotment > 0) ? (allotment - p->time_in_queue) : -1;

	if (quantum > 0 && run_for > quantum) run_for = quantum;
	if (allotment_remaining > 0 && run_for > allotment_remaining) run_for = allotment_remaining;

	if (run_for <= 0) run_for = p->remaining_time;

	Event *event = malloc(sizeof(Event));
	if (!event) {
		state->running_index = -1;
		return;
	}

	event->time = state->current_time + run_for;
	event->type = (run_for >= p->remaining_time) ? EVENT_COMPLETION : EVENT_QUANTUM_EXPIRE;
	event->process = p;
	event->next = NULL;
	push_event_sorted(event_queue, event, event->type == EVENT_QUANTUM_EXPIRE);
}

static void preempt_running_process(SchedulerState *state, MLFQRuntime *runtime, Event **event_queue) {
	if (!state || !runtime || !event_queue || state->running_index < 0) return;

	Process *running = &state->processes[state->running_index];
	int elapsed = state->current_time - state->last_dispatch_time;
	if (elapsed < 0) elapsed = 0;

	cancel_event(event_queue, running, EVENT_COMPLETION);
	cancel_event(event_queue, running, EVENT_QUANTUM_EXPIRE);

	running->remaining_time -= elapsed;
	if (running->remaining_time < 0) running->remaining_time = 0;
	running->time_in_queue += elapsed;
	log_execution(state, running->pid, state->last_dispatch_time, state->current_time);

	if (running->remaining_time > 0) {
		int level = running->priority;
		if (level < 0) level = 0;
		if (level < runtime->num_levels - 1) {
			int allotment = runtime->allotments[level];
			if (allotment > 0 && running->time_in_queue >= allotment) {
				running->priority = level + 1;
				running->time_in_queue = 0;
				queue_push_back(&runtime->queues[running->priority], state->running_index);
			} else {
				queue_push_front(&runtime->queues[level], state->running_index);
			}
		} else {
			queue_push_front(&runtime->queues[level], state->running_index);
		}
	} else {
		running->finish_time = state->current_time;
		state->completed_count++;
	}

	state->running_index = -1;
}

static void handle_arrival(SchedulerState *state, MLFQRuntime *runtime, Event **event_queue, Process *process) {
	if (!state || !runtime || !process) return;

	int idx = process_index_from_ptr(state, process);
	if (idx < 0) return;

	process->priority = 0;
	process->time_in_queue = 0;
	queue_push_back(&runtime->queues[0], idx);

	if (should_preempt_for_arrival(state, runtime)) {
		preempt_running_process(state, runtime, event_queue);
	}
}

static void handle_completion(SchedulerState *state, MLFQRuntime *runtime, Process *process) {
	if (!state || !runtime || !process || state->running_index < 0) return;

	int idx = process_index_from_ptr(state, process);
	if (idx != state->running_index) return;

	int elapsed = state->current_time - state->last_dispatch_time;
	if (elapsed < 0) elapsed = 0;
	process->remaining_time -= elapsed;
	if (process->remaining_time < 0) process->remaining_time = 0;
	process->time_in_queue += elapsed;
	log_execution(state, process->pid, state->last_dispatch_time, state->current_time);
	process->finish_time = state->current_time;
	state->running_index = -1;
	state->completed_count++;
}

static void handle_quantum_expire(SchedulerState *state, MLFQRuntime *runtime, Event **event_queue) {
	if (!state || !runtime || !event_queue || state->running_index < 0) return;

	Process *running = &state->processes[state->running_index];
	int idx = state->running_index;
	int elapsed = state->current_time - state->last_dispatch_time;
	if (elapsed < 0) elapsed = 0;

	running->remaining_time -= elapsed;
	if (running->remaining_time < 0) running->remaining_time = 0;
	running->time_in_queue += elapsed;
	log_execution(state, running->pid, state->last_dispatch_time, state->current_time);

	int level = running->priority;
	if (level < 0) level = 0;
	if (running->remaining_time > 0) {
		int allotment = runtime->allotments[level];
		if (level < runtime->num_levels - 1 && allotment > 0 && running->time_in_queue >= allotment) {
			running->priority = level + 1;
			running->time_in_queue = 0;
			queue_push_back(&runtime->queues[running->priority], idx);
		} else if (event_queue && *event_queue && (*event_queue)->time == state->current_time && (*event_queue)->type == EVENT_ARRIVAL) {
			queue_push_front(&runtime->queues[level], idx);
		} else {
			queue_push_back(&runtime->queues[level], idx);
		}
	} else {
		running->finish_time = state->current_time;
		state->completed_count++;
	}

	state->running_index = -1;
}

static void handle_priority_boost(SchedulerState *state, MLFQRuntime *runtime, Event **event_queue) {
	if (!state || !runtime) return;

	if (state->running_index != -1) {
		preempt_running_process(state, runtime, event_queue);
	}

	for (int level = 1; level < runtime->num_levels; level++) {
		while (!queue_is_empty(&runtime->queues[level])) {
			int idx = queue_pop_front(&runtime->queues[level]);
			if (idx < 0) break;
			Process *p = &state->processes[idx];
			p->priority = 0;
			p->time_in_queue = 0;
			queue_push_back(&runtime->queues[0], idx);
		}
	}

	runtime->next_boost_time = (runtime->boost_period > 0) ? state->current_time + runtime->boost_period : -1;
	schedule_next_boost(event_queue, runtime);
}

static void initialize_history(SchedulerState *state) {
	if (!state) return;
	if (!state->history) {
		state->history_capacity = 16;
		state->history = malloc(sizeof(ExecutionSlice) * state->history_capacity);
	}
	state->history_count = 0;
}

int schedule_mlfq(SchedulerState *state, MLFQConfig *config) {
	if (!state || !state->processes || state->num_processes <= 0) return -1;

	initialize_history(state);
	if (!state->history) return -1;

	MLFQRuntime runtime;
	memset(&runtime, 0, sizeof(runtime));
	if (runtime_init(&runtime, state, config) != 0) {
		return -1;
	}

	Event *event_queue = initialize_events(state);
	if (!event_queue) {
		runtime_destroy(&runtime);
		return -1;
	}

	schedule_next_boost(&event_queue, &runtime);

	while (event_queue != NULL) {
		Event *current = pop_event(&event_queue);
		if (!current) break;

		state->current_time = current->time;

		switch (current->type) {
			case EVENT_ARRIVAL:
				handle_arrival(state, &runtime, &event_queue, current->process);
				break;
			case EVENT_COMPLETION:
				handle_completion(state, &runtime, current->process);
				break;
			case EVENT_QUANTUM_EXPIRE:
				handle_quantum_expire(state, &runtime, &event_queue);
				break;
			case EVENT_PRIORITY_BOOST:
				handle_priority_boost(state, &runtime, &event_queue);
				break;
			default:
				break;
		}

		free(current);
		dispatch_if_idle(state, &runtime, &event_queue);
	}

	calculate_metrics(state);
	print_gantt_chart(state);
	print_metrics(state);

	free_event_queue(&event_queue);
	runtime_destroy(&runtime);
	return 0;
}

