#pragma once
#include <cstdint>

static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) |
        ((x & 0x00FF0000) >> 8)  | ((x & 0xFF000000) >> 24);
}

static inline uint32_t be32(const uint32_t* p) { return bswap32(*p); }
static inline uint32_t fdt_rd32(const uint8_t* base, uint32_t off) {
    return bswap32(*(const uint32_t*)(base + off));
}

struct mem_info { uint64_t base; uint64_t size; };
struct uart_info { uint64_t base; uint32_t irq; uint32_t reg_shift; };
struct mmio_dev { uint64_t base; uint32_t irq; };

static inline bool fdt_str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static inline bool fdt_compat_match(const uint8_t* val, uint32_t len, const char* target) {
    const uint8_t* end = val + len;
    while (val < end) {
        if (fdt_str_eq((const char*)val, target)) return true;
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

    int d = ctx->depth;
    ctx->depth++;
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
        uint32_t plen = (len + 3) & ~3;
        ctx->pos = (const uint32_t*)(p->val + plen);
        return 1;
    }
    return 0;
}

static inline void fdt_read_reg(fdt_ctx* ctx, int d, const uint8_t* val, uint64_t* base, uint64_t* size) {
    const uint32_t* rp = (const uint32_t*)val;
    *base = fdt_read_cells(&rp, ctx->addr_cells[d]);
    if (size) *size = fdt_read_cells(&rp, ctx->size_cells[d]);
}
