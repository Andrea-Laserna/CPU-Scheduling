#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/process.h"

// Reads process definitions from a workload file
Process* load_processes(const char *filename, int *num_processes) {
    /*
    issue to fix: hardcoded capacity of 100 processes 
    */

    // Open workload file in text-read mode
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    Process *processes = malloc(sizeof(Process) * 100); // Initial capacity
    char line[256];
    int count = 0;  
    
    while (fgets(line, sizeof(line), file)) {
        
        if (line[0] == '#' || line[0] == '\n') continue;

        Process *p = &processes[count];
        
        // Expected input format: <pid> <arrival_time> <burst_time>
        if (sscanf(line, "%s %d %d", p->pid, &p->arrival_time, &p->burst_time) == 3) {

            p->remaining_time = p->burst_time;
            p->start_time = -1; // Process hasn't started yet
            p->finish_time = 0; // Will be filled when process completes
            p->waiting_time = 0;// Computed later in metrics
            p->priority = 0;      // Default for MLFQ
            p->time_in_queue = 0; // Default for MLFQ

            count++;
        }
    }


    *num_processes = count;
    fclose(file);

    return processes;
}