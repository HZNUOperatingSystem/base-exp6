#include "param.h"
#include "stat.h"
#include "types.h"
#include "user.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long align_t;

union header {
    struct {
        union header* ptr;
        uint size;
    } s;
    align_t x;
};

typedef union header header_u;

static header_u base;
static header_u* freep;

void free(void* ap) {
    header_u *bp, *p;

    bp = (header_u*)ap - 1;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    if (bp + bp->s.size == p->s.ptr) {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else
        bp->s.ptr = p->s.ptr;
    if (p + p->s.size == bp) {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else
        p->s.ptr = bp;
    freep = p;
}

static header_u* morecore(uint nu) {
    char* p;
    header_u* hp;

    if (nu < 4096)
        nu = 4096;
    p = sbrk(nu * sizeof(header_u));
    if (p == SBRK_ERROR)
        return 0;
    hp = (header_u*)p;
    hp->s.size = nu;
    free((void*)(hp + 1));
    return freep;
}

void* malloc(uint nbytes) {
    header_u *p, *prevp;
    uint nunits;

    nunits = (nbytes + sizeof(header_u) - 1) / sizeof(header_u) + 1;
    if ((prevp = freep) == 0) {
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }
    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr) {
        if (p->s.size >= nunits) {
            if (p->s.size == nunits)
                prevp->s.ptr = p->s.ptr;
            else {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void*)(p + 1);
        }
        if (p == freep)
            if ((p = morecore(nunits)) == 0)
                return 0;
    }
}
