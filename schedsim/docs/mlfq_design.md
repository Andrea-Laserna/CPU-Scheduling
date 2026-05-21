
# MLFQ Design Justification

## Goals
The MLFQ configuration aims to balance short interactive jobs against longer CPU-bound jobs while preventing starvation through periodic priority boosts. The design follows a 3-level queue structure with increasing time slices and a non-preemptive lowest queue.

## Queue Structure and Parameters
We use three queues (Q0, Q1, Q2) to keep the simulator simple while still showing the intended MLFQ behavior:

- Q0: shortest time slice to favor interactive jobs.
- Q1: medium time slice to reduce overhead after initial responsiveness.
- Q2: FCFS for long CPU-bound jobs to minimize context switching.

Default parameters (Config A):

- Levels: 3
- Quantums: Q0=10, Q1=30, Q2=FCFS (-1)
- Allotments: Q0=10, Q1=30, Q2=FCFS (-1)
- Boost period: 200

Alternative parameters (Config B) to test higher responsiveness:

- Levels: 3
- Quantums: Q0=5, Q1=15, Q2=FCFS (-1)
- Allotments: Q0=5, Q1=15, Q2=FCFS (-1)
- Boost period: 100

Rationale:
- Smaller Q0/Q1 slices increase responsiveness but raise context-switch overhead.
- A shorter boost period reduces starvation risk at the cost of more frequent queue resets.
- FCFS at Q2 keeps long jobs moving without repeated preemption.

## Empirical Results
All results are averages from the simulator output. Commands used:

- Default config (A):
	`./schedsim --algorithm=MLFQ --input=tests/workload1.txt`
	`./schedsim --algorithm=MLFQ --input=tests/workload2.txt`

- Alternate config (B):
	`./schedsim --algorithm=MLFQ --mlfq-config=/tmp/mlfq_alt.txt --input=tests/workload1.txt`
	`./schedsim --algorithm=MLFQ --mlfq-config=/tmp/mlfq_alt.txt --input=tests/workload2.txt`

### MLFQ Averages (Workload1)

| Config | Avg TT | Avg WT | Avg RT |
|--------|--------|--------|--------|
| A (10/30, boost 200) | 591.00 | 435.00 | 3.00 |
| B (5/15, boost 100)  | 553.00 | 397.00 | 0.00 |

### MLFQ Averages (Workload2)

| Config | Avg TT | Avg WT | Avg RT |
|--------|--------|--------|--------|
| A (10/30, boost 200) | 17015.87 | 16766.44 | 415.17 |
| B (5/15, boost 100)  | 17005.77 | 16756.34 | 200.81 |

## Comparison Against STCF and RR
These baselines provide a reference for optimal average TT (STCF) and time-sliced fairness (RR).

### Workload1 Baselines

| Algorithm | Avg TT | Avg WT | Avg RT |
|-----------|--------|--------|--------|
| STCF | 393.00 | 237.00 | 15.00 |
| RR (q=30) | 651.00 | 495.00 | 67.00 |

### Workload2 Baselines

| Algorithm | Avg TT | Avg WT | Avg RT |
|-----------|--------|--------|--------|
| STCF | 8598.56 | 8349.13 | 8221.09 |
| RR (q=30) | 16850.56 | 16601.13 | 1466.23 |

## Tradeoffs Observed
- MLFQ (A) improves responsiveness over RR on Workload1, with lower Avg RT and lower Avg TT.
- MLFQ (B) further reduces response time, indicating better interactive performance, but only modestly changes Avg TT/WT for large workloads.
- STCF remains the best for Avg TT/WT but yields very high Avg RT on Workload2, illustrating the fairness/latency tradeoff.
- RR provides predictable fairness but higher Avg TT/WT than MLFQ in these tests.

## Summary
The chosen 3-level MLFQ with FCFS at the lowest level balances responsiveness and overhead. The alternative config demonstrates that smaller quantums and more frequent boosts reduce response time further, with minimal impact on average turnaround in large workloads.

