#pragma once
#include <cstdint>

static const int VIRTIO_QUEUE_SIZE = 8;

struct __attribute__((packed)) virtio_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct __attribute__((packed)) virtio_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_QUEUE_SIZE];
};

struct __attribute__((packed)) virtio_used_elem {
    uint32_t id;
    uint32_t len;
};

struct __attribute__((packed)) virtio_used {
    uint16_t flags;
    uint16_t idx;
    virtio_used_elem ring[VIRTIO_QUEUE_SIZE];
};

struct __attribute__((packed)) virtio_blk_req {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
};

struct VirtIOBlock {
    volatile uint32_t* regs;
    uint64_t phys_base;
    uint16_t last_used_idx;

    alignas(16) virtio_desc desc_tbl[VIRTIO_QUEUE_SIZE];
    virtio_avail avail;
    virtio_used used;

    VirtIOBlock(uint64_t base)
        : regs((volatile uint32_t*)base)
        , phys_base(base)
        , last_used_idx(0) {
        for (int i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
            desc_tbl[i].addr = 0;
            desc_tbl[i].len = 0;
            desc_tbl[i].flags = 0;
            desc_tbl[i].next = 0;
        }
        avail.flags = 0;
        avail.idx = 0;
        for (int i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
            avail.ring[i] = 0;
            used.ring[i].id = 0;
            used.ring[i].len = 0;
        }
        used.flags = 0;
        used.idx = 0;
    }

    void write_reg(uint32_t off, uint32_t val) {
        regs[off / 4] = val;
    }

    uint32_t read_reg(uint32_t off) {
        return regs[off / 4];
    }

    bool probe() {
        if (read_reg(0x000) != 0x74726976)
            return false;
        if (read_reg(0x004) != 2)
            return false;
        if (read_reg(0x008) != 2)
            return false;

        write_reg(0x070, 0);
        write_reg(0x070, 1);
        write_reg(0x070, 2);

        write_reg(0x014, 0);
        uint32_t feat_lo = read_reg(0x010);
        write_reg(0x014, 1);
        uint32_t feat_hi = read_reg(0x010);

        uint32_t drv_lo = feat_lo;
        uint32_t drv_hi = feat_hi | 1;

        write_reg(0x024, 0);
        write_reg(0x020, drv_lo);
        write_reg(0x024, 1);
        write_reg(0x020, drv_hi);

        write_reg(0x070, 8);
        if (!(read_reg(0x070) & 8))
            return false;

        write_reg(0x030, 0);
        uint32_t max = read_reg(0x034);
        if (max < VIRTIO_QUEUE_SIZE)
            return false;
        write_reg(0x038, VIRTIO_QUEUE_SIZE);

        uint64_t desc_pa = (uint64_t)(uintptr_t)desc_tbl;
        uint64_t avail_pa = (uint64_t)(uintptr_t)&avail;
        uint64_t used_pa = (uint64_t)(uintptr_t)&used;

        write_reg(0x080, (uint32_t)(desc_pa));
        write_reg(0x084, (uint32_t)(desc_pa >> 32));
        write_reg(0x090, (uint32_t)(avail_pa));
        write_reg(0x094, (uint32_t)(avail_pa >> 32));
        write_reg(0x0A0, (uint32_t)(used_pa));
        write_reg(0x0A4, (uint32_t)(used_pa >> 32));

        write_reg(0x044, 1);

        write_reg(0x070, 4);

        return true;
    }

    void read_sector(uint64_t lba, void* buf) {
        virtio_blk_req hdr;
        hdr.type = 0;
        hdr.ioprio = 0;
        hdr.sector = lba;
        uint8_t status = 0xFF;

        uint16_t desc_idx = avail.idx % VIRTIO_QUEUE_SIZE;

        desc_tbl[desc_idx].addr = (uint64_t)(uintptr_t)&hdr;
        desc_tbl[desc_idx].len = 16;
        desc_tbl[desc_idx].flags = 1;
        desc_tbl[desc_idx].next = (desc_idx + 1) % VIRTIO_QUEUE_SIZE;

        uint16_t d1 = (desc_idx + 1) % VIRTIO_QUEUE_SIZE;
        desc_tbl[d1].addr = (uint64_t)(uintptr_t)buf;
        desc_tbl[d1].len = 512;
        desc_tbl[d1].flags = 3;
        desc_tbl[d1].next = (desc_idx + 2) % VIRTIO_QUEUE_SIZE;

        uint16_t d2 = (desc_idx + 2) % VIRTIO_QUEUE_SIZE;
        desc_tbl[d2].addr = (uint64_t)(uintptr_t)&status;
        desc_tbl[d2].len = 1;
        desc_tbl[d2].flags = 2;

        __sync_synchronize();

        avail.ring[avail.idx % VIRTIO_QUEUE_SIZE] = desc_idx;
        __sync_synchronize();
        avail.idx++;
        __sync_synchronize();

        write_reg(0x050, 0);

        while (used.idx == last_used_idx)
            __sync_synchronize();
        last_used_idx = used.idx;
    }
};
