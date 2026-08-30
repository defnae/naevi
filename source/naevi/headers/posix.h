// source/naevi/headers/posix.h

#ifndef POSIX_H
#define POSIX_H

typedef __INT32_TYPE__ sig_atomic_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __INT64_TYPE__ off_t;

#ifdef __linux__
typedef __SIZE_TYPE__ nfds_t;
#else
typedef __UINT32_TYPE__ nfds_t;
#endif

typedef __UINT16_TYPE__ ws_row;
typedef __UINT16_TYPE__ ws_col;

typedef __UINT32_TYPE__ tcflag_t;
typedef __UINT8_TYPE__ cc_t;
typedef __UINT32_TYPE__ speed_t;

#ifdef __linux__
    typedef __INT32_TYPE__ sa_flags_t;

    typedef struct {
        unsigned long __val[128 / sizeof(unsigned long)];
    } sigset_t;

#elif defined(__APPLE__)
    typedef __INT32_TYPE__ sa_flags_t;
    typedef __UINT32_TYPE__ sigset_t;

#elif defined(__MSYS__) || defined(__CYGWIN__)
    typedef __SIZE_TYPE__ sa_flags_t;
    typedef __SIZE_TYPE__ sigset_t;

#else
    typedef __SIZE_TYPE__ sa_flags_t;
    typedef __SIZE_TYPE__ sigset_t;
#endif

struct stat;
struct pollfd;
struct winsize;
struct sigaction;
#if defined(__APPLE__)
struct __siginfo;
#endif

#if defined(__linux__)
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 48
#elif defined(__APPLE__)
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 96
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define STAT_BUFFER_SIZE 128
#define STAT_SIZE_OFFSET 40
#endif

#if defined(__linux__)
#define NCCS 32
#elif defined(__APPLE__)
#define NCCS 20
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define NCCS 18
#endif

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
#if defined(__linux__) || defined(__CYGWIN__) || defined(__MSYS__)
    char c_line;
#endif
    cc_t c_cc[NCCS];
#if defined(__linux__)
    char c_reserved_pad[3];
#elif defined(__CYGWIN__) || defined(__MSYS__)
    char c_reserved_pad[1];
#endif
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#if defined(__linux__)
struct sigaction {
	void (*sa_handler) (__INT32_TYPE__);

	sigset_t sa_mask;

	__INT32_TYPE__ sa_flags;

#if __SIZEOF_POINTER__ == 8
	__INT32_TYPE__ sa_reserved_pad;
#endif

	void (*sa_restorer) (void);
};

#elif defined(__APPLE__)
struct sigaction {
    union {
        void (*sa_handler) (__INT32_TYPE__);

        void (*sa_sigaction) (__INT32_TYPE__, struct __siginfo *, void *);
    } __sigaction_u;

    sigset_t sa_mask;
    __INT32_TYPE__ sa_flags;
};

#elif defined(__CYGWIN__) || defined(__MSYS__)
struct sigaction {
    void (*sa_handler) (__INT32_TYPE__);

    sigset_t sa_mask;

    __INT32_TYPE__ sa_flags;

    __UINT8_TYPE__ sa_reserved_pad[4];
};

#endif

#define SA_RESTART 0x10000000

struct winsize {
    __UINT16_TYPE__ ws_row;
    __UINT16_TYPE__ ws_col;
    __UINT16_TYPE__ ws_xpixel;
    __UINT16_TYPE__ ws_ypixel;
};

struct pollfd {
    __INT32_TYPE__ fd;
    __INT16_TYPE__ events;
    __INT16_TYPE__ revents;
};

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001

#if defined(__linux__)
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#elif defined(__APPLE__) || defined(__CYGWIN__) || defined(__MSYS__)
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#endif

#define TCSANOW 0

#if defined(__linux__)
#define ICANON 0x0002
#define ECHO 0x0008
#define ISIG 0x0001
#elif defined(__APPLE__)
#define ICANON 0x00000100
#define ECHO 0x00000008
#define ISIG 0x00000080
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define ICANON 0x0002
#define ECHO 0x0004
#define ISIG 0x0001
#endif

#if defined(__linux__)
#define VMIN 6
#define VTIME 5
#elif defined(__APPLE__)
#define VMIN 16
#define VTIME 17
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define VMIN 9
#define VTIME 16
#endif

#define SIGINT 2
#define SIGTERM 15
#define SIGWINCH 28

#if defined(__linux__)
#define TIOCGWINSZ 0x5413
#elif defined(__APPLE__)
#define TIOCGWINSZ 0x40087468
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define TIOCGWINSZ 0x5401
#endif

#define POLLIN 0x01

open(const char *, __INT32_TYPE__, ...);
close(__INT32_TYPE__);

ssize_t read(__INT32_TYPE__, void *, __SIZE_TYPE__);
ssize_t write(__INT32_TYPE__, const void *, __SIZE_TYPE__);

fstat(__INT32_TYPE__, struct stat *);

tcgetattr(__INT32_TYPE__, struct termios *);
tcsetattr(__INT32_TYPE__, __INT32_TYPE__, const struct termios *);

ioctl(__INT32_TYPE__, __INT32_TYPE__, ...);
poll(struct pollfd [], nfds_t, __INT32_TYPE__);

void (*signal (__INT32_TYPE__, void (*) (__INT32_TYPE__))) (__INT32_TYPE__);
sigaction(__INT32_TYPE__, const struct sigaction *__restrict__, struct sigaction *__restrict__);
sigemptyset(sigset_t *);

#endif /* POSIX_H */
