// source/naevi/headers/posix.h

#ifndef POSIX_H
#define POSIX_H

typedef __UINT16_TYPE__ ws_row;
typedef __UINT16_TYPE__ ws_col;

typedef __PTRDIFF_TYPE__ ssize_t;

typedef __INT32_TYPE__ sig_atomic_t;

#ifdef __linux__
typedef __SIZE_TYPE__ nfds_t;
#else
typedef __UINT32_TYPE__ nfds_t;
#endif

typedef struct termios termios;
typedef struct stat stat;
typedef struct pollfd pollfd;
typedef struct winsize winsize;

ssize_t read(__INT32_TYPE__, void *, __SIZE_TYPE__);
ssize_t write(__INT32_TYPE__, const void *, __SIZE_TYPE__);

open(const char *, __INT32_TYPE__, ...);
close(__INT32_TYPE__);
fstat(__INT32_TYPE__, stat *);

tcgetattr(__INT32_TYPE__, termios *);
tcsetattr(__INT32_TYPE__, __INT32_TYPE__, const termios *);

ioctl(__INT32_TYPE__, unsigned long, ...);
poll(pollfd *, nfds_t, __INT32_TYPE__);

void (*signal(__INT32_TYPE__, void (*)(__INT32_TYPE__)))(__INT32_TYPE__);
siginterrupt(__INT32_TYPE__, __INT32_TYPE__);

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define TCSANOW 0

typedef __INT64_TYPE__ off_t;

#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__) || defined(__i386__) || defined(__arm__))
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#elif defined(__APPLE__) || defined(__CYGWIN__) || defined(__MSYS__)
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#endif

#if defined(__linux__) && defined(__x86_64__) && defined(NAEVI_LIBC_GLIBC)
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 48
#elif defined(__linux__) && defined(__x86_64__) && defined(NAEVI_LIBC_MUSL)
#elif defined(__linux__) && !defined(NAEVI_LIBC_GLIBC) && !defined(NAEVI_LIBC_MUSL)
#elif defined(__APPLE__)
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 96
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define STAT_BUFFER_SIZE 128
#define STAT_SIZE_OFFSET 40
#endif

typedef __UINT32_TYPE__ tcflag_t;
typedef __UINT8_TYPE__ cc_t;
typedef __UINT32_TYPE__ speed_t;

#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__) || defined(__i386__) || defined(__arm__))
#define NCCS 32

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
	char c_reserved_pad[3];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#define ICANON 0x0002
#define ECHO 0x0008
#define ISIG 0x0001
#define VMIN 6
#define VTIME 5

#elif defined(__APPLE__)

#define NCCS 20

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#define ICANON 0x00000100
#define ECHO 0x00000008
#define ISIG 0x00000080
#define VMIN 16
#define VTIME 17

#elif defined(__CYGWIN__) || defined(__MSYS__)

#define NCCS 18

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	char c_line;
	cc_t c_cc[NCCS];
	char c_reserved_pad[1];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#define ICANON 0x0002
#define ECHO 0x0004
#define ISIG 0x0001
#define VMIN 9
#define VTIME 16

#endif

#if defined(__APPLE__)
#define TIOCGWINSZ 0x40087468
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define TIOCGWINSZ 0x5401
#elif defined(__linux__)
#define TIOCGWINSZ 0x5413
#endif

#define SIGWINCH 28

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

#define POLLIN 0x01

#endif /* POSIX_H */
