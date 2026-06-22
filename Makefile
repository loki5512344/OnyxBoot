CROSS ?= riscv64-elf
CXX = $(CROSS)-g++
OBJCOPY = $(CROSS)-objcopy

CXXFLAGS = -march=rv64gc -mabi=lp64d -mcmodel=medany \
    -Os -flto \
    -ffunction-sections -fdata-sections \
    -fno-exceptions -fno-rtti -fno-threadsafe-statics \
    -fno-use-cxa-atexit \
    -fomit-frame-pointer -fno-ident -g0 \
    -ffreestanding -nostdlib -Iinclude -Wall -Wextra

LDFLAGS = -T linker.ld -nostdlib \
    -Wl,--gc-sections -Wl,--strip-all -Wl,-n

SRCS = src/boot_entry.cpp src/boot_main.cpp fs/fat.cpp fs/ext4.cpp

all: bootloader.bin

bootloader.elf: $(SRCS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootloader.elf bootloader.bin

test:
	./test/run_qemu.sh

.PHONY: all clean test
