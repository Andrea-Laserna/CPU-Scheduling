# CPU Scheduling Simulator

## Members
- Andrea Laserna
- Cedric Oyco

## Implemented Algorithms
- FCFS
- SJF
- STCF
- RR
- MLFQ

## Build
```
make
```

## Usage
File-based workload:
```
./schedsim --algorithm=FCFS --input=tests/workload1.txt
```

Inline workload:
```
./schedsim --algorithm=FCFS --processes="A:0:240,B:10:180"
```

Round Robin with custom quantum:
```
./schedsim --algorithm=RR --quantum=30 --input=tests/workload1.txt
```

Run comparison table:
```
./schedsim --compare --input=tests/workload1.txt
```

## MLFQ Config
The MLFQ config file can be passed with `--mlfq-config=PATH` and supports
either key-value lines or positional lines. Example (key-value):
```
levels=3
quantums=10,30,-1
allotments=10,30,-1
boost_period=200
```

Example (positional):
```
3
10,30,-1
10,30,-1
200
```

## Example Output (FCFS workload1)
Command:
```
./schedsim --algorithm=FCFS --input=tests/workload1.txt
```

Expected averages:
```
Average Turnaround Time : 515.00
Average Waiting Time    : 359.00
Average Response Time   : 359.00
```

## Known Limitations
- Test automation is planned but not yet included (no `make test` target).
- Comparison mode uses the default MLFQ parameters (no config support in compare).
