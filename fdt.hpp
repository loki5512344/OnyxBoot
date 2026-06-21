#pragma once
#include <cstdint>

static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) |
           ((x & 0x00FF0000) >> 8)  | ((x & 0xFF000000) >> 24);
}

static inline uint32_t be32(const uint32_t* p) { return bswap32(*p); }

static inline uint32_t fdt_rd32(const uint8_t* base, uint32_t off) {
    const uint32_t* p = (const uint32_t*)(base + off);
    return bswap32(*p);
}

struct mem_info {
    uint64_t base;
    uint64_t size;
};

struct uart_info {
    uint64_t base;
    uint32_t irq;
};

struct mmio_dev {
    uint64_t base;
    uint32_t irq;
};

static inline bool fdt_str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static inline bool fdt_compat_match(const uint8_t* val, uint32_t len, const char* target) {
    const uint8_t* end = val + len;
    while (val < end) {
        if (fdt_str_eq((const char*)val, target))
            return true;
        while (val < end && *val)
            val++;
        val++;
    }
    return false;
}

static inline uint64_t fdt_read_cells(const uint32_t** pp, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++)
        v = (v << 32) | be32((*pp)++);
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
    if (!blob)
        return -1;
    const uint8_t* b = (const uint8_t*)blob;
    if (fdt_rd32(b, 0) != 0xD00DFEED)
        return -1;
    uint32_t ver = fdt_rd32(b, 20);
    if (ver < 16)
        return -1;
    uint32_t off_struct = fdt_rd32(b, 8);
    uint32_t sz_struct = fdt_rd32(b, 36);
    uint32_t off_strings = fdt_rd32(b, 12);
    ctx->pos = (const uint32_t*)(b + off_struct);
    ctx->end = (const uint32_t*)(b + off_struct + sz_struct);
    ctx->strings = (const char*)(b + off_strings);
    ctx->depth = 0;
    ctx->addr_cells[0] = 2;
    ctx->size_cells[0] = 2;
    return 0;
}

static inline mem_info fdt_find_memory(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) {
        mem_info m = {0x80000000ULL, 128ULL * 1024 * 1024};
        return m;
    }

    while (ctx.pos < ctx.end) {
        uint32_t token = be32(ctx.pos++);
        if (token == 9) break;
        if (token == 4) continue;
        if (token != 1) { ctx.depth--; continue; }

        const char* name = (const char*)ctx.pos;
        uint32_t slen = 0;
        while (name[slen]) slen++;
        slen = (slen + 4) & ~3;
        ctx.pos = (const uint32_t*)((const uint8_t*)ctx.pos + slen);

        int d = ctx.depth;
        ctx.depth++;
        ctx.addr_cells[ctx.depth] = ctx.addr_cells[d];
        ctx.size_cells[ctx.depth] = ctx.size_cells[d];

        int is_memory = 0;
        int have_reg = 0;
        uint64_t reg_base = 0;
        uint64_t reg_size = 0;

        while (ctx.pos < ctx.end) {
            uint32_t t = be32(ctx.pos);
            if (t == 1 || t == 2) break;
            if (t == 9) goto mem_done;
            if (t == 4) { ctx.pos++; continue; }
            if (t != 3) break;

            ctx.pos++;
            uint32_t len = be32(ctx.pos++);
            uint32_t nameoff = be32(ctx.pos++);
            const uint8_t* val = (const uint8_t*)ctx.pos;
            uint32_t plen = (len + 3) & ~3;
            ctx.pos = (const uint32_t*)(val + plen);

            const char* pname = ctx.strings + nameoff;
            if (fdt_str_eq(pname, "device_type")) {
                char dt[8];
                uint32_t ml = len < 7 ? len : 7;
                for (uint32_t i = 0; i < ml; i++) dt[i] = (char)val[i];
                dt[ml] = 0;
                if (fdt_str_eq(dt, "memory"))
                    is_memory = 1;
            } else if (fdt_str_eq(pname, "reg")) {
                const uint32_t* rp = (const uint32_t*)val;
                reg_base = fdt_read_cells(&rp, ctx.addr_cells[d]);
                reg_size = fdt_read_cells(&rp, ctx.size_cells[d]);
                have_reg = 1;
            } else if (fdt_str_eq(pname, "#address-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.addr_cells[ctx.depth] = (int)be32(rp);
            } else if (fdt_str_eq(pname, "#size-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.size_cells[ctx.depth] = (int)be32(rp);
            }
        }

        if (is_memory && have_reg) {
            mem_info m = {reg_base, reg_size};
            return m;
        }
    }
mem_done:;
    mem_info m = {0x80000000ULL, 128ULL * 1024 * 1024};
    return m;
}

static inline uart_info fdt_find_uart(const void* blob) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0) {
        uart_info u = {0x10000000ULL, 10};
        return u;
    }

    while (ctx.pos < ctx.end) {
        uint32_t token = be32(ctx.pos++);
        if (token == 9) break;
        if (token == 4) continue;
        if (token != 1) { ctx.depth--; continue; }

        const char* name = (const char*)ctx.pos;
        uint32_t slen = 0;
        while (name[slen]) slen++;
        slen = (slen + 4) & ~3;
        ctx.pos = (const uint32_t*)((const uint8_t*)ctx.pos + slen);

        int d = ctx.depth;
        ctx.depth++;
        ctx.addr_cells[ctx.depth] = ctx.addr_cells[d];
        ctx.size_cells[ctx.depth] = ctx.size_cells[d];

        int is_uart = 0;
        int have_reg = 0;
        uint64_t reg_base = 0;
        uint32_t irq = 10;

        while (ctx.pos < ctx.end) {
            uint32_t t = be32(ctx.pos);
            if (t == 1 || t == 2) break;
            if (t == 9) goto uart_done;
            if (t == 4) { ctx.pos++; continue; }
            if (t != 3) break;

            ctx.pos++;
            uint32_t len = be32(ctx.pos++);
            uint32_t nameoff = be32(ctx.pos++);
            const uint8_t* val = (const uint8_t*)ctx.pos;
            uint32_t plen = (len + 3) & ~3;
            ctx.pos = (const uint32_t*)(val + plen);

            const char* pname = ctx.strings + nameoff;
            if (fdt_str_eq(pname, "compatible")) {
                if (fdt_compat_match(val, len, "ns16550a"))
                    is_uart = 1;
            } else if (fdt_str_eq(pname, "reg")) {
                const uint32_t* rp = (const uint32_t*)val;
                reg_base = fdt_read_cells(&rp, ctx.addr_cells[d]);
                have_reg = 1;
            } else if (fdt_str_eq(pname, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)val);
            } else if (fdt_str_eq(pname, "#address-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.addr_cells[ctx.depth] = (int)be32(rp);
            } else if (fdt_str_eq(pname, "#size-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.size_cells[ctx.depth] = (int)be32(rp);
            }
        }

        if (is_uart && have_reg) {
            uart_info u = {reg_base, irq};
            return u;
        }
    }
uart_done:;
    uart_info u = {0x10000000ULL, 10};
    return u;
}

