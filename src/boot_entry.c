#include "types.h"

extern void boot_main(uint64_t fdt_addr);

extern uint8_t _sbss[];
extern uint8_t _ebss[];
extern uint8_t _stack_end[];

__attribute__((naked, section(".text.boot")))
void _start(void) {
    asm volatile(
        "bnez a0, 2f\n"
        "1:\n"
        /* Hart 0: save FDT (a1), clear BSS, call boot_main */
        "mv s0, a1\n"
        "la t0, _sbss\n"
        "la t1, _ebss\n"
        "3:\n"
        "bgeu t0, t1, 4f\n"
        "sw zero, 0(t0)\n"
        "addi t0, t0, 4\n"
        "j 3b\n"
        "4:\n"
        "la sp, _stack_end\n"
        "mv a0, s0\n"
        "call boot_main\n"
        "5:\n"
        "wfi\n"
        "j 5b\n"
        "2:\n"
        /* Secondary harts: park in WFI permanently */
        "wfi\n"
        "j 2b\n"
    );
}
