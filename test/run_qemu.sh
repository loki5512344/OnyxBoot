#!/bin/sh
set -e

CROSS="${CROSS:-riscv64-elf}"
TEST_DIR="$(dirname "$0")"
TOP_DIR="$(dirname "$TEST_DIR")"

# Build test kernel
echo "==> Building test kernel"
$CROSS-g++ -march=rv64gc -mabi=lp64d -mcmodel=medany -ffreestanding -nostdlib \
    -O2 -Wall -Wextra "$TEST_DIR/kernel.c" -T "$TEST_DIR/kernel.ld" -nostdlib \
    -o "$TEST_DIR/kernel.elf"
$CROSS-objcopy -O binary "$TEST_DIR/kernel.elf" "$TEST_DIR/kernel.bin"

# Build bootloader
echo "==> Building bootloader"
make -C "$TOP_DIR" CROSS="$CROSS" clean all

# Create disk image
echo "==> Creating disk image"
dd if=/dev/zero of="$TEST_DIR/test.img" bs=1M count=4 2>/dev/null
dd if="$TEST_DIR/kernel.elf" of="$TEST_DIR/test.img" bs=512 conv=notrunc 2>/dev/null

# Run QEMU
echo "==> Starting QEMU"
qemu-system-riscv64 -M virt -m 256M \
    -bios "$TOP_DIR/bootloader.bin" \
    -drive file="$TEST_DIR/test.img",format=raw,if=none,id=drive0 \
    -device virtio-blk-device,drive=drive0 \
    -nographic -serial mon:stdio

# QEMU riscv64 virt supports virtio-9p-device for file sharing:
#   -fsdev local,security_model=mapped,id=fsdev0,path=/path/to/share \
#   -device virtio-9p-device,fsdev=fsdev0,mount_tag=hostshare
