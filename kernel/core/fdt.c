#include "defs.h"
#include "memlayout.h"
#include "types.h"

#define FDT_MAGIC 0xd00dfeedU

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

// FDT is big-endian; convert to host (little-endian RISC-V).
static uint32 bswap32(uint32 x) {
    return ((x & 0xffU) << 24) | ((x & 0xff00U) << 8) | ((x & 0xff0000U) >> 8) |
           ((x & 0xff000000U) >> 24);
}

uint64 fdt_get_memory_size(uint64 dtb_addr) {
    uint32* hdr = (uint32*)dtb_addr;
    uint32 off_struct, size_struct, off_strings;
    uint32 *sp, *ep, *strings;

    if (dtb_addr == 0 || bswap32(hdr[0]) != FDT_MAGIC)
        return 0;

    // FDT header (all big-endian):
    //  +0: magic  +4: totalsize  +8: off_struct  +12: off_strings
    // +16: off_rsvmap  +20: version  +24: last_comp_ver
    // +28: boot_cpuid  +32: size_strings  +36: size_struct
    off_struct = bswap32(hdr[2]);
    size_struct = bswap32(hdr[9]);
    off_strings = bswap32(hdr[3]);

    sp = (uint32*)(dtb_addr + off_struct);
    ep = sp + size_struct / 4;
    strings = (uint32*)(dtb_addr + off_strings);

    while (sp < ep) {
        uint32 token = bswap32(*sp++);

        switch (token) {
        case FDT_BEGIN_NODE:
            // skip node name (null-terminated, 4-byte aligned)
            while (*(char*)sp)
                sp = (uint32*)((char*)sp + 1);
            sp = (uint32*)((char*)sp + 1);
            sp = (uint32*)(((uint64)sp + 3) & ~(uint64)3);
            break;

        case FDT_END_NODE:
            break;

        case FDT_PROP: {
            uint32 len = bswap32(*sp++);
            uint32 nameoff = bswap32(*sp++);
            char* name = (char*)strings + nameoff;

            // On 64-bit RISC-V, #address-cells=2 and #size-cells=2,
            // so 'reg' is two 64-bit big-endian values: base, size.
            if (len >= 16 && name[0] == 'r' && name[1] == 'e' &&
                name[2] == 'g' && name[3] == 0) {
                uint64 base = ((uint64)bswap32(sp[0]) << 32) | bswap32(sp[1]);
                if (base == KERNBASE) {
                    uint64 size =
                        ((uint64)bswap32(sp[2]) << 32) | bswap32(sp[3]);
                    if (size > 0)
                        return KERNBASE + size;
                }
            }

            // skip property value (len bytes, 4-byte aligned)
            sp = (uint32*)((uint64)sp + ((len + 3) & ~(uint64)3));
            break;
        }

        case FDT_NOP:
            break;

        case FDT_END:
            return 0;

        default:
            return 0;
        }
    }
    return 0;
}
