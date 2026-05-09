#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define stat xv6_stat // avoid clash with host struct stat
#include "file.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "types.h"

#ifndef static_assert
#define static_assert(a, b)                                                    \
    do {                                                                       \
        switch (0)                                                             \
        case 0:                                                                \
        case (a):;                                                             \
    } while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / BPB + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGBLOCKS + 1; // header_u followed by LOGBLOCKS data blocks.
int nmeta;   // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks; // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode* ip);
void rsect(uint sec, void* buf);
uint ialloc(ushort type, ushort major, ushort minor);
uint allocblock(void);
void iappend(uint inum, void* p, int n);
void die(const char*);

// convert to riscv byte order
ushort xshort(ushort x) {
    ushort y;
    uchar* a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x) {
    uint y;
    uchar* a = (uchar*)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

int main(int argc, char* argv[]) {
    int i, cc, fd;
    uint rootino, inum, off;
    struct dirent de;
    char buf[BSIZE];
    struct dinode din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BSIZE % sizeof(struct dinode)) == 0);
    assert((BSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0)
        die(argv[1]);

    // 1 fs block = 1 disk sector
    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;

    sb.magic = FSMAGIC;
    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);

    // printf("nmeta %d (boot, super, log blocks %u, inode blocks %u, bitmap
    // blocks %u) blocks %d total %d\n",
    //        nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

    freeblock = nmeta; // the first free block that we can allocate

    for (i = 0; i < FSSIZE; i++)
        wsect(i, zeroes);

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    rootino = ialloc(T_DIR, 0, 0);
    assert(rootino == ROOTINO);

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    inum = ialloc(T_DEVICE, CONSOLE, 0);
    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, "console", DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    for (i = 2; i < argc; i++) {
        char* shortname = strrchr(argv[i], '/');
        shortname = shortname ? shortname + 1 : argv[i];

        if ((fd = open(argv[i], 0)) < 0)
            die(argv[i]);

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (shortname[0] == '_')
            shortname += 1;

        assert(strlen(shortname) <= DIRSIZ);

        inum = ialloc(T_FILE, 0, 0);

        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, shortname, DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        while ((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);

        close(fd);
    }

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off / BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock);

    exit(0);
}

void wsect(uint sec, void* buf) {
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
        die("lseek");
    if (write(fsfd, buf, BSIZE) != BSIZE)
        die("write");
}

void winode(uint inum, struct dinode* ip) {
    char buf[BSIZE];
    uint bn;
    struct dinode* dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode*)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

void rinode(uint inum, struct dinode* ip) {
    char buf[BSIZE];
    uint bn;
    struct dinode* dip;

    bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    dip = ((struct dinode*)buf) + (inum % IPB);
    *ip = *dip;
}

void rsect(uint sec, void* buf) {
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
        die("lseek");
    if (read(fsfd, buf, BSIZE) != BSIZE)
        die("read");
}

uint ialloc(ushort type, ushort major, ushort minor) {
    uint inum = freeinode++;
    struct dinode din;

    bzero(&din, sizeof(din));
    din.type = xshort(type);
    din.major = xshort(major);
    din.minor = xshort(minor);
    din.nlink = xshort(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

uint allocblock(void) {
    if (freeblock >= FSSIZE) {
        fprintf(stderr, "mkfs: out of blocks\n");
        exit(1);
    }
    return freeblock++;
}

void balloc(int used) {
    uchar buf[BSIZE];
    int i, b;

    for (b = 0; b < nbitmap; b++) {
        bzero(buf, BSIZE);
        for (i = 0; i < BPB; i++) {
            int block = b * BPB + i;
            if (block < used)
                buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
        }
        wsect(sb.bmapstart + b, buf);
    }
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void* xp, int n) {
    char* p = (char*)xp;
    uint fbn, off, n1;
    struct dinode din;
    char buf[BSIZE];
    uint indirect[NINDIRECT];
    uint indirect2[NINDIRECT];
    uint x;

    rinode(inum, &din);
    off = xint(din.size);
    // printf("append inum %d at off %d sz %d\n", inum, off, n);
    while (n > 0) {
        fbn = off / BSIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(allocblock());
            }
            x = xint(din.addrs[fbn]);
        } else if (fbn < NDIRECT + NINDIRECT) {
            uint inbn = fbn - NDIRECT;

            if (xint(din.addrs[NDIRECT]) == 0) {
                din.addrs[NDIRECT] = xint(allocblock());
            }
            rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            if (indirect[inbn] == 0) {
                indirect[inbn] = xint(allocblock());
                wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
            }
            x = xint(indirect[inbn]);
        } else {
            uint dbn = fbn - NDIRECT - NINDIRECT;
            uint i1 = dbn / NINDIRECT;
            uint i2 = dbn % NINDIRECT;

            if (xint(din.addrs[NDIRECT + 1]) == 0) {
                din.addrs[NDIRECT + 1] = xint(allocblock());
            }
            rsect(xint(din.addrs[NDIRECT + 1]), (char*)indirect);
            if (indirect[i1] == 0) {
                indirect[i1] = xint(allocblock());
                wsect(xint(din.addrs[NDIRECT + 1]), (char*)indirect);
            }
            rsect(xint(indirect[i1]), (char*)indirect2);
            if (indirect2[i2] == 0) {
                indirect2[i2] = xint(allocblock());
                wsect(xint(indirect[i1]), (char*)indirect2);
            }
            x = xint(indirect2[i2]);
        }
        n1 = min(n, (fbn + 1) * BSIZE - off);
        rsect(x, buf);
        bcopy(p, buf + off - (fbn * BSIZE), n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}

void die(const char* s) {
    perror(s);
    exit(1);
}
