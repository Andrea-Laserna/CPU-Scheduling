# Step 1: Set Up Project Structure
1. Organized files
2. Defined data structures
    - include/
        |_ process.h
        |_ scheduler.h
3. Makefile

# Step 2: Simulation Engine
1. Parse argument in main.c
2. Handle creation and loading of processes in process.c
    - load_processes: read input files
    - initialize_processL set initilal remaining time = burst time and set start time to -1
3. Event manager to manage timeline/even linked list in utils.c
    - initialize_events: takes list of processes and create event arrival for each
    - push_event: add new event to list
    - pop_event: take next event from head of list
4. Store the math in metrics.c
    - calculate_metrics: after simulation, calculate TT, WT, RT


# Execute
1.
```
make
```
2.
```
./schedsim --algorithm=FCFS --input=tests/workload1.txt
```

```
./schedsim --algorithm=RR --quantum=30 --input=tests/workload1.txt
```
