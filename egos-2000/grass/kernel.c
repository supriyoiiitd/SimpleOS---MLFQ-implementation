/*
 * (C) 2025, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 */

#include "process.h"
#include <string.h>

uint core_in_kernel;
uint core_to_proc_idx[NCORES];
struct process proc_set[MAX_NPROCESS + 1];

#define curr_proc_idx core_to_proc_idx[core_in_kernel]
#define curr_pid      proc_set[curr_proc_idx].pid
#define curr_status   proc_set[curr_proc_idx].status
#define curr_saved    proc_set[curr_proc_idx].saved_registers

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    asm("csrr %0, mhartid" : "=r"(core_in_kernel));
    asm("csrr %0, mepc" : "=r"(proc_set[curr_proc_idx].mepc));
    memcpy(curr_saved, SAVED_REGISTER_ADDR, SAVED_REGISTER_SIZE);

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    asm("csrw mepc, %0" ::"r"(proc_set[curr_proc_idx].mepc));
    memcpy(SAVED_REGISTER_ADDR, curr_saved, SAVED_REGISTER_SIZE);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield();
static void proc_try_syscall(struct process* proc);

static void excp_entry(uint id) {
    if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
        uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
        memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr, sizeof(struct syscall));
        proc_set[curr_proc_idx].syscall.status = PENDING;

        proc_set_pending(curr_pid);
        proc_set[curr_proc_idx].mepc += 4;
        proc_try_syscall(&proc_set[curr_proc_idx]);
        proc_yield();
        return;
    }
    FATAL("excp_entry: kernel got exception %d", id);
}

static void intr_entry(uint id) {
    if (id != INTR_ID_TIMER) FATAL("excp_entry: kernel got interrupt %d", id);
    
    /* SAFE: Increment Process 0 tick count as Global Clock */
    proc_set[0].timer_interrupt_count++;

    if (curr_proc_idx != 0) {
        proc_set[curr_proc_idx].timer_interrupt_count++;
    }
    proc_yield();
}

static void proc_yield() {
    // 1. Mark current as RUNNABLE if it was RUNNING
    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);

    /* Calculate Runtime using Process 0 Clock */
    static uint last_schedule_tick = 0;
    uint current_tick = proc_set[0].timer_interrupt_count;
    
    uint runtime = 0;
    if (current_tick > last_schedule_tick) {
        runtime = current_tick - last_schedule_tick;
    }
    last_schedule_tick = current_tick;

    if (curr_pid < MAX_NPROCESS && curr_status != PROC_UNUSED) {
         struct process *curr = &proc_set[curr_proc_idx];
         curr->cpu_time += runtime;
         mlfq_update_level(curr, (ulonglong)runtime);

         if (curr->has_run_before == 0) {
             curr->first_scheduled_time = current_tick;
             curr->response_time = curr->first_scheduled_time - curr->arrival_time;
             curr->has_run_before = 1;
         }
         curr->turnaround_time = current_tick - curr->arrival_time;
    }

    mlfq_reset_level();

    int next_idx = -1;
    int min_level = 255; 

    /* Find Best Process (MLFQ Rule 1) */
    // Note: This loop scans OTHERS but skips current due to modulo math wrapping to 0
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        int idx = (curr_proc_idx + i) % (MAX_NPROCESS + 1);
        if (idx == 0) continue; 

        struct process* p = &proc_set[idx];
        if (p->status == PROC_PENDING_SYSCALL) proc_try_syscall(p);

        if (p->status == PROC_READY || p->status == PROC_RUNNABLE) {
            if (p->queue_level < min_level) {
                min_level = p->queue_level;
                next_idx = idx;
            }
        }
    }

    /* Fallback Logic - THE FIX IS HERE */
    if (next_idx == -1) {
        // If we found nobody else, check if WE (current process) are still runnable.
        // curr_status is a macro that reads memory, so it sees PROC_RUNNABLE now.
        if (curr_status == PROC_RUNNABLE) {
             next_idx = curr_proc_idx; /* Keep running current */
        } else {
             /* Really no one to run. Crash. */
             FATAL("proc_yield: no process to run on core %d", core_in_kernel);
        }
    }

    curr_proc_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->mmu_flush_cache();
    if (curr_status == PROC_READY) {
        curr_saved[0]                = APPS_ARG;
        curr_saved[1]                = APPS_ARG + 4;
        proc_set[curr_proc_idx].mepc = APPS_ENTRY;
    }
    proc_set_running(curr_pid);
    earth->timer_reset(core_in_kernel);
}

static void proc_try_send(struct process* sender) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process* dst = &proc_set[i];
        if (dst->pid == sender->syscall.receiver && dst->status != PROC_UNUSED) {
            if (!(dst->syscall.type == SYS_RECV && dst->syscall.status == PENDING)) return;
            if (!(dst->syscall.sender == GPID_ALL || dst->syscall.sender == sender->pid)) return;

            dst->syscall.status = DONE;
            dst->syscall.sender = sender->pid;
            memcpy(dst->syscall.content, sender->syscall.content, SYSCALL_MSG_LEN);
            return;
        }
    }
    FATAL("proc_try_send: unknown receiver pid=%d", sender->syscall.receiver);
}

static void proc_try_recv(struct process* receiver) {
    if (receiver->syscall.status == PENDING) return;
    uint syscall_paddr = earth->mmu_translate(receiver->pid, SYSCALL_ARG);
    memcpy((void*)syscall_paddr, &receiver->syscall, sizeof(struct syscall));
    proc_set_runnable(receiver->pid);
    proc_set_runnable(receiver->syscall.sender);
}

static void proc_try_syscall(struct process* proc) {
    switch (proc->syscall.type) {
    case SYS_RECV: proc_try_recv(proc); break;
    case SYS_SEND: proc_try_send(proc); break;
    default: FATAL("proc_try_syscall: unknown syscall type=%d", proc->syscall.type);
    }
}