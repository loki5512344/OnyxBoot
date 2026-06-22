#include <cstdint>
#include "uart.hpp"
#include "virtio.hpp"
#include "sdhci.hpp"
#include "elf.hpp"
#include "fdt_dev.hpp"
#include "bootmenu.hpp"
#include "debug.hpp"

extern uint8_t _sbss[];
extern uint8_t _stack_end[];

extern "C" void boot_main(uint64_t fdt_addr) {
    uart_info ui = fdt_find_uart((const void*)fdt_addr);
    UART uart(ui.base, ui.reg_shift);
    uart.init();
    const char* model = fdt_get_model((const void*)fdt_addr, "unknown");

    uart.puts("SlipperBoot v0.4 [");
    uart.puts(model);
    uart.puts("]\n");

    BootDevice devs[16];
    int ndevs = 0;

    // find VirtIO devices from FDT, probe each one
    mmio_dev virtio_devs[8];
    int nv = fdt_find_virtio((const void*)fdt_addr, virtio_devs, 8);
    for (int i = 0; i < nv; i++) {
#ifdef DEBUG
        {
            debug_puts(uart, "found virtio at 0x");
            for (int j = 56; j >= 0; j -= 8)
                uart.putchar("0123456789abcdef"[(virtio_devs[i].base >> j) & 0xF]);
            debug_puts(uart, "\n");
        }
#endif
        VirtIOBlock disk(virtio_devs[i].base);
        if (disk.probe()) {
            devs[ndevs].type = DEV_VIRTIO;
            devs[ndevs].base = virtio_devs[i].base;
            devs[ndevs].irq = virtio_devs[i].irq;
            devs[ndevs].avail = true;
            ndevs++;
        }
    }

    // find SDHCI devices from FDT
    mmio_dev sdhci_devs[8];
    int ns = fdt_find_sdhci((const void*)fdt_addr, sdhci_devs, 8);
    for (int i = 0; i < ns; i++) {
        devs[ndevs].type = DEV_SDHCI;
        devs[ndevs].base = sdhci_devs[i].base;
        devs[ndevs].irq = sdhci_devs[i].irq;
        devs[ndevs].avail = true;
        ndevs++;
    }

    if (ndevs < 1) {
        uart.puts("no boot device\n");
        while (1) ;
    }

    int sel = boot_menu(uart, devs, ndevs);
    if (sel < 0 || sel >= ndevs || !devs[sel].avail) {
        uart.puts("invalid device\n");
        while (1) ;
    }

    uint8_t* kernel_buf = (uint8_t*)0x80800000;
    uint32_t nsectors = 4096;
    mem_info dram = fdt_find_memory((const void*)fdt_addr);
    uint64_t buf_end = (uint64_t)kernel_buf + (uint64_t)nsectors * 512;
    if (buf_end > dram.base + dram.size)
        nsectors = (uint32_t)((dram.base + dram.size - (uint64_t)kernel_buf) / 512);

    uart.puts("loading kernel.elf [");

    bool ok = true;
    if (devs[sel].type == DEV_VIRTIO) {
        VirtIOBlock disk(devs[sel].base);
        if (!disk.probe()) ok = false;
        for (uint32_t i = 0; i < nsectors && ok; i++) {
#ifdef DEBUG
            {
                debug_puts(uart, "loading sector ");
                uart.putchar('0' + (i / 10) % 10);
                uart.putchar('0' + i % 10);
                debug_puts(uart, "\n");
            }
#endif
            if (!disk.read_sector(i, &kernel_buf[i * 512])) ok = false;
            if ((i & 7) == 0) uart.putchar('#');
        }
    } else {
        SDHCI mmc(devs[sel].base);
        if (!mmc.probe()) ok = false;
        for (uint32_t i = 0; i < nsectors && ok; i++) {
            if (!mmc.read_sector(i, &kernel_buf[i * 512])) ok = false;
            if ((i & 7) == 0) uart.putchar('#');
        }
    }
    uart.puts("]\n");
    if (!ok) {
        uart.puts("read error\n");
        while (1) ;
    }

    ELF64 elf(kernel_buf);
    if (!elf.valid()) {
        uart.puts("bad ELF\n");
        while (1) ;
    }

    uint64_t boot_start = (uint64_t)_sbss;
    uint64_t boot_end = (uint64_t)_stack_end;
    if (!elf.check_safe(boot_start, boot_end)) {
        uart.puts("ELF overlaps bootloader\n");
        while (1) ;
    }

    elf.load_all();

#ifdef DEBUG
    {
        debug_puts(uart, "jump to 0x");
        for (int j = 56; j >= 0; j -= 8)
            uart.putchar("0123456789abcdef"[(elf.entry() >> j) & 0xF]);
        debug_puts(uart, "\n");
    }
#endif
    uart.puts("jumping to kernel\n");

    auto entry = elf.entry();
    void (*kernel_entry)(uint64_t, uint64_t) =
        reinterpret_cast<decltype(kernel_entry)>(entry);
    kernel_entry(0, fdt_addr);
}
