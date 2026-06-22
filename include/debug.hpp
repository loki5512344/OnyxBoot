#pragma once
#include "uart.hpp"

#ifdef DEBUG
static inline void debug_puts(UART& u, const char* s) {
    u.puts(s);
}
#else
static inline void debug_puts(UART&, const char*) {}
#endif