static inline int fdt_find_virtio(const void* blob, mmio_dev* out, int max) {
    fdt_ctx ctx;
    if (fdt_init(blob, &ctx) < 0)
        return 0;

    int found = 0;

    while (ctx.pos < ctx.end && found < max) {
        uint32_t token = be32(ctx.pos++);
        if (token == 9) break;
        if (token == 4) continue;
        if (token != 1) { ctx.depth--; continue; }

        const char* name = (const char*)ctx.pos;
        uint32_t slen = 0;
        while (name[slen]) slen++;
        slen = (slen + 4) & ~3;
        ctx.pos = (const uint32_t*)((const uint8_t*)ctx.pos + slen);

        int d = ctx.depth;
        ctx.depth++;
        ctx.addr_cells[ctx.depth] = ctx.addr_cells[d];
        ctx.size_cells[ctx.depth] = ctx.size_cells[d];

        int is_virtio = 0;
        int have_reg = 0;
        uint64_t reg_base = 0;
        uint32_t irq = 8;

        while (ctx.pos < ctx.end) {
            uint32_t t = be32(ctx.pos);
            if (t == 1 || t == 2) break;
            if (t == 9) goto virtio_done;
            if (t == 4) { ctx.pos++; continue; }
            if (t != 3) break;

            ctx.pos++;
            uint32_t len = be32(ctx.pos++);
            uint32_t nameoff = be32(ctx.pos++);
            const uint8_t* val = (const uint8_t*)ctx.pos;
            uint32_t plen = (len + 3) & ~3;
            ctx.pos = (const uint32_t*)(val + plen);

            const char* pname = ctx.strings + nameoff;
            if (fdt_str_eq(pname, "compatible")) {
                if (fdt_compat_match(val, len, "virtio,mmio"))
                    is_virtio = 1;
            } else if (fdt_str_eq(pname, "reg")) {
                const uint32_t* rp = (const uint32_t*)val;
                reg_base = fdt_read_cells(&rp, ctx.addr_cells[d]);
                have_reg = 1;
            } else if (fdt_str_eq(pname, "interrupts")) {
                irq = (uint32_t)be32((const uint32_t*)val);
            } else if (fdt_str_eq(pname, "#address-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.addr_cells[ctx.depth] = (int)be32(rp);
            } else if (fdt_str_eq(pname, "#size-cells") && len == 4) {
                const uint32_t* rp = (const uint32_t*)val;
                ctx.size_cells[ctx.depth] = (int)be32(rp);
            }
        }

        if (is_virtio && have_reg) {
            out[found].base = reg_base;
            out[found].irq = irq;
            found++;
        }
    }
virtio_done:
    return found;
}
