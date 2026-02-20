# SimpleOS---MLFQ-implementation
Priority Juggler: MLFQ Implementation
This project involves the implementation of a custom Multi-Level Feedback Queue (MLFQ) process scheduling policy within the EGOS-2000 (Earth and Grass Operating System) kernel. The system is designed to balance process responsiveness and throughput by dynamically adjusting process priorities based on their execution behavior.

🚀 Key Features
5-Level Priority Queue: Processes are managed across five distinct priority levels (0 to 4), where Level 0 is the highest priority.

Dynamic Demotion Logic: Implements a time-allotment rule where processes are demoted to a lower priority level if they exceed (Level + 1) * 5 ticks of execution time on their current level.

Starvation Prevention (Priority Boost): A global priority reset is performed every 500 ticks (approx. 10 seconds), returning all active processes to the highest priority queue to prevent long-running tasks from being starved.

Interactive Process Prioritization: The system monitors TTY input; if keyboard activity is detected, the Shell process is automatically boosted to Level 0 to maintain high interactive responsiveness.

Performance Analytics Suite: The kernel tracks and reports detailed metrics upon process completion, including Turnaround Time, Response Time, Total CPU Time, and total Timer Interrupts.

🏗️ System Architecture (EGOS-2000)
The project leverages the three-layer architecture of EGOS-2000 to implement scheduling logic:

Earth Layer: Provides the hardware-specific abstractions for the timer interrupts (Global Clock) and TTY/Disk interfaces.

Grass Layer: Contains the core proc_yield logic and Process Control Blocks (PCBs). This layer handles the MLFQ state transitions and context switching.

Application Layer: Houses the shell and user commands that run on top of the custom scheduler.
