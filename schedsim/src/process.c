#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/process.h"

// Reads process definitions from a workload file
Process* load_processes(const char *filename, int *num_processes) {
    // Open workload file in text-read mode
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    // Allocate a fixed-capacity array for up to 100 processes
    Process *processes = malloc(sizeof(Process) * 100); // Initial capacity
    // Temporary buffer for each line read from file
    char line[256];
    // Number of valid process rows parsed so far
    int count = 0;

    // Read file line by line until EOF
    while (fgets(line, sizeof(line), file)) {
        // Skip comment lines and blank lines
        if (line[0] == '#' || line[0] == '\n') continue;

        // Target slot where parsed values will be stored
        Process *p = &processes[count];
        // Expected input format: <pid> <arrival_time> <burst_time>
        if (sscanf(line, "%s %d %d", p->pid, &p->arrival_time, &p->burst_time) == 3) {
            // Preemptive schedulers decrement from remaining_time to 0
            p->remaining_time = p->burst_time;
            p->start_time = -1; // Process hasn't started yet
            p->finish_time = 0; // Will be filled when process completes
            p->waiting_time = 0;// Computed later in metrics
            p->priority = 0;      // Default for MLFQ
            p->time_in_queue = 0; // Default for MLFQ
            // Count only successfully parsed process rows
            count++;
        }
    }

    // Publish total number of processes parsed
    *num_processes = count;
    // Close file descriptor before returning
    fclose(file);
    // Caller is responsible for freeing this array
    return processes;
}