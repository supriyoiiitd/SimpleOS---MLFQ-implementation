/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"
#include <stdio.h>

#define MLFQ_NLEVELS          5
/* 500 ticks ≈ 10 seconds */
#define RESET_PERIOD_TICKS    500 

extern struct process proc_set[MAX_NPROCESS + 1];

static void proc_set_status(int pid, enum proc_status status) {
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

int proc_alloc() {
    static uint curr_pid = 0;
    // START LOOP AT 1. NEVER TOUCH 0.
    for (uint i = 1; i <= MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;
            
            // Initialize Stats
            proc_set[i].turnaround_time = 0;
            proc_set[i].response_time = 0;
            proc_set[i].cpu_time = 0;
            proc_set[i].timer_interrupt_count = 0;
            
            // Use Process 0 as the Safe Global Clock Source
            proc_set[i].arrival_time = proc_set[0].timer_interrupt_count;
            
            proc_set[i].first_scheduled_time = 0;
            proc_set[i].has_run_before = 0;

            proc_set[i].queue_level = 0;
            proc_set[i].ticks_on_level = 0;

            return curr_pid;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    struct process *p = &proc_set[pid]; 

    printf("Process %d finished.\n", pid);
    printf("  Turnaround time: %u\n", p->turnaround_time);
    printf("  Response time: %u\n", p->response_time);
    printf("  CPU time: %u\n", p->cpu_time);
    printf("  Interrupts: %u\n", p->timer_interrupt_count);

    if (pid != GPID_ALL) {
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
    } else {
        for (uint i = 0; i < MAX_NPROCESS; i++)
            if (proc_set[i].pid >= GPID_USER_START &&
                proc_set[i].status != PROC_UNUSED) {
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].status = PROC_UNUSED;
            }
    }
}

void mlfq_update_level(struct process* p, ulonglong runtime) {
    p->ticks_on_level += (uint)runtime;
    
    /* Demotion Rule: (Level + 1) * 5 ticks */
    uint max_ticks = (p->queue_level + 1) * 5;

    if (p->ticks_on_level >= max_ticks) {
        if (p->queue_level < MLFQ_NLEVELS - 1) {
            p->queue_level++;
            p->ticks_on_level = 0;
        }
    }
}

void mlfq_reset_level() {
    if (!earth->tty_input_empty()) {
        for (uint i = 0; i < MAX_NPROCESS; i++) {
             if (proc_set[i].pid == GPID_SHELL && proc_set[i].status != PROC_UNUSED) {
                 proc_set[i].queue_level = 0;
                 proc_set[i].ticks_on_level = 0;
             }
        }
    }

    static uint last_reset_tick = 0;
    
    /* Read Global Clock from Process 0 */
    uint current_tick = proc_set[0].timer_interrupt_count;

    if (current_tick - last_reset_tick >= RESET_PERIOD_TICKS) {
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].status != PROC_UNUSED) {
                proc_set[i].queue_level = 0;
                proc_set[i].ticks_on_level = 0;
            }
        }
        last_reset_tick = current_tick;
    }
}

void proc_sleep(int pid, uint usec) { }
void proc_coresinfo() { }