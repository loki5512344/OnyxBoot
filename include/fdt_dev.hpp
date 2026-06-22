#pragma once
#include "fdt.hpp"

static inline mem_info fdt_find_memory(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return {0x80000000ULL, 128ULL * 1024 * 1024};

    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1) {
        int d = ctx.depth - 1;
        int is_mem = 0, have_reg = 0;
        uint64_t base = 0, size = 0;
        fdt_prop p;

        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) goto mem_out;
            if (fdt_str_eq(p.name, "device_type")) {
                char dt[8];
                uint32_t ml = p.len < 7 ? p.len : 7;
                for (uint32_t i = 0; i < ml; i++) dt[i] = (char)p.val[i];
                dt[ml] = 0;
                if (fdt_str_eq(dt, "memory")) is_mem = 1;
            } else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, &size);
                have_reg = 1;
            } else if (fdt_str_eq(p.name, "#address-cells") && p.len == 4)
                ctx.addr_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
            else if (fdt_str_eq(p.name, "#size-cells") && p.len == 4)
                ctx.size_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
        }
        if (is_mem && have_reg) return {base, size};
    }
mem_out:
    return {0x80000000ULL, 128ULL * 1024 * 1024};
}

static inline uart_info fdt_find_uart(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return {0x10000000ULL, 10, 0};

    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1) {
        int d = ctx.depth - 1;
        int is_uart = 0, have_reg = 0;
        uint64_t base = 0;
        uint32_t irq = 10;
        uint32_t reg_shift = 0;
        fdt_prop p;

        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) goto uart_out;
            if (fdt_str_eq(p.name, "compatible")) {
                if (fdt_compat_match(p.val, p.len, "ns16550a")) is_uart = 1;
            } else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, (uint64_t*)0);
                have_reg = 1;
            } else if (fdt_str_eq(p.name, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)p.val);
            } else if (fdt_str_eq(p.name, "reg-shift") && p.len == 4) {
                reg_shift = be32((const uint32_t*)p.val);
            } else if (fdt_str_eq(p.name, "#address-cells") && p.len == 4)
                ctx.addr_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
            else if (fdt_str_eq(p.name, "#size-cells") && p.len == 4)
                ctx.size_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
        }
        if (is_uart && have_reg) return {base, irq, reg_shift};
    }
uart_out:
    return {0x10000000ULL, 10, 0};
}

static inline int fdt_find_mmio(const void* blob, mmio_dev* out, int max, const char* compat) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return 0;

    int found = 0;
    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1 && found < max) {
        int d = ctx.depth - 1;
        int match = 0, have_reg = 0;
        uint64_t base = 0;
        uint32_t irq = 0;
        fdt_prop p;

        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) goto mmio_out;
            if (fdt_str_eq(p.name, "compatible")) {
                if (fdt_compat_match(p.val, p.len, compat)) match = 1;
            } else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, (uint64_t*)0);
                have_reg = 1;
            } else if (fdt_str_eq(p.name, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)p.val);
            } else if (fdt_str_eq(p.name, "#address-cells") && p.len == 4)
                ctx.addr_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
            else if (fdt_str_eq(p.name, "#size-cells") && p.len == 4)
                ctx.size_cells[ctx.depth] = (int)be32((const uint32_t*)p.val);
        }
        if (match && have_reg) {
            out[found].base = base;
            out[found].irq = irq;
            found++;
        }
    }
mmio_out:
    return found;
}

static inline const char* fdt_get_model(const void* blob, const char* fallback) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return fallback;
    const char* nn = fdt_begin_node(&ctx);
    if (nn == (const char*)-1) return fallback;
    fdt_prop p;
    while (int r = fdt_next_prop(&ctx, &p)) {
        if (r < 0) return fallback;
        if (fdt_str_eq(p.name, "model")) return (const char*)p.val;
    }
    return fallback;
}

static inline int fdt_find_virtio(const void* blob, mmio_dev* out, int max) {
    return fdt_find_mmio(blob, out, max, "virtio,mmio");
}

static inline int fdt_find_sdhci(const void* blob, mmio_dev* out, int max) {
    return fdt_find_mmio(blob, out, max, "snps,dw-mshc|sophgo,sg2000-sdhci|generic-sdhci");
}
