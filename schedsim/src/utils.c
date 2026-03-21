#include <stdlib.h>
#include "scheduler.h"
#include "utils.h"
// Event - node in the queue that records the process
// State - processes, num_processes, current_time

// Use state to create the initial arrival events for all processes
Event* initialize_events(SchedulerState *state) {
    // Head pointer of the sorted event linked list
    Event *head = NULL;

    // Create one ARRIVAL event for each process
    for (int i = 0; i < state->num_processes; i++) {
        // Allocate a new event node
        Event *new_event = malloc(sizeof(Event));
        // Event happens exactly at process arrival time
        new_event->time = state->processes[i].arrival_time;
        // Initial events are all arrivals
        new_event->type = EVENT_ARRIVAL;
        // Keep direct pointer to that process
        new_event->process = &state->processes[i];
        // Initialize next pointer before insertion
        new_event->next = NULL;

        // Insert into sorted linked list (by time)
        if (!head || new_event->time < head->time) {
            // Insert at beginning if list empty or event is earliest
            new_event->next = head;
            head = new_event;
        } else {
            // Find insertion point for new event where ordering by time is preserved
            Event *curr = head;
            // Keep moving until nextnode where current time > next time
            while (curr->next && curr->next->time <= new_event->time) {
                curr = curr->next;
            }
            // Link new event after current
            // Insert new node between curr and curr next
            new_event->next = curr->next;
            curr->next = new_event;
        }
    }
    // Return head of fully built, time-sorted event queue
    return head;
}

// Pulls the next event from the queue
Event* pop_event(Event **head) {
    // Guard against NULL pointer input or empty list
    // Pointer to head pointer is NULL || List empty head points to NULL
    if (!head || !*head) return NULL;
    // Remove current head and advance list
    // Save current head to temp
    Event *temp = *head;
    // Advance head to 2nd node to remove first node from list
    *head = (*head)->next;
    // Return detached event node to caller
    return temp;
}

int enqueue_ready(SchedulerState *state, int idx) {
    if (state->ready_count == state->ready_capacity) return -1;
    state->ready_queue[state->ready_tail] = idx;
    state->ready_tail = (state->ready_tail + 1) % state->ready_capacity;
    state->ready_count++;
    return 0;
}

int dequeue_ready(SchedulerState *state) {
    if (state->ready_count == 0) return -1;
    int idx = state->ready_queue[state->ready_head];
    state->ready_head = (state->ready_head + 1) % state->ready_capacity;
    state->ready_count--;
    return idx;
}