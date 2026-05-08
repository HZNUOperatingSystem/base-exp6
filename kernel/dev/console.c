//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "defs.h"
#include "file.h"
#include "fs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

#define BACKSPACE 0x100  // erase the last output character
#define C(x) ((x) - '@') // Control-x

//
// send one character to the uart, but don't use
// interrupts or sleep(). safe to be called from
// interrupts, e.g. by printf and to echo input
// characters.
//
void consputc(int c) {
    if (c == BACKSPACE) {
        // if the user typed backspace, overwrite with a space.
        uartputc_sync('\b');
        uartputc_sync(' ');
        uartputc_sync('\b');
    } else {
        uartputc_sync(c);
    }
}

struct {
    struct spinlock lock;

    // input circular buffer
#define INPUT_BUF_SIZE 128
    char buf[INPUT_BUF_SIZE];
    uint r; // Read index
    uint w; // Write index
    uint e; // Edit index
} cons;

int utf8_char_len(uint end) {
    if (end <= cons.w)
        return 0;

    uint idx = end - 1;
    int n = 1;

    while (idx > cons.w && (cons.buf[idx % INPUT_BUF_SIZE] & 0xC0) == 0x80) {
        idx--;
        n++;
    }
    return n;
}

uint utf8_decode(uint end, int len) {
    unsigned char c0 = cons.buf[(end - len) % INPUT_BUF_SIZE];

    if (len == 1)
        return c0;

    uint r = 0;
    if (len == 2)
        r = c0 & 0x1F;
    else if (len == 3)
        r = c0 & 0x0F;
    else if (len == 4)
        r = c0 & 0x07;
    else
        return c0;

    for (int i = 1; i < len; i++) {
        unsigned char c = cons.buf[(end - len + i) % INPUT_BUF_SIZE];
        if ((c & 0xC0) != 0x80)
            return c0;
        r = (r << 6) | (c & 0x3F);
    }
    return r;
}

int utf8_display_width(uint rune) {
    if (rune < 0x80)
        return 1;
    if ((rune >= 0x1100 && rune <= 0x115F) ||
        (rune >= 0x2329 && rune <= 0x232A) ||
        (rune >= 0x2E80 && rune <= 0xA4CF) ||
        (rune >= 0xAC00 && rune <= 0xD7A3) ||
        (rune >= 0xF900 && rune <= 0xFAFF) ||
        (rune >= 0xFE10 && rune <= 0xFE19) ||
        (rune >= 0xFE30 && rune <= 0xFE6F) ||
        (rune >= 0xFF00 && rune <= 0xFF60) ||
        (rune >= 0xFFE0 && rune <= 0xFFE6) ||
        (rune >= 0x20000 && rune <= 0x3FFFD))
        return 2;
    return 1;
}

//
// user write() system calls to the console go here.
// uses sleep() and UART interrupts.
//
int consolewrite(int user_src, uint64 src, int n) {
    char buf[32]; // move batches from user space to uart.
    int i = 0;

    while (i < n) {
        int nn = sizeof(buf);
        if (nn > n - i)
            nn = n - i;
        if (either_copyin(buf, user_src, src + i, nn) == -1)
            break;
        uartwrite(buf, nn);
        i += nn;
    }

    return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dst indicates whether dst is a user
// or kernel address.
//
int consoleread(int user_dst, uint64 dst, int n) {
    uint target;
    int c;
    char cbuf;

    target = n;
    acquire(&cons.lock);
    while (n > 0) {
        // wait until interrupt handler has put some
        // input into cons.buffer.
        while (cons.r == cons.w) {
            if (killed(myproc())) {
                release(&cons.lock);
                return -1;
            }
            sleep(&cons.r, &cons.lock);
        }

        c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

        if (c == C('D')) { // end-of-file
            if (n < target) {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                cons.r--;
            }
            break;
        }

        // copy the input byte to the user-space buffer.
        cbuf = c;
        if (either_copyout(user_dst, dst, &cbuf, 1) == -1)
            break;

        dst++;
        --n;

        if (c == '\n') {
            // a whole line has arrived, return to
            // the user-level read().
            break;
        }
    }
    release(&cons.lock);

    return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for each input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void consoleintr(int c) {
    acquire(&cons.lock);

    switch (c) {
    case C('P'): // Print process list.
        procdump();
        break;
    case C('U'): // Kill line.
        while (cons.e != cons.w &&
               cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
            int len = utf8_char_len(cons.e);
            int width = utf8_display_width(utf8_decode(cons.e, len));

            cons.e -= len;
            while (width-- > 0) {
                consputc(BACKSPACE);
            }
        }
        break;
    case C('H'): // Backspace
    case '\x7f': // Delete key
        if (cons.e != cons.w) {
            int len = utf8_char_len(cons.e);
            int width = utf8_display_width(utf8_decode(cons.e, len));

            cons.e -= len;
            while (width-- > 0) {
                consputc(BACKSPACE);
            }
        }
        break;
    default:
        if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
            c = (c == '\r') ? '\n' : c;

            // echo back to the user.
            consputc(c);

            // store for consumption by consoleread().
            cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

            if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
                // wake up consoleread() if a whole line (or end-of-file)
                // has arrived.
                cons.w = cons.e;
                wakeup(&cons.r);
            }
        }
        break;
    }

    release(&cons.lock);
}

void consoleinit(void) {
    initlock(&cons.lock, "cons");

    uartinit();

    // connect read and write system calls
    // to consoleread and consolewrite.
    devsw[CONSOLE].read = consoleread;
    devsw[CONSOLE].write = consolewrite;
}
