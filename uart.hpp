#pragma once
#include <cstdint>

struct UART {
    volatile uint8_t* regs;

    UART(uint64_t base) : regs((volatile uint8_t*)base) {}

    void init() {
        regs[3] = 0x03;
        regs[2] = 0x07;
        regs[1] = 0x01;
    }

    void putchar(char c) {
        if (c == '\n')
            putchar('\r');
        while (!(regs[5] & 0x20))
            ;
        regs[0] = (uint8_t)c;
    }

    void puts(const char* s) {
        while (*s)
            putchar(*s++);
    }

    char getchar() {
        while (!(regs[5] & 0x01))
            ;
        return (char)regs[0];
    }
};
