#include <cstdint>
#include "uart.hpp"
#include "virtio.hpp"
#include "sdhci.hpp"
#include "elf.hpp"
#include "fdt.hpp"
#include "bootmenu.hpp"

extern uint8_t _sbss[];
extern uint8_t _stack_end[];

alignas(4096) VirtQueue g_vq;

typedef bool (*blk_read_t)(uint64_t lba, void* buf, void* priv);
bool fat32_read_file(blk_read_t read, void* priv, const char* name,
                     uint8_t* buf, uint32_t* size, uint32_t max_size);
bool ext4_read_file(blk_read_t read, void* priv, const char* path,
                    uint8_t* buf, uint32_t* size, uint32_t max_size);

extern "C" __attribute__((used)) void boot_main(uint64_t fdt_addr) {
    uart_info ui = fdt_find_uart((const void*)fdt_addr);
    UART uart(ui.base, ui.reg_shift);
    uart.init();
    const char* model = fdt_get_model((const void*)fdt_addr, "unknown");

    uart.puts("     ███████\n");
    uart.puts("  ███░░░░░███\n");
    uart.puts(" ███     ░░███ ████████   █████ ████ █████ █████\n");
    uart.puts("░███      ░███░░███░░███ ░░███ ░███ ░░███ ░░███\n");
    uart.puts("░███      ░███ ░███ ░███  ░███ ░███  ░░░█████░\n");
    uart.puts("░░███     ███  ░███ ░███  ░███ ░███   ███░░░███\n");
    uart.puts(" ░░░███████░   ████ █████ ░░███████  █████ █████\n");
    uart.puts("   ░░░░░░░    ░░░░ ░░░░░   ░░░░░███ ░░░░░ ░░░░░\n");
    uart.puts("                           ███ ░███\n");
    uart.puts("                          ░░██████\n");
    uart.puts("                           ░░░░░░\n");

    uart.puts("OnyxBoot v0.4 [");
    uart.puts(model);
    uart.puts("]\n");

    BootDevice devs[16];
    int ndevs = 0;

    mmio_dev virtio_devs[8];
    int nv = fdt_find_virtio((const void*)fdt_addr, virtio_devs, 8);
    for (int i = 0; i < nv; i++) {
        VirtIOBlock disk(virtio_devs[i].base);
        if (disk.probe()) {
            devs[ndevs].type = DEV_VIRTIO;
            devs[ndevs].base = virtio_devs[i].base;
            devs[ndevs].irq = virtio_devs[i].irq;
            devs[ndevs].avail = true;
            ndevs++;
        }
    }

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

    mem_info dram = fdt_find_memory((const void*)fdt_addr);
    uint32_t max_size = 4 * 1024 * 1024;
    if (dram.size < max_size + 0x10000) max_size = (uint32_t)(dram.size - 0x10000);
    uint64_t kernel_buf_addr = 0x80400000;
    uint8_t* kernel_buf = (uint8_t*)kernel_buf_addr;
    uint32_t kernel_size = 0;
    bool ok = false;

    uart.puts("loading kernel.elf\n");

    if (devs[sel].type == DEV_VIRTIO) {
        VirtIOBlock disk(devs[sel].base);
        if (disk.probe()) {
            auto read_sec = [](uint64_t lba, void* buf, void* priv) -> bool {
                return ((VirtIOBlock*)priv)->read_sector(lba, buf);
            };
            ok = fat32_read_file(read_sec, &disk, "kernel.elf",
                                 kernel_buf, &kernel_size, max_size)
              || ext4_read_file(read_sec, &disk, "kernel.elf",
                                kernel_buf, &kernel_size, max_size);
        }
    } else {
        SDHCI mmc(devs[sel].base);
        if (mmc.probe()) {
            auto read_sec = [](uint64_t lba, void* buf, void* priv) -> bool {
                return ((SDHCI*)priv)->read_sector(lba, buf);
            };
            ok = fat32_read_file(read_sec, &mmc, "kernel.elf",
                                 kernel_buf, &kernel_size, max_size)
              || ext4_read_file(read_sec, &mmc, "kernel.elf",
                                kernel_buf, &kernel_size, max_size);
        }
    }

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

    uart.puts("jumping to kernel\n");

    auto entry = elf.entry();
    void (*kernel_entry)(uint64_t, uint64_t) =
        reinterpret_cast<decltype(kernel_entry)>(entry);
    kernel_entry(0, fdt_addr);
}
