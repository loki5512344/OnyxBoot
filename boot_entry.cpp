#include <cstdint>

extern "C" void boot_main(uint64_t fdt_addr);

extern uint8_t _sbss[];
extern uint8_t _ebss[];
extern uint8_t _stack_end[];

extern "C" __attribute__((naked, section(".text.boot")))
void _start() {
    asm volatile(
        "csrr t0, mhartid\n"
        "bnez t0, 1f\n"
        "mv s0, a1\n"
        "la t0, _sbss\n"
        "la t1, _ebss\n"
        "2:\n"
        "bgeu t0, t1, 3f\n"
        "sw zero, 0(t0)\n"
        "addi t0, t0, 4\n"
        "j 2b\n"
        "3:\n"
        "la sp, _stack_end\n"
        "mv a0, s0\n"
        "call boot_main\n"
        "1:\n"
        "wfi\n"
        "j 1b\n"
    );
}
