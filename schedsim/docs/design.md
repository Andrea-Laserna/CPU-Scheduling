READY QUEUE design
used index array instead of pointers since indices are safer than pointers when arrays are reallocated later.
Elaboration:
The Scenario: Growing the Array
Imagine you read a workload file, but you don't know how many processes are in it.

You allocate an array for 10 processes using malloc().

You read the first 10 processes and put them in your processes array.

Process A (which is at processes[0]) arrives, so you put a pointer to it in your ready_queue.

The Disaster (Using Pointers)
Now, you read the 11th process from the file. Your array is full!
To make room, your code uses realloc() to double the array size to 20.

Here is what realloc() does behind the scenes:

It looks for a bigger block of memory.

It copies all your existing processes to that new, bigger location.

It deletes (frees) the old memory block.

If your ready_queue holds pointers (exact memory addresses), you now have a massive bug. The pointer in your queue is still pointing to the old memory address that was just deleted. This is called a "dangling pointer."
The next time the engine looks at the ready queue and tries to read ready_queue[0]->burst_time, your program will instantly crash with a Segmentation Fault.


1. Why not int ready_queue;?
If you write int ready_queue;, you are creating a box that can hold exactly one integer.
If Process 5 arrives, you can say ready_queue = 5;. But what happens if Process 2 arrives while Process 5 is still waiting? You have nowhere to put it! A queue needs to hold multiple items, so it must be a list (an array), not a single integer.

2. Why not a fixed array int ready_queue[100];?
You could create a fixed-size array. But what if your professor tests your code with a text file that has 101 processes?
Your program will try to put that 101st process into ready_queue[100], write data outside the boundaries of the array, and cause a Segmentation Fault. Since your grading rubric mentions being strictly graded on memory safety, hardcoding array sizes is a dangerous game.

3. The Solution: int *ready_queue; (The Dynamic Array)
In C, an array is really just a block of contiguous memory, and the variable name is just a pointer to the very first slot in that block.

When you declare int *ready_queue;, you are telling the computer: "I am going to need an array of integers, but I don't know how big it needs to be yet. Just hold this pointer ready."

Then, inside your Engine's initialization function (after you count how many processes are in the text file), you use malloc() to carve out exactly the right amount of space: