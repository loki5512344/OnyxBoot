# Default toolchain — override CROSS for alternatives:
#   riscv64-elf-g++     (bare-metal GNU MCU Eclipse)
#   riscv64-linux-gnu-g++ (Linux-capable, may work)
CROSS = riscv64-unknown-elf
CXX = $(CROSS)-g++
OBJCOPY = $(CROSS)-objcopy
CXXFLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany -ffreestanding -nostdlib -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti
LDFLAGS = -T linker.ld -nostdlib

bootloader.elf: boot_entry.cpp boot_main.cpp
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootloader.elf bootloader.bin
