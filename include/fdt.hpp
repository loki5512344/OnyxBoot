#pragma once
#include <cstdint>

struct mem_info { uint64_t base; uint64_t size; };
struct uart_info { uint64_t base; uint32_t irq; uint32_t reg_shift; };
struct mmio_dev { uint64_t base; uint32_t irq; };

static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}
static inline uint32_t be32(const uint32_t* p) { return bswap32(*p); }
static inline uint32_t fdt_rd32(const uint8_t* b, uint32_t o) {
    return bswap32(*(const uint32_t*)(b + o));
}
static inline bool fdt_str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static inline bool fdt_compat_match(const uint8_t* val, uint32_t len, const char* t) {
    const uint8_t* end = val + len;
    while (val < end) {
        if (fdt_str_eq((const char*)val, t)) return true;
        while (val < end && *val) val++;
        val++;
    }
    return false;
}
static inline uint64_t fdt_read_cells(const uint32_t** pp, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) v = (v << 32) | be32((*pp)++);
    return v;
}
struct fdt_ctx {
    const uint32_t* pos;
    const uint32_t* end;
    const char* strings;
    int depth;
    int addr_cells[16];
    int size_cells[16];
};

static inline int fdt_init(const void* blob, fdt_ctx* ctx) {
    if (!blob) return -1;
    const uint8_t* b = (const uint8_t*)blob;
    if (fdt_rd32(b, 0) != 0xD00DFEED) return -1;
    if (fdt_rd32(b, 20) < 16) return -1;
    ctx->pos = (const uint32_t*)(b + fdt_rd32(b, 8));
    ctx->end = (const uint32_t*)(b + fdt_rd32(b, 8) + fdt_rd32(b, 36));
    ctx->strings = (const char*)(b + fdt_rd32(b, 12));
    ctx->depth = 0;
    ctx->addr_cells[0] = 2;
    ctx->size_cells[0] = 2;
    return 0;
}
static inline const char* fdt_begin_node(fdt_ctx* ctx) {
    uint32_t tok;
    do {
        if (ctx->pos >= ctx->end) return (const char*)-1;
        tok = be32(ctx->pos++);
        if (tok == 9) return (const char*)-1;
        if (tok == 4) continue;
        if (tok != 1) { ctx->depth--; continue; }
        break;
    } while (1);
    const char* name = (const char*)ctx->pos;
    uint32_t slen = 0;
    while (name[slen]) slen++;
    ctx->pos = (const uint32_t*)((const uint8_t*)ctx->pos + ((slen + 4) & ~3));
    int d = ctx->depth++;
    ctx->addr_cells[ctx->depth] = ctx->addr_cells[d];
    ctx->size_cells[ctx->depth] = ctx->size_cells[d];
    return name;
}

struct fdt_prop {
    const char* name;
    const uint8_t* val;
    uint32_t len;
};

static inline int fdt_next_prop(fdt_ctx* ctx, fdt_prop* p) {
    while (ctx->pos < ctx->end) {
        uint32_t t = be32(ctx->pos);
        if (t == 1 || t == 2) return 0;
        if (t == 9) return -1;
        if (t == 4) { ctx->pos++; continue; }
        if (t != 3) break;
        ctx->pos++;
        uint32_t len = be32(ctx->pos++);
        uint32_t nameoff = be32(ctx->pos++);
        p->val = (const uint8_t*)ctx->pos;
        p->len = len;
        p->name = ctx->strings + nameoff;
        if (fdt_str_eq(p->name, "#address-cells") && len == 4)
            ctx->addr_cells[ctx->depth] = (int)be32((const uint32_t*)p->val);
        if (fdt_str_eq(p->name, "#size-cells") && len == 4)
            ctx->size_cells[ctx->depth] = (int)be32((const uint32_t*)p->val);
        ctx->pos = (const uint32_t*)(p->val + ((len + 3) & ~3));
        return 1;
    }
    return 0;
}

static inline void fdt_read_reg(fdt_ctx* ctx, int d, const uint8_t* val, uint64_t* base, uint64_t* size) {
    const uint32_t* rp = (const uint32_t*)val;
    *base = fdt_read_cells(&rp, ctx->addr_cells[d]);
    if (size) *size = fdt_read_cells(&rp, ctx->size_cells[d]);
}
static inline mem_info fdt_find_memory(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return {0x80000000ULL, 128ULL * 1024 * 1024};
    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1) {
        int d = ctx.depth - 1, is_mem = 0, have_reg = 0;
        uint64_t base = 0, size = 0;
        fdt_prop p;
        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) return {0x80000000ULL, 128ULL * 1024 * 1024};
            if (fdt_str_eq(p.name, "device_type") && fdt_str_eq((const char*)p.val, "memory"))
                is_mem = 1;
            else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, &size);
                have_reg = 1;
            }
        }
        if (is_mem && have_reg) return {base, size};
    }
    return {0x80000000ULL, 128ULL * 1024 * 1024};
}
static inline uart_info fdt_find_uart(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return {0x10000000ULL, 10, 0};
    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1) {
        int d = ctx.depth - 1, is_uart = 0, have_reg = 0;
        uint64_t base = 0;
        uint32_t irq = 10, reg_shift = 0;
        fdt_prop p;
        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) return {0x10000000ULL, 10, 0};
            if (fdt_str_eq(p.name, "compatible")) {
                if (fdt_compat_match(p.val, p.len, "ns16550a")) is_uart = 1;
            } else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, (uint64_t*)0);
                have_reg = 1;
            } else if (fdt_str_eq(p.name, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)p.val);
            } else if (fdt_str_eq(p.name, "reg-shift") && p.len == 4) {
                reg_shift = be32((const uint32_t*)p.val);
            }
        }
        if (is_uart && have_reg) return {base, irq, reg_shift};
    }
    return {0x10000000ULL, 10, 0};
}
static inline int fdt_find_mmio(const void* blob, mmio_dev* out, int max, const char* compat) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) return 0;
    int found = 0;
    const char* nn;
    while ((nn = fdt_begin_node(&ctx)) != (const char*)-1 && found < max) {
        int d = ctx.depth - 1, match = 0, have_reg = 0;
        uint64_t base = 0;
        uint32_t irq = 0;
        fdt_prop p;
        while (int r = fdt_next_prop(&ctx, &p)) {
            if (r < 0) return found;
            if (fdt_str_eq(p.name, "compatible")) {
                if (fdt_compat_match(p.val, p.len, compat)) match = 1;
            } else if (fdt_str_eq(p.name, "reg")) {
                fdt_read_reg(&ctx, d, p.val, &base, (uint64_t*)0);
                have_reg = 1;
            } else if (fdt_str_eq(p.name, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)p.val);
            }
        }
        if (match && have_reg) { out[found].base = base; out[found].irq = irq; found++; }
    }
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
    int n = fdt_find_mmio(blob, out, max, "snps,dw-mshc");
    n += fdt_find_mmio(blob, out + n, max - n, "sophgo,sg2000-sdhci");
    n += fdt_find_mmio(blob, out + n, max - n, "generic-sdhci");
    return n;
}
