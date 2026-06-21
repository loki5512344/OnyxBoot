#include <cstdint>
#include "uart.hpp"
#include "virtio.hpp"
#include "elf.hpp"
#include "fdt.hpp"

extern "C" void boot_main(uint64_t fdt_addr) {
    uart_info ui = fdt_find_uart((const void*)fdt_addr);
    UART uart(ui.base);
    uart.init();
    uart.puts("SlipperBoot v0.2\n");

    mem_info mi = fdt_find_memory((const void*)fdt_addr);
    (void)mi;

    mmio_dev virtio_devs[8];
    int n = fdt_find_virtio((const void*)fdt_addr, virtio_devs, 8);
    if (n < 1) {
        uart.puts("no virtio\n");
        while (1)
            ;
    }

    VirtIOBlock disk(virtio_devs[0].base);
    if (!disk.probe()) {
        uart.puts("no disk\n");
        while (1)
            ;
    }

    static uint8_t kernel_buf[2 * 1024 * 1024] __attribute__((aligned(512)));

    for (int i = 0; i < 4096; i++)
        disk.read_sector(i, &kernel_buf[i * 512]);

    ELF64 elf(kernel_buf);
    if (!elf.valid()) {
        uart.puts("bad ELF\n");
        while (1)
            ;
    }
    elf.load_all();

    uart.puts("jumping to kernel\n");

    auto entry = elf.entry();
    void (*kernel_entry)(uint64_t, uint64_t) =
        reinterpret_cast<decltype(kernel_entry)>(entry);
    kernel_entry(0, fdt_addr);
}
