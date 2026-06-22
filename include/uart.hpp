#pragma once
#include <cstdint>

#ifdef DEBUG
#define debug_puts(u, s) (u).puts(s)
#else
#define debug_puts(u, s)
#endif

struct UART {
    volatile uint8_t* regs;
    uint32_t shift;

    UART(uint64_t base, uint32_t reg_shift = 0) : regs((volatile uint8_t*)base), shift(reg_shift) {}

    void init() {
        regs[3 << shift] = 0x03;
        regs[2 << shift] = 0x07;
        regs[1 << shift] = 0x01;
    }

    bool data_ready() { return regs[5 << shift] & 0x01; }
    char read_char() { return (char)regs[0 << shift]; }
    bool tx_ready() { return regs[5 << shift] & 0x20; }

    void putchar(char c) {
        if (c == '\n') putchar('\r');
        while (!tx_ready()) ;
        regs[0 << shift] = (uint8_t)c;
    }

    void puts(const char* s) {
        while (*s) putchar(*s++);
    }

    char getchar() {
        while (!data_ready()) ;
        return read_char();
    }
};
