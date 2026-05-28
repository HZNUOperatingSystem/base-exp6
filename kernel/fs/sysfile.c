//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "defs.h"
#include "fcntl.h"
#include "file.h"
#include "fs.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

#define O_CREATE 0x200
#define O_TRUNC 0x400

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int* pfd, struct file** pf) {
    int fd;
    struct file* f;

    argint(n, &fd);
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc(struct file* f) {
    int fd;
    struct proc* p = myproc();

    for (fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

uint64 sys_dup(void) {
    struct file* f;
    int fd;

    if (argfd(0, 0, &f) < 0)
        return -1;
    if ((fd = fdalloc(f)) < 0)
        return -1;
    filedup(f);
    return fd;
}

uint64 sys_read(void) {
    struct file* f;
    int n;
    uint64 p;

    argaddr(1, &p);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0)
        return -1;
    return fileread(f, p, n);
}

uint64 sys_write(void) {
    struct file* f;
    int n;
    uint64 p;

    argaddr(1, &p);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0)
        return -1;

    return filewrite(f, p, n);
}

uint64 sys_close(void) {
    int fd;
    struct file* f;

    if (argfd(0, &fd, &f) < 0)
        return -1;
    myproc()->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

uint64 sys_filemap(void) {
    struct file* f;
    uint64 len;

    if (argfd(0, 0, &f) < 0)
        return -1;
    argaddr(1, &len);
    // Lab 3 hint: reserve a virtual range and remember which file should
    // supply page contents when vmfault reaches that range.
    return -1;
}

uint64 sys_fstat(void) {
    struct file* f;
    uint64 st; // user pointer to struct stat

    argaddr(1, &st);
    if (argfd(0, 0, &f) < 0)
        return -1;
    return filestat(f, st);
}

uint64 sys_open(void) {
    char path[MAXPATH];
    int fd, omode;
    struct file* f;
    struct inode* ip;
    int n;

    argint(1, &omode);
    if ((n = argstr(0, path, MAXPATH)) < 0)
        return -1;

    if (omode & (O_CREATE | O_TRUNC))
        return -1;

    if ((ip = namei(path)) == 0)
        return -1;
    ilock(ip);
    if (ip->type == T_DIR && omode != O_RDONLY) {
        iunlockput(ip);
        return -1;
    }

    if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
        iunlockput(ip);
        return -1;
    }

    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        iunlockput(ip);
        return -1;
    }

    if (ip->type == T_DEVICE) {
        f->type = FD_DEVICE;
        f->major = ip->major;
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable =
        ip->type == T_DEVICE && ((omode & O_WRONLY) || (omode & O_RDWR));

    iunlock(ip);

    return fd;
}

uint64 sys_exec(void) {
    char path[MAXPATH], *argv[MAXARG];
    int i;
    uint64 uargv, uarg;

    argaddr(1, &uargv);
    if (argstr(0, path, MAXPATH) < 0) {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (i = 0;; i++) {
        if (i >= NELEM(argv)) {
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(uint64) * i, (uint64*)&uarg) < 0) {
            goto bad;
        }
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == 0)
            goto bad;
        if (fetchstr(uarg, argv[i], PGSIZE) < 0)
            goto bad;
    }

    int ret = kexec(path, argv);

    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);

    return ret;

bad:
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);
    return -1;
}
