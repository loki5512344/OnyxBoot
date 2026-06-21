<p align="center">
  <img src="https://img.shields.io/badge/platform-RISC--V%2064--bit-green" alt="RISC-V 64">
  <img src="https://img.shields.io/badge/language-C%2B%2B20-blue" alt="C++20">
  <img src="https://img.shields.io/badge/size-%7E4KB-purple" alt="~4KB">
  <img src="https://img.shields.io/badge/license-GPL--3.0-red" alt="GPL-3.0">
</p>

<h1 align="center">SlipperBoot</h1>
<p align="center"><em>Minimalist RISC‑V 64‑bit bootloader written in freestanding C++</em></p>

---

**SlipperBoot** is a tiny, bare‑metal bootloader for RISC‑V 64‑bit (rv64gc) systems. It discovers hardware through the Flattened Device Tree (FDT), reads a kernel image (`kernel.elf`) from a VirtIO v2 modern MMIO block device, parses the ELF64 binary, and jumps to its entry point — all in ~4 KB of machine code.

It is the first‑stage loader for the [SlipperOS](https://github.com/anomalyco/SlipperOS) kernel.

---

## Quick Start

```console
$ make
riscv64-unknown-elf-g++ -march=rv64gc -mabi=lp64d -mcmodel=medany -ffreestanding -nostdlib -O2 -Wall -Wextra -fno-exceptions -fno-rtti boot_entry.cpp boot_main.cpp -T linker.ld -nostdlib -o bootloader.elf
riscv64-unknown-elf-objcopy -O binary bootloader.elf bootloader.bin
$ ls -lh bootloader.bin
-rw-r--r-- 1 user user 4.0K …
```

Place `bootloader.bin` at address `0x80000000` (QEMU `-kernel` or raw flash) and `kernel.elf` on a VirtIO block device. The bootloader will locate the device via FDT, load the ELF, and hand off control.

---

## Build Dependencies

| Tool                       | Purpose                  |
|----------------------------|--------------------------|
| `riscv64-unknown-elf-g++`  | Default cross‑compiler   |
| `riscv64-unknown-elf-objcopy` | Binary extraction     |
| `make`                     | Build orchestration      |

Override the toolchain prefix via `CROSS`:

```console
make CROSS=riscv64-elf          # GNU MCU Eclipse variant
```

---

## Project Structure

| File              | Role                                       |
|-------------------|--------------------------------------------|
| `boot_entry.cpp`  | Naked `_start`: BSS clear, stack setup, call `boot_main` |
| `boot_main.cpp`   | Main boot sequence (FDT, VirtIO, ELF load) |
| `uart.hpp`        | NS16550A UART driver                       |
| `virtio.hpp`      | VirtIO v2 modern MMIO block driver         |
| `elf.hpp`         | ELF64 parser (PT_LOAD copy to RAM)         |
| `fdt.hpp`         | Flattened Device Tree parser               |
| `linker.ld`       | Linker script (`BASE_ADDRESS = 0x80000000`)|
| `Makefile`        | Cross‑compilation rules                    |

---

## Technical Details

- **Freestanding** — no libc, no libstdc++, no global constructors, no static objects.
- **No assembly files** — all privileged code (BSS, stack, WFI) lives in `boot_entry.cpp` via inline `asm`.
- **FDT‑based discovery** — scans the device tree for `"ns16550a"` (UART console) and `"virtio,mmio"` (block device).
- **VirtIO v2 only** — modern MMIO interface with `VIRTIO_F_VERSION_1` mandatory.
- **ELF64 loader** — iterates `PT_LOAD` segments, copies them to their target addresses, and calls `entry(hart_id=0, fdt_addr)`.
- **Stack** — 4 KB. **Binary budget** — 32 KB max. Current build ~4 KB.
- **License** — GPL‑3.0.

---

## License

```
SlipperBoot — Minimalist RISC‑V 64‑bit bootloader
Copyright (C) 2024  Anomaly

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License.
```
