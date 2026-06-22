#!/bin/sh
set -e

TEST_DIR="$(dirname "$0")"
TOP_DIR="$(dirname "$TEST_DIR")"

if command -v genimage >/dev/null 2>&1; then
    genimage --config "$TEST_DIR/genimage.cfg" \
             --rootpath / \
             --tmppath /tmp/genimage-onyxboot \
             --inputpath "$TEST_DIR" \
             --outputpath "$TEST_DIR"
else
    echo "genimage not found, falling back to dd"
    dd if=/dev/zero of="$TEST_DIR/test.img" bs=1M count=4 2>/dev/null
    dd if="$TEST_DIR/kernel.elf" of="$TEST_DIR/test.img" bs=512 conv=notrunc 2>/dev/null
fi
