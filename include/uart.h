#pragma once
#include "types.h"

#ifdef DEBUG
#define debug_puts(u, s) uart_puts(u, s)
#else
#define debug_puts(u, s)
#endif

typedef struct {
    volatile uint8_t* regs;
    uint32_t shift;
} UART;

static inline void uart_init(UART* u) {
    u->regs[3 << u->shift] = 0x03;
    u->regs[2 << u->shift] = 0x07;
    u->regs[1 << u->shift] = 0x01;
}

static inline bool uart_data_ready(UART* u) {
    return u->regs[5 << u->shift] & 0x01;
}

static inline char uart_read_char(UART* u) {
    return (char)u->regs[0 << u->shift];
}

static inline bool uart_tx_ready(UART* u) {
    return u->regs[5 << u->shift] & 0x20;
}

static inline void uart_putchar(UART* u, char c) {
    if (c == '\n') uart_putchar(u, '\r');
    while (!uart_tx_ready(u)) ;
    u->regs[0 << u->shift] = (uint8_t)c;
}

static inline void uart_puts(UART* u, const char* s) {
    while (*s) uart_putchar(u, *s++);
}

static inline char uart_getchar(UART* u) {
    while (!uart_data_ready(u)) ;
    return uart_read_char(u);
}
