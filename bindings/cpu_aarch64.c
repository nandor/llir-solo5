/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hvt/bindings.h"
#include "cpu_aarch64.h"

extern void *cpu_exception_vectors;

struct regs {
    uint64_t xreg[30];
    uint64_t lr;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t esr_el1;
};

static const char *exception_modes[]= {
    "Synchronous Abort",
    "IRQ",
    "FIQ",
    "Error"
};

void cpu_init(void)
{
#ifdef __llir__
    __asm__ __volatile__("set $aarch64_vbar, %0"
            :
            : "r" ((uint64_t)&cpu_exception_vectors)
            : "memory");
#else
    __asm__ __volatile__("msr VBAR_EL1, %0"
            :
            : "r" ((uint64_t)&cpu_exception_vectors)
            : "memory");
#endif
}

static void dump_registers(struct regs *regs)
{
    uint32_t idx;

    log(INFO, "Solo5: Dump registers:\n");
    log(INFO, "\t ESR    : 0x%016lx\n", regs->esr_el1);
    log(INFO, "\t PC     : 0x%016lx\n", regs->elr_el1);
    log(INFO, "\t LR     : 0x%016lx\n", regs->lr);
    log(INFO, "\t PSTATE : 0x%016lx\n", regs->spsr_el1);

    for (idx = 0; idx < 28; idx+=4)
        log(INFO, "\t x%02d ~ x%02d: 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
            idx, idx + 3, regs->xreg[idx], regs->xreg[idx + 1],
            regs->xreg[idx + 2], regs->xreg[idx + 3]);

    log(INFO, "\t x28 ~ x29: 0x%016lx 0x%016lx\n", regs->xreg[28], regs->xreg[29]);
}

void cpu_trap_handler(struct regs *regs, int el, int mode, int is_valid)
{
    const uint32_t exception_cls = ESR_EC(regs->esr_el1);

    log(INFO, "Solo5: Trap: EL%d %s%s caught\n",
        el, is_valid ? "" : "Invalid ", exception_modes[mode]);

    if (exception_cls == ESR_EC_DABT_LOW ||
        exception_cls == ESR_EC_DABT_CUR) {
            uint64_t addr;
#ifdef __llir__
            __asm__ __volatile__("get.i64 %0, $aarch64_far" :"=r"(addr) ::);
#else
            __asm__ __volatile__("mrs %0, FAR_EL1" :"=&r"(addr) ::);
#endif
            log(INFO, "Data Abort Address: 0x%016lx\n", addr);
    }

    dump_registers(regs);

    PANIC("Fatal trap", NULL);
}

/* keeps track of cpu_intr_disable() depth */
int cpu_intr_depth = 1;

void cpu_intr_disable(void)
{
    #ifdef __llir__
        __asm__ __volatile__("aarch64_cli");
    #else
        __asm__ __volatile__("msr daifset, #2");
    #endif
    cpu_intr_depth++;
}

void cpu_intr_enable(void)
{
    assert(cpu_intr_depth > 0);

    if (--cpu_intr_depth == 0) {
        #ifdef __llir__
        __asm__ __volatile__("aarch64_sti");
        #else
            __asm__ __volatile__("msr daifclr, #2");
        #endif
    }
}

void cpu_halt(void)
{
    /* Copied from FreeBSD:sys/arm64/arm64/machdep.c */
    cpu_intr_disable();
    while (1) {
        #ifdef __llir__
            __asm __volatile("aarch64_wfi");
        #else
            __asm __volatile__("wfi");
        #endif
    }
}
