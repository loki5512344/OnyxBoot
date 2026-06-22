CROSS ?= riscv64-unknown-elf
CXX = $(CROSS)-g++
OBJCOPY = $(CROSS)-objcopy

CXXFLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany -ffreestanding -nostdlib -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti -Iinclude
LDFLAGS = -T linker.ld -nostdlib

SRCS = src/boot_entry.cpp src/boot_main.cpp

all: bootloader.bin

bootloader.elf: $(SRCS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootloader.elf bootloader.bin

.PHONY: all clean
