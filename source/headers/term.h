// headers/term.h

#ifndef TERM_H
#define TERM_H

#ifndef __clang__
#error "Toolchain mismatch: Clang required."
#endif

#include <headers/platform.h>
#include <headers/std/int.h>
#include <headers/std/def.h>
#include <headers/std/attr.h>

/* ── file I/O + raw read/write ───────────────────────────────────────────
 *
 * these are real libc symbols (POSIX), declared by hand so we never
 * #include a system header. every target we support (glibc, musl,
 * macOS libSystem, msys2's runtime) exports these under the same
 * names and the same calling convention, so this is safe.
 */

extern int64_t read(int32_t fd, void *buf, uint64_t count);
extern int64_t write(int32_t fd, const void *buf, uint64_t count);
extern int32_t open(const char *path, int32_t flags, ...);
extern int32_t close(int32_t fd);

#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_CREAT   0x0040 /* linux value; overridden below where it differs */
#define O_TRUNC   0x0200 /* linux value; overridden below where it differs */

#if defined(NAEVI_DARWIN)
/* macOS's <fcntl.h> O_CREAT/O_TRUNC bit positions differ from Linux's. */
#undef O_CREAT
#undef O_TRUNC
#define O_CREAT  0x0200
#define O_TRUNC  0x0400
#endif

/* ── struct stat (only the field we actually use: st_size) ──────────────
 *
 * rather than replicate the entire (and differently-padded) struct stat
 * per platform, we call fstat into a byte buffer sized generously, and
 * read st_size out of it via the platform-specific offset. this avoids
 * getting any *other* field's layout wrong, since we never touch them.
 */

extern int32_t fstat(int32_t fd, void *statbuf);

#if defined(NAEVI_LINUX)
/* verified: offsetof(struct stat, st_size) == 48, sizeof == 144
 * on x86_64 glibc. */
#define STAT_BUF_SIZE    144
#define STAT_SIZE_OFFSET 48
#elif defined(NAEVI_DARWIN)
/* UNVERIFIED: offset taken from documented xnu struct stat64 layout,
 * not tested on real hardware. */
#define STAT_BUF_SIZE    144
#define STAT_SIZE_OFFSET 96
#elif defined(NAEVI_MSYS)
/* UNVERIFIED: offset taken from documented newlib/cygwin struct stat
 * layout, not tested on real hardware. */
#define STAT_BUF_SIZE    96
#define STAT_SIZE_OFFSET 32
#endif

static int64_t file_size(int32_t fd) {
    uint8_t st[STAT_BUF_SIZE];
    if (fstat(fd, st) != 0) return -1;
    return *(int64_t *)(st + STAT_SIZE_OFFSET);
}

/* ── termios ──────────────────────────────────────────────────────────── */

extern int32_t tcgetattr(int32_t fd, void *termios_p);
extern int32_t tcsetattr(int32_t fd, int32_t optional_actions, void *termios_p);

#define TCSANOW 0

#if defined(NAEVI_LINUX)

/* verified: this is glibc's/musl's struct termios (the one
 * tcgetattr/tcsetattr actually read and write), NOT the raw kernel
 * ioctl(TCGETS) ABI struct -- those differ (kernel's is NCCS=19,
 * libc's is NCCS=32) precisely because tcgetattr/tcsetattr internally
 * use the newer TCGETS2/TCSETS2 ioctls and translate between the two.
 * calling the real libc functions (for portability) means matching
 * *their* struct, not the kernel's. confirmed via offsetof() against
 * glibc: sizeof == 60, c_cc starts at 17, c_ispeed at 52, c_ospeed at 56. */
#define NCCS 32
typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[NCCS];
    uint8_t  c_reserved_pad[3]; /* explicit: glibc/musl align c_ispeed to 4 bytes here */
    uint32_t c_ispeed;
    uint32_t c_ospeed;
} termios_t;

#define T_ICANON 0x0002
#define T_ECHO   0x0008
#define T_ISIG   0x0001
#define T_VMIN   6
#define T_VTIME  5

#elif defined(NAEVI_DARWIN)

/* UNVERIFIED: written against documented xnu/BSD <sys/termios.h> layout,
 * not tested on real hardware. */
#define NCCS 20
typedef struct {
    uint64_t c_iflag;
    uint64_t c_oflag;
    uint64_t c_cflag;
    uint64_t c_lflag;
    uint8_t  c_cc[NCCS];
    uint64_t c_ispeed;
    uint64_t c_ospeed;
} termios_t;

#define T_ICANON 0x00000100
#define T_ECHO   0x00000008
#define T_ISIG   0x00000080
#define T_VMIN   16
#define T_VTIME  17

#elif defined(NAEVI_MSYS)

/* UNVERIFIED: written against documented Cygwin/newlib <sys/termios.h>
 * layout (which msys2's runtime derives from), not tested on real
 * hardware. */
#define NCCS 18
typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[NCCS];
    uint32_t c_ispeed;
    uint32_t c_ospeed;
} termios_t;

#define T_ICANON 0x0002
#define T_ECHO   0x0008
#define T_ISIG   0x0001
#define T_VMIN   6
#define T_VTIME  5

#endif

static termios_t g_orig_termios;

static void raw_on(void) {
    termios_t t;
    tcgetattr(0, &g_orig_termios);
    t = g_orig_termios;
    t.c_lflag &= ~(uint32_t)(T_ICANON | T_ECHO | T_ISIG);
    t.c_cc[T_VMIN]  = 1;
    t.c_cc[T_VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
}

static void raw_off(void) {
    tcsetattr(0, TCSANOW, &g_orig_termios);
}

/* ── window size ──────────────────────────────────────────────────────── */

extern int32_t ioctl(int32_t fd, uint64_t request, void *arg);

#if defined(NAEVI_DARWIN)
#define TIOCGWINSZ 0x40087468
#else
/* UNVERIFIED for msys2: written against documented cygwin ioctl
 * numbering, which mirrors the linux value here. */
#define TIOCGWINSZ 0x5413
#endif

typedef struct {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} winsize_t;

static uint16_t g_rows = 24;
static uint16_t g_cols = 80;

static void get_winsize(void) {
    winsize_t ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_row > 0) g_rows = ws.ws_row;
        if (ws.ws_col > 0) g_cols = ws.ws_col;
    }
}

/* ── input readiness (for ESC-sequence disambiguation) ───────────────────
 *
 * a bare ESC keypress and the start of an arrow-key escape sequence
 * (ESC [ A/B/C/D) are indistinguishable from the first byte alone.
 * poll() with a short timeout lets us tell them apart: if no further
 * byte shows up within a few milliseconds, it was a real, standalone
 * ESC and not the start of a sequence. struct pollfd's layout
 * (int fd; short events; short revents;) is standard POSIX and
 * identical on Linux, macOS, and msys2. */

typedef struct {
    int32_t fd;
    int16_t events;
    int16_t revents;
} pollfd_t;

#define POLLIN 0x0001

extern int32_t poll(pollfd_t *fds, uint64_t nfds, int32_t timeout_ms);

static uint8_t input_ready(int32_t timeout_ms) {
    pollfd_t pfd;
    pfd.fd = 0;
    pfd.events = POLLIN;
    pfd.revents = 0;
    return (uint8_t)(poll(&pfd, 1, timeout_ms) > 0);
}

/* ── process exit ────────────────────────────────────────────────────── */

extern NORETURN void exit(int32_t code);

#endif
