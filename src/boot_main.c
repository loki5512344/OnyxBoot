#include "types.h"
#include "uart.h"
#include "virtio.h"
#include "sdhci.h"
#include "elf.h"
#include "fdt.h"
#include "bootmenu.h"

extern uint8_t _sbss[];
extern uint8_t _stack_end[];

VirtQueue g_vq __attribute__((aligned(4096)));

typedef bool (*blk_read_t)(uint64_t lba, void* buf, void* priv);

bool fat32_read_file(blk_read_t read, void* priv, const char* name,
                     uint8_t* buf, uint32_t* size, uint32_t max_size);
bool ext4_read_file(blk_read_t read, void* priv, const char* path,
                    uint8_t* buf, uint32_t* size, uint32_t max_size);

/* Wrappers for VirtIO and SDHCI read_sector */
static bool vio_read_wrap(uint64_t lba, void* buf, void* priv) {
    return vio_read_sector((VirtIOBlock*)priv, lba, buf);
}
static bool sdhci_read_wrap(uint64_t lba, void* buf, void* priv) {
    return sdhci_read_sector((SDHCI*)priv, lba, buf);
}

void boot_main(uint64_t fdt_addr) {
    uart_info ui = fdt_find_uart((const void*)fdt_addr);
    UART uart;
    uart.regs = (volatile uint8_t*)ui.base;
    uart.shift = ui.reg_shift;
    uart_init(&uart);

    const char* model = fdt_get_model((const void*)fdt_addr, "unknown");

    uart_puts(&uart, "     ███████\n");
    uart_puts(&uart, "  ███░░░░░███\n");
    uart_puts(&uart, " ███     ░░███ ████████   █████ ████ █████ █████\n");
    uart_puts(&uart, "░███      ░███░░███░░███ ░░███ ░███ ░░███ ░░███\n");
    uart_puts(&uart, "░███      ░███ ░███ ░███  ░███ ░███  ░░░█████░\n");
    uart_puts(&uart, "░░███     ███  ░███ ░███  ░███ ░███   ███░░░███\n");
    uart_puts(&uart, " ░░░███████░   ████ █████ ░░███████  █████ █████\n");
    uart_puts(&uart, "   ░░░░░░░    ░░░░ ░░░░░   ░░░░░███ ░░░░░ ░░░░░\n");
    uart_puts(&uart, "                           ███ ░███\n");
    uart_puts(&uart, "                          ░░██████\n");
    uart_puts(&uart, "                           ░░░░░░\n");

    uart_puts(&uart, "OnyxBoot v0.4 [");
    uart_puts(&uart, model);
    uart_puts(&uart, "]\n");

    BootDevice devs[16];
    int ndevs = 0;

    mmio_dev virtio_devs[8];
    int nv = fdt_find_virtio((const void*)fdt_addr, virtio_devs, 8);
    for (int i = 0; i < nv; i++) {
        VirtIOBlock disk;
        vio_init(&disk, virtio_devs[i].base);
        if (vio_probe(&disk)) {
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
        SDHCI mmc;
        sdhci_init(&mmc, sdhci_devs[i].base);
        if (sdhci_probe(&mmc)) {
            devs[ndevs].type = DEV_SDHCI;
            devs[ndevs].base = sdhci_devs[i].base;
            devs[ndevs].irq = sdhci_devs[i].irq;
            devs[ndevs].avail = true;
            ndevs++;
        }
    }

    if (ndevs < 1) {
        uart_puts(&uart, "no boot device\n");
        while (1) ;
    }

    int sel = boot_menu(&uart, devs, ndevs);
    if (sel < 0 || sel >= ndevs || !devs[sel].avail) {
        uart_puts(&uart, "invalid device\n");
        while (1) ;
    }

    mem_info dram = fdt_find_memory((const void*)fdt_addr);
    uint32_t max_size = 4 * 1024 * 1024;
    if (dram.size < max_size + 0x10000) max_size = (uint32_t)(dram.size - 0x10000);
    uint64_t kernel_buf_addr = (dram.base + dram.size - max_size) & ~(uint64_t)0xFFF;
    uint8_t* kernel_buf = (uint8_t*)kernel_buf_addr;
    uint32_t kernel_size = 0;
    bool ok = false;

    uart_puts(&uart, "loading kernel.elf\n");

    if (devs[sel].type == DEV_VIRTIO) {
        VirtIOBlock disk;
        vio_init(&disk, devs[sel].base);
        if (vio_probe(&disk)) {
            ok = fat32_read_file(vio_read_wrap, &disk, "kernel.elf",
                                 kernel_buf, &kernel_size, max_size)
              || ext4_read_file(vio_read_wrap, &disk, "kernel.elf",
                                kernel_buf, &kernel_size, max_size);
        }
    } else {
        SDHCI mmc;
        sdhci_init(&mmc, devs[sel].base);
        if (sdhci_probe(&mmc)) {
            ok = fat32_read_file(sdhci_read_wrap, &mmc, "kernel.elf",
                                 kernel_buf, &kernel_size, max_size)
              || ext4_read_file(sdhci_read_wrap, &mmc, "kernel.elf",
                                kernel_buf, &kernel_size, max_size);
        }
    }

    if (!ok) {
        uart_puts(&uart, "read error\n");
        while (1) ;
    }

    ELF64 elf;
    elf64_init(&elf, kernel_buf);
    if (!elf64_valid(&elf)) {
        uart_puts(&uart, "bad ELF\n");
        while (1) ;
    }

    uint64_t boot_start = (uint64_t)_sbss;
    uint64_t boot_end = (uint64_t)_stack_end;
    if (!elf64_check_safe(&elf, boot_start, boot_end)) {
        uart_puts(&uart, "ELF overlaps bootloader\n");
        while (1) ;
    }

    elf64_load_all(&elf);

    uart_puts(&uart, "jumping to kernel\n");

    uint64_t entry = elf64_entry(&elf);
    void (*kernel_entry)(uint64_t, uint64_t) = (void (*)(uint64_t, uint64_t))entry;
    kernel_entry(0, fdt_addr);
}
