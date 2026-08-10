// headers/naevi/naevi.h

#ifndef NAEVI_H
#define NAEVI_H

typedef __PTRDIFF_TYPE__ ssize_t;
typedef long long off_t;

#ifdef __linux__
typedef unsigned long nfds_t;
#else
typedef unsigned int nfds_t;
#endif

typedef struct termios termios;
typedef struct stat stat;
typedef struct pollfd pollfd;
typedef struct winsize winsize;

extern void* malloc(__SIZE_TYPE__);
extern void* realloc(void*, __SIZE_TYPE__);
extern void free(void*);

extern ssize_t read(int, void*, __SIZE_TYPE__);
extern ssize_t write(int, const void*, __SIZE_TYPE__);

extern int open(const char*, int, ...);
extern int close(int);
extern int fstat(int, stat*);

extern int tcgetattr(int, termios*);
extern int tcsetattr(int, int, const termios*);

extern int ioctl(int, unsigned long, ...);
extern int poll(pollfd*, nfds_t, int);

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001

#if defined(__APPLE__) || defined(__CYGWIN__) || defined(__MSYS__)
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#else
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#endif

#ifdef __linux__
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 48
#elif defined(__APPLE__)
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 96
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define STAT_BUFFER_SIZE 128
#define STAT_SIZE_OFFSET 40
#else
#define STAT_BUFFER_SIZE 144
#define STAT_SIZE_OFFSET 48
#endif

#define TCSANOW 0

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#ifdef __linux__

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

#define TERMINAL_FLAG_CANONICAL_MODE 0x0002
#define TERMINAL_FLAG_ECHO 0x0008
#define TERMINAL_FLAG_SIGNAL 0x0001
#define TERMINAL_INDEX_VMIN 6
#define TERMINAL_INDEX_VTIME 5

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

#define TERMINAL_FLAG_CANONICAL_MODE 0x00000100
#define TERMINAL_FLAG_ECHO 0x00000008
#define TERMINAL_FLAG_SIGNAL 0x00000080
#define TERMINAL_INDEX_VMIN 16
#define TERMINAL_INDEX_VTIME 17

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

#define TERMINAL_FLAG_CANONICAL_MODE 0x0002
#define TERMINAL_FLAG_ECHO 0x0004
#define TERMINAL_FLAG_SIGNAL 0x0001
#define TERMINAL_INDEX_VMIN 9
#define TERMINAL_INDEX_VTIME 16

#else

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

#define TERMINAL_FLAG_CANONICAL_MODE 0x0002
#define TERMINAL_FLAG_ECHO 0x0008
#define TERMINAL_FLAG_SIGNAL 0x0001
#define TERMINAL_INDEX_VMIN 6
#define TERMINAL_INDEX_VTIME 5

#endif

#ifdef __APPLE__
#define TIOCGWINSZ 0x40087468
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define TIOCGWINSZ 0x5401
#else
#define TIOCGWINSZ 0x5413
#endif

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN 0x0001

#define EDITOR_MODE_NORMAL 0
#define EDITOR_MODE_INSERT 1
#define EDITOR_MODE_COMMAND 2

#define KEY_INSERT 0x101

#define OUTPUT_BUFFER_MAXIMUM (1024 * 64)
#define TEXT_BUFFER_SIZE 4096
#define TABULATION_WIDTH 8

typedef enum {
    PIECE_SOURCE_ORIGINAL = 0,
    PIECE_SOURCE_ADD = 1
} PieceSource;

#define PS PieceSource

typedef struct PieceNode {
    __SIZE_TYPE__ StartOffset;
    __SIZE_TYPE__ Length;
    __SIZE_TYPE__ LineFeeds;
    __SIZE_TYPE__ SubtreeLength;
    __SIZE_TYPE__ SubtreeLineFeeds;

    struct PieceNode* LeftChild;
    struct PieceNode* RightChild;

    __UINT32_TYPE__ Priority;
    __INT32_TYPE__ ReferenceCount;
    PS Source;
    __UINT32_TYPE__ pad;
} PieceNode;

#define PN PieceNode

typedef struct {
    PN* left_node;
    PN* right_node;
} SplitResult;

#define SP SplitResult

#endif
