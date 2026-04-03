#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/process.h"

// Reads process definitions from a workload file
Process *load_processes(const char *filename, int *num_processes)
{
    /*
    issue fixed: hardcoded capacity of 100 processes
    */

    // Open workload file in text-read mode
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Failed to open file");
        return NULL;
    }

    // Fix overwriting illegal memory with realloc
    int capacity = 100;

    Process *processes = malloc(sizeof(Process) * capacity); // Initial capacity

    // Check initial malloc
    if (!processes)
    {
        fclose(file);
        return NULL;
    }

    char line[256];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n')
            continue;

        // Check capacity before trying to access processes[count]
        if (count >= capacity)
        {
            int new_capacity = capacity * 2;
            Process *temp = realloc(processes, sizeof(Process) * new_capacity);

            if (!temp)
            {
                fprintf(stderr, "Error: malloc failed during resize");
                free(processes);
                fclose(file);
                return NULL;
            }
            processes = temp;
            capacity = new_capacity;
        }

        Process *p = &processes[count];

        // Expected input format: <pid> <arrival_time> <burst_time>
        if (sscanf(line, "%15s %d %d", p->pid, &p->arrival_time, &p->burst_time) == 3)
        {

            p->remaining_time = p->burst_time;
            p->start_time = -1;   // Process hasn't started yet
            p->finish_time = 0;   // Will be filled when process completes
            p->waiting_time = 0;  // Computed later in metrics
            p->priority = 0;      // Default for MLFQ
            p->time_in_queue = 0; // Default for MLFQ

            count++;
        }
    }

    *num_processes = count;
    fclose(file);

    return processes;
}