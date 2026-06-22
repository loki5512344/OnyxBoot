#pragma once
#include <cstdint>
#include "uart.hpp"

enum DevType {
    DEV_NONE,
    DEV_VIRTIO,
    DEV_SDHCI,
};

struct BootDevice {
    DevType type;
    uint64_t base;
    uint32_t irq;
    bool avail;
};

static inline int boot_menu(UART& uart, BootDevice* devs, int ndevs) {
    uart.puts("\nSlipperBoot boot menu\n");
    uart.puts("--------------------\n");

    for (int i = 0; i < ndevs; i++) {
        if (!devs[i].avail) continue;
        const char* tname = devs[i].type == DEV_VIRTIO ? "VirtIO" : "MMC/SD";
        uart.puts("  ");
        uart.putchar('0' + i);
        uart.puts(": ");
        uart.puts(tname);
        uart.puts(" @ 0x");

        uint64_t b = devs[i].base;
        for (int j = 60; j >= 0; j -= 4) {
            uint8_t nib = (uint8_t)(b >> j) & 0xF;
            uart.putchar(nib < 10 ? '0' + nib : 'a' + nib - 10);
        }
        uart.putchar('\n');
    }
    uart.puts("--------------------\n");
    uart.puts("Select device (0-");
    uart.putchar('0' + ndevs - 1);
    uart.puts(", or enter for auto): ");

    char c = 0;
    for (volatile int timeout = 0; timeout < 20000000; timeout++) {
        if (uart.data_ready()) {
            c = uart.read_char();
            uart.putchar(c);
            uart.putchar('\n');
            break;
        }
    }

    if (c >= '0' && c <= '9') {
        int sel = c - '0';
        if (sel < ndevs && devs[sel].avail)
            return sel;
    }

    // Auto: return first available
    for (int i = 0; i < ndevs; i++)
        if (devs[i].avail) return i;

    return -1;
}
