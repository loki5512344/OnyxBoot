#pragma once
#include <cstdint>

struct __attribute__((packed)) elf64_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct __attribute__((packed)) elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

static inline void elf_memcpy(uint8_t* dst, const uint8_t* src, uint64_t n) {
    for (uint64_t i = 0; i < n; i++)
        dst[i] = src[i];
}

static inline void elf_memset(uint8_t* dst, uint8_t c, uint64_t n) {
    for (uint64_t i = 0; i < n; i++)
        dst[i] = c;
}

struct ELF64 {
    const elf64_ehdr* ehdr;
    bool valid_elf;
    uint64_t entry_point;

    ELF64(const void* d) : ehdr((const elf64_ehdr*)d) {
        const uint8_t* id = (const uint8_t*)d;
        valid_elf = false;
        entry_point = 0;
        if (id[0] != 0x7F || id[1] != 'E' || id[2] != 'L' || id[3] != 'F')
            return;
        if (ehdr->e_type != 2 || ehdr->e_machine != 0xF3)
            return;
        entry_point = ehdr->e_entry;
        valid_elf = true;
    }

    bool valid() const { return valid_elf; }
    uint64_t entry() const { return entry_point; }

    void load_all() const {
        auto* phdr = (const elf64_phdr*)((const uint8_t*)ehdr + ehdr->e_phoff);
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type != 1)
                continue;
            uint8_t* dst = (uint8_t*)(uintptr_t)phdr[i].p_paddr;
            const uint8_t* src = (const uint8_t*)ehdr + phdr[i].p_offset;
            elf_memcpy(dst, src, phdr[i].p_filesz);
            if (phdr[i].p_memsz > phdr[i].p_filesz)
                elf_memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
    }
};
