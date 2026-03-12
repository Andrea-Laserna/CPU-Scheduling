#ifndef PROCESS_H
#define PROCESS_H

typedef struct {
	char pid[16];           // Process identifier
	int arrival_time;       // When process arrives
	int burst_time;         // Total CPU time needed
	int remaining_time;     // For preemptive algorithms
	int start_time;         // When first executed (for RT)
	int finish_time;        // When completed (for TT)
	int waiting_time;       // Time spent waiting
	int priority;           // For MLFQ
	int time_in_queue;      // For MLFQ allotment tracking
} Process;

// Load processes from a workload text file into a dynamically allocated array
// Success -> returns pointer to array and writes count to num_processes
// Error -> returns NULL
Process *load_processes(const char *filename, int *num_processes);

#endif

