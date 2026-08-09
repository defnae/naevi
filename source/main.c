// main.c

#include <headers/std/int.h>
#include <headers/std/char.h>
#include <headers/std/bool.h>
#include <headers/std/def.h>
#include <headers/std/arg.h>
#include <headers/std/str.h>
#include <headers/std/attr.h>

#ifdef _WIN32
#define ABI MSABI
#else
#define ABI SYSVABI
#endif

#if defined(__linux__)
#define NAEVI_LINUX 1
#elif defined(__APPLE__)
#define NAEVI_DARWIN 1
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define NAEVI_MSYS 1
#else
#error "Unsupported platform: only Linux, macOS, and MSYS2 are supported."
#endif

typedef ptrdiff_t ssize_t;
typedef long long off_t;

#if defined(NAEVI_LINUX)
typedef unsigned long nfds_t;
#else
typedef unsigned int nfds_t;
#endif

typedef struct termios termios;
typedef struct stat stat;
typedef struct pollfd pollfd;
typedef struct winsize winsize;

extern void* malloc(size_t);
extern void* realloc(void*, size_t);
extern void free(void*);

extern ssize_t read(int, void*, size_t);
extern ssize_t write(int, const void*, size_t);

extern int open(const char*, int, ...);
extern int close(int);
extern int fstat(int, stat*);

extern int tcgetattr(int, termios*);
extern int tcsetattr(int, int, const termios*);

extern int ioctl(int, unsigned long, ...);
extern int poll(pollfd*, nfds_t, int);

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001

#if defined(NAEVI_DARWIN) || defined(NAEVI_MSYS)
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#else
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#endif

#if defined(NAEVI_LINUX)
#define STAT_BUF_SIZE 144
#define STAT_SIZE_OFFSET 48
#elif defined(NAEVI_DARWIN)
#define STAT_BUF_SIZE 144
#define STAT_SIZE_OFFSET 96
#elif defined(NAEVI_MSYS)
#define STAT_BUF_SIZE 128
#define STAT_SIZE_OFFSET 40
#else
#define STAT_BUF_SIZE 144
#define STAT_SIZE_OFFSET 48
#endif

#define TCSANOW 0

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#if defined(NAEVI_LINUX)

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    char8_t c_reserved_pad[3];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define T_ICANON 0x0002
#define T_ECHO 0x0008
#define T_ISIG 0x0001
#define T_VMIN 6
#define T_VTIME 5

#elif defined(NAEVI_DARWIN)

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

#define T_ICANON 0x00000100
#define T_ECHO 0x00000008
#define T_ISIG 0x00000080
#define T_VMIN 16
#define T_VTIME 17

#elif defined(NAEVI_MSYS)

#define NCCS 18

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    char c_line;
    cc_t c_cc[NCCS];
    char8_t c_reserved_pad[1];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define T_ICANON 0x0002
#define T_ECHO 0x0004
#define T_ISIG 0x0001
#define T_VMIN 9
#define T_VTIME 16

#else

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    char8_t c_reserved_pad[3];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define T_ICANON 0x0002
#define T_ECHO 0x0008
#define T_ISIG 0x0001
#define T_VMIN 6
#define T_VTIME 5

#endif

#if defined(NAEVI_DARWIN)
#define TIOCGWINSZ 0x40087468
#elif defined(NAEVI_MSYS)
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

#define NORMAL_MODE 0
#define INSERT_MODE 1
#define COMMAND_MODE 2

#define KEY_INSERT 0x101

#define OUTPUT_MAX (1024 * 64)
#define TEXT_BUF_SIZE 4096
#define TAB_WIDTH 8

typedef struct {
    char8_t* Buffer;
    size_t* LineOffset;
    size_t BufferLength;
    size_t BufferCapacity;
    size_t LineCount;
    size_t LineCapacity;
    size_t Row;
    size_t Column;
    size_t Top;
    size_t StatusLength;
    size_t CommandLineLength;
    size_t OutputLength;
    termios OriginalTermios;
    unsigned short Rows;
    unsigned short Columns;
    char8_t FileName[TEXT_BUF_SIZE];
    char8_t Status[TEXT_BUF_SIZE];
    char8_t CommandLine[TEXT_BUF_SIZE];
    char8_t OutputBuffer[OUTPUT_MAX];
    uint8_t Mode;
    bool Dirty;
    bool Running;
    bool ForceQuit;
    char8_t Pushback;
    bool HasPushback;
    char8_t Pending;
    char8_t pad[1];
} Globals;

static Globals GlobalData = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 24, 80, {0}, {0}, {0}, {0}, NORMAL_MODE, false, true, false, 0, false, 0, {0} };
static Globals* G = &GlobalData;

static ABI bool input_ready(int timeoutMilliseconds);
static ABI char8_t* u64toa(uint64_t value, char8_t* buffer);
static ABI void ensure_buffer_capacity(size_t needed);
static ABI void ensure_line_capacity(size_t needed);
static ABI void rebuild_line_offsets(void);
static ABI size_t line_length(size_t line);
static ABI size_t line_layout(size_t line, size_t stopByte, size_t *stopRow, size_t *stopCol);
static ABI size_t line_visual_row_count(size_t line);
static ABI size_t utf8_char_length(char8_t c);
static ABI void clamp_column(void);
static ABI void output_flush(void);
static ABI void output_bytes(const char8_t* s, size_t count);
static ABI void output_string(const char8_t* s);
static ABI void output_byte(char8_t c);
static ABI void output_csi(const char8_t* sequence);
static ABI void cursor_goto(unsigned short row, unsigned short column);
static ABI void set_status(const char8_t* message);
static ABI void save_file(void);
static ABI void render(void);
static ABI size_t cursor_offset(void);
static ABI void buffer_insert(size_t offset, char8_t c);
static ABI void buffer_delete(size_t offset);
static ABI void delete_char_at_cursor(void);
static ABI void adjust_scroll(void);
static ABI void move_cursor(int direction);

ABI int main(int argc, char* argv[]) {
    int byte;

    G->Running = true;

    {
        winsize windowSize;
        if (ioctl(1, TIOCGWINSZ, &windowSize) == 0) {
            if (windowSize.ws_row > 0) G->Rows = windowSize.ws_row;
            if (windowSize.ws_col > 0) G->Columns = windowSize.ws_col;
        }
    }

    if (argc < 2) {
        G->BufferLength = 0;
        rebuild_line_offsets();
    } else {
        int fd;
        ssize_t n;
        off_t size;
        const char8_t* path = argv[1];
        size_t pathLength = strlen((const char*) path);
        size_t i_clr;

        if (pathLength >= sizeof(G->FileName)) pathLength = sizeof(G->FileName) - 1;

        for (i_clr = 0; i_clr < sizeof(G->FileName); i_clr++) G->FileName[i_clr] = 0;

        memcpy(G->FileName, path, pathLength);

        fd = open(argv[1], O_RDONLY, 0);
        if (fd < 0) {
            G->BufferLength = 0;
            rebuild_line_offsets();
        } else {
            uint8_t statBuffer[STAT_BUF_SIZE];
            if (fstat(fd, (stat*) statBuffer) != 0) size = -1;
            else {
                off_t* size_ptr = (off_t*) (statBuffer + STAT_SIZE_OFFSET);
                size = *size_ptr;
            }

            if (size > 0) {
                size_t total = 0;
                ensure_buffer_capacity((size_t) size);
                while (total < (size_t) size) {
                    n = read(fd, G->Buffer + total, (size_t) size - total);
                    if (n <= 0) break;

                    total += (size_t) n;
                }

                G->BufferLength = total;
            }

            close(fd);
            rebuild_line_offsets();

            G->Dirty = false;
        }
    }

    {
        termios settings;
        tcflag_t mask;
        tcgetattr(0, &G->OriginalTermios);
        settings = G->OriginalTermios;
        mask = T_ICANON | T_ECHO | T_ISIG;
        settings.c_lflag &= ~mask;
        settings.c_cc[T_VMIN] = 1;
        settings.c_cc[T_VTIME] = 0;
        tcsetattr(0, TCSANOW, &settings);
    }

    output_csi("2J");
    render();

    while (G->Running) {
        ssize_t n;

        if (G->HasPushback) {
            byte = G->Pushback;
            G->HasPushback = false;
        } else {
            char8_t rawByte;
            n = read(0, &rawByte, 1);
            if (n <= 0) break;
            byte = rawByte;
        }

        if (byte == 0x1b && input_ready(20)) {
            char8_t next;
            bool isNavKey = false;

            n = read(0, &next, 1);
            if (n > 0 && next == '[' && input_ready(20)) {
                char8_t code;
                n = read(0, &code, 1);
                if (n > 0) {
                    switch (code) {
                        case 'A': byte = 'k'; isNavKey = true; break;
                        case 'B': byte = 'j'; isNavKey = true; break;
                        case 'C': byte = 'l'; isNavKey = true; break;
                        case 'D': byte = 'h'; isNavKey = true; break;
                        case 'H': byte = '0'; isNavKey = true; break;
                        case 'F': byte = '$'; isNavKey = true; break;

                        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': {
                            if (input_ready(20)) {
                                char8_t tilde;
                                n = read(0, &tilde, 1);
                                if (n > 0) {
                                    if (tilde == '~') {
                                        switch (code) {
                                            case '1': case '7': byte = '0'; isNavKey = true; break;
                                            case '4': case '8': byte = '$'; isNavKey = true; break;
                                            case '2': byte = KEY_INSERT; isNavKey = true; break;
                                            case '3': byte = 'x'; isNavKey = true; break;
                                            case '5': byte = 0x02; isNavKey = true; break;
                                            case '6': byte = 0x06; isNavKey = true; break;
                                            default: break;
                                        }
                                    } else {
                                        // Sequence has modifiers (e.g. ESC [ 3 ; 5 ~)
                                        // Drain remaining parameter/modifier bytes until terminating byte (0x40 - 0x7E)
                                        char8_t drain = tilde;
                                        while ((drain < 0x40 || drain > 0x7E) && input_ready(20)) {
                                            if (read(0, &drain, 1) <= 0) break;
                                        }
                                    }
                                }
                            }
                            break;
                        }

                        default: break;
                    }
                } else {
                    G->Pushback = next;
                    G->HasPushback = true;

                    byte = 0x1b;
                }
            }

            if (isNavKey) {
                if (byte == KEY_INSERT) {
                    if (G->Mode == NORMAL_MODE || G->Mode == INSERT_MODE) {
                        if (G->Mode == INSERT_MODE) {
                            if (G->Column > 0) {
                                size_t start = G->LineOffset[G->Row];
                                G->Column--;
                                while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
                            }
                            G->Mode = NORMAL_MODE;
                        } else {
                            G->Mode = INSERT_MODE;
                        }
                        clamp_column();
                        adjust_scroll();
                    }
                } else {
                    switch (G->Mode) {
                        case NORMAL_MODE:
                        case INSERT_MODE: {
                            if (byte == 'x') {
                                delete_char_at_cursor();
                            } else {
                                move_cursor(byte);
                            }
                            clamp_column();
                            adjust_scroll();

                            break;
                        }

                        default: break;
                    }
                }

                render(); continue;
            }

            if (byte == 0x1b) switch (G->Mode) {
                case INSERT_MODE: {
                    if (G->Column > 0) {
                        size_t start = G->LineOffset[G->Row];
                        G->Column--;
                        while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
                    }

                    G->Mode = NORMAL_MODE;

                    switch (G->Mode) {
                        case NORMAL_MODE: clamp_column(); break;

                        default: break;
                    }

                    adjust_scroll(); break;
                }

                case COMMAND_MODE: G->Mode = NORMAL_MODE; G->CommandLineLength = 0; break;

                default: break;
            }

            render(); continue;
        }

        switch (G->Mode) {
            case NORMAL_MODE: {
                G->StatusLength = 0;

                switch (G->Pending) {
                    case 'd': {
                        G->Pending = 0;
                        if (byte != 'd') break;

                        if (G->Row < G->LineCount) {
                            size_t start = G->LineOffset[G->Row];
                            size_t end = (G->Row + 1 < G->LineCount) ? G->LineOffset[G->Row + 1] : G->BufferLength;
                            size_t length = end - start;

                            if (length > 0) {
                                size_t count_del = G->BufferLength - end;
                                memmove(G->Buffer + start, G->Buffer + end, count_del);

                                G->BufferLength -= length;
                                rebuild_line_offsets();

                                G->Dirty = true;
                            }
                        }

                        if (G->Row >= G->LineCount && G->LineCount > 0) G->Row = G->LineCount - 1;

                        break;
                    }

                    case 'g': {
                        G->Pending = 0;
                        if (byte == 'g') {
                            G->Row = 0;
                            G->Column = 0;
                            G->Top = 0;
                        }

                        break;
                    }

                    default: {
                        switch (byte) {
                            case 'h': move_cursor('h'); break;
                            case 'l': move_cursor('l'); break;
                            case 'j': move_cursor('j'); break;
                            case 'k': move_cursor('k'); break;
                            case '0': move_cursor('0'); break;
                            case '$': move_cursor('$'); break;
                            case 'G': if (G->LineCount > 0) G->Row = G->LineCount - 1; break;
                            case 'g': G->Pending = 'g'; break;
                            case 'i': G->Mode = INSERT_MODE; break;
                            case 'a': {
                                size_t len = line_length(G->Row);
                                G->Mode = INSERT_MODE;
                                if (len > 0) {
                                    size_t start = G->LineOffset[G->Row];
                                    size_t cLen = utf8_char_length(G->Buffer[start + G->Column]);
                                    G->Column += cLen;
                                }
                                break;
                            }
                            case 'A': G->Mode = INSERT_MODE; G->Column = line_length(G->Row); break;
                            case 'I': G->Column = 0; G->Mode = INSERT_MODE; break;

                            case 'o': {
                                size_t offset = G->LineOffset[G->Row] + line_length(G->Row);
                                if (offset < G->BufferLength && G->Buffer[offset] == '\n') offset++;
                                buffer_insert(offset, '\n');
                                G->Row++;
                                G->Column = 0;
                                G->Mode = INSERT_MODE;

                                break;
                            }

                            case 'O': {
                                buffer_insert(G->LineOffset[G->Row], '\n');
                                G->Column = 0;
                                G->Mode = INSERT_MODE;

                                break;
                            }

                            case 'x': {
                                delete_char_at_cursor();
                                break;
                            }
                            case 'd': G->Pending = 'd'; break;

                            case ':': {
                                size_t i_cmd;

                                G->Mode = COMMAND_MODE;
                                G->CommandLineLength = 0;

                                for (i_cmd = 0; i_cmd < sizeof(G->CommandLine); i_cmd++) G->CommandLine[i_cmd] = 0;

                                break;
                            }

                            case 0x06: move_cursor(0x06); break;
                            case 0x02: move_cursor(0x02); break;

                            default: break;
                        }

                        break;
                    }
                }

                switch (G->Mode) {
                    case NORMAL_MODE: clamp_column(); break;

                    default: break;
                }

                adjust_scroll(); break;
            }

            case INSERT_MODE: {
                switch (byte) {
                    case 0x1b: {
                        if (G->Column > 0) {
                            size_t start = G->LineOffset[G->Row];
                            G->Column--;
                            while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
                        }
                        G->Mode = NORMAL_MODE;
                        break;
                    }

                    case 127:
                    case 8: {
                        size_t bytesToDelete;
                        size_t k;

                        if (cursor_offset() > 0) {
                            bool mergingUp = (G->Column == 0 && G->Row > 0);
                            size_t joinColumn = mergingUp ? line_length(G->Row - 1) : 0;

                            if (mergingUp) {
                                buffer_delete(cursor_offset() - 1);
                                G->Row--;
                                G->Column = joinColumn;
                            } else if (G->Column > 0) {
                                size_t start = G->LineOffset[G->Row];
                                size_t oldCol = G->Column;
                                G->Column--;
                                while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;

                                bytesToDelete = oldCol - G->Column;
                                for (k = 0; k < bytesToDelete; k++) buffer_delete(start + G->Column);
                            }
                        }

                        break;
                    }

                    case '\n':
                    case '\r': {
                        buffer_insert(cursor_offset(), '\n');

                        G->Row++;
                        G->Column = 0;

                        break;
                    }

                    case '\t': {
                        buffer_insert(cursor_offset(), '\t');
                        G->Column++;

                        break;
                    }

                    default:
                        if (byte >= 32 && byte < 0x100) {
                            buffer_insert(cursor_offset(), (char8_t) byte);
                            G->Column++;
                        }
                        break;
                }

                switch (G->Mode) {
                    case NORMAL_MODE: clamp_column(); break;

                    default: break;
                }

                adjust_scroll(); break;
            }

            case COMMAND_MODE: {
                switch (byte) {
                    case 0x1b: G->Mode = NORMAL_MODE; G->CommandLineLength = 0; break;

                    case '\n':
                    case '\r': {
                        char8_t* command;

                        G->CommandLine[G->CommandLineLength] = '\0';
                        command = G->CommandLine;

                        switch (command[0]) {
                            case 'q': {
                                switch (command[1]) {
                                    case '\0': {
                                        if (G->Dirty && !G->ForceQuit) set_status("Unsaved changes, :q! to force.");

                                        else G->Running = false; break;
                                    }

                                    case '!': {
                                        switch (command[2]) {
                                            case '\0': G->Running = false; break;

                                            default: set_status("Unknown command."); break;
                                        }

                                        break;
                                    }

                                    default: set_status("Unknown command."); break;
                                }

                                break;
                            }

                            case 'w': {
                                switch (command[1]) {
                                    case '\0':
                                    case '!': {
                                        switch (command[command[1] == '!' ? 2 : 1]) {
                                            case '\0': save_file(); break;

                                            default: set_status("Unknown command."); break;
                                        }

                                        break;
                                    }

                                    case 'q': {
                                        switch (command[2]) {
                                            case '\0': {
                                                save_file();

                                                G->Running = false; break;
                                            }

                                            case '!': {
                                                switch (command[3]) {
                                                    case '\0': {
                                                        save_file();

                                                        G->Running = false; break;
                                                    }

                                                    default: set_status("Unknown command."); break;
                                                }

                                                break;
                                            }
                                            case ' ': {
                                                switch (command[3]) {
                                                    case '\0': set_status("Unknown command."); break;
                                                    default: {
                                                        size_t length = strlen((const char*) (command + 3));

                                                        if (length >= sizeof(G->FileName)) length = sizeof(G->FileName) - 1;
                                                        memcpy(G->FileName, command + 3, length);
                                                        G->FileName[length] = '\0';

                                                        save_file();
                                                        G->Running = false; break;
                                                    }
                                                }

                                                break;
                                            }

                                            default: set_status("Unknown command."); break;
                                        }

                                        break;
                                    }
                                    case ' ': {
                                        switch (command[2]) {
                                            case '\0': set_status("Unknown command."); break;
                                            default: {
                                                size_t length = strlen((const char*) (command + 2));

                                                if (length >= sizeof(G->FileName)) length = sizeof(G->FileName) - 1;
                                                memcpy(G->FileName, command + 2, length);
                                                G->FileName[length] = '\0';

                                                save_file(); break;
                                            }
                                        }

                                        break;
                                    }

                                    default: set_status("Unknown command."); break;
                                }

                                break;
                            }

                            case 'x': {
                                switch (command[1]) {
                                    case '\0': {
                                        save_file();

                                        G->Running = false; break;
                                    }

                                    case '!': {
                                        switch (command[2]) {
                                            case '\0': {
                                                save_file();

                                                G->Running = false; break;
                                            }

                                            default: set_status("Unknown command."); break;
                                        }

                                        break;
                                    }

                                    default: set_status("Unknown command."); break;
                                }

                                break;
                            }

                            default: set_status("Unknown command."); break;
                        }

                        G->Mode = NORMAL_MODE;
                        G->CommandLineLength = 0; break;
                    }

                    case 127:
                    case 8: if (G->CommandLineLength > 0) G->CommandLineLength--; break;

                    default: if (byte >= 32 && byte < 127 && G->CommandLineLength + 1 < sizeof(G->CommandLine)) G->CommandLine[G->CommandLineLength++] = (char8_t) byte; break;
                }

                break;
            }

            default: break;
        }

        render();
    }

    tcsetattr(0, TCSANOW, &G->OriginalTermios);
    output_csi("?7h");
    output_csi("2J");
    cursor_goto(0, 0);
    output_flush();

    return 0;
}

static bool input_ready(int timeoutMilliseconds) {
    pollfd descriptor;
    descriptor.fd = 0;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    return poll(&descriptor, 1, timeoutMilliseconds) > 0;
}

static char8_t* u64toa(uint64_t value, char8_t* buffer) {
    char8_t* p = buffer + 20;

    *p = '\0';
    if (value == 0) {
        *--p = '0';

        return p;
    }

    while (value) {
        *--p = '0' + value % 10;
        value /= 10;
    }

    return p;
}

static void ensure_buffer_capacity(size_t needed) {
    size_t newCapacity;

    if (needed <= G->BufferCapacity) return;

    newCapacity = G->BufferCapacity ? G->BufferCapacity : 4096;
    while (newCapacity < needed) newCapacity *= 2;

    G->Buffer = (char8_t*) realloc(G->Buffer, newCapacity);
    G->BufferCapacity = newCapacity;
}

static void ensure_line_capacity(size_t needed) {
    size_t newCapacity;
    if (needed <= G->LineCapacity) return;

    newCapacity = G->LineCapacity ? G->LineCapacity : 256;
    while (newCapacity < needed) newCapacity *= 2;

    G->LineOffset = (size_t*) realloc(G->LineOffset, newCapacity * sizeof(size_t));
    G->LineCapacity = newCapacity;
}

static void rebuild_line_offsets(void) {
    size_t i;

    G->LineCount = 0;
    ensure_line_capacity(1);

    G->LineOffset[G->LineCount++] = 0;
    for (i = 0; i < G->BufferLength; i++) {
        if (G->Buffer[i] != '\n') continue;
        if (i + 1 <= G->BufferLength) {
            ensure_line_capacity(G->LineCount + 1);

            G->LineOffset[G->LineCount++] = i + 1;
        }
    }
}

static size_t line_length(size_t line) {
    size_t start, end;
    if (line >= G->LineCount) return 0;

    start = G->LineOffset[line];
    end = (line + 1 < G->LineCount) ? G->LineOffset[line + 1] - 1 : G->BufferLength;

    return (end > start) ? (end - start) : 0;
}

static size_t line_layout(size_t line, size_t stopByte, size_t *stopRow, size_t *stopCol) {
    size_t start, length, columns, byteOfs, row, visCol;
    bool stopped;

    if (line >= G->LineCount) {
        if (stopRow) *stopRow = 0;
        if (stopCol) *stopCol = 0;
        return 1;
    }

    start = G->LineOffset[line];
    length = line_length(line);
    columns = G->Columns ? G->Columns : 1;

    byteOfs = 0;
    row = 0;
    visCol = 0;
    stopped = false;

    while (byteOfs < length) {
        char8_t c = G->Buffer[start + byteOfs];

        if (c == '\t') {
            size_t width = TAB_WIDTH - (visCol % TAB_WIDTH);

            if (visCol + width > columns && visCol > 0) {
                row++;
                visCol = 0;
                continue;
            }

            if (!stopped && byteOfs >= stopByte) {
                if (stopRow) *stopRow = row;
                if (stopCol) *stopCol = visCol;
                stopped = true;
            }

            visCol += width;
            byteOfs++;
        } else if ((c & 0xC0) == 0x80) {
            byteOfs++;
        } else {
            if (visCol + 1 > columns && visCol > 0) {
                row++;
                visCol = 0;
                continue;
            }

            if (!stopped && byteOfs >= stopByte) {
                if (stopRow) *stopRow = row;
                if (stopCol) *stopCol = visCol;
                stopped = true;
            }

            visCol++;
            byteOfs++;

            while (byteOfs < length && (G->Buffer[start + byteOfs] & 0xC0) == 0x80) byteOfs++;
        }
    }

    if (!stopped) {
        if (stopRow) *stopRow = row;
        if (stopCol) *stopCol = visCol;
    }

    return row + 1;
}

static size_t line_visual_row_count(size_t line) {
    return line_layout(line, (size_t) -1, NULL, NULL);
}

static size_t utf8_char_length(char8_t c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;

    return 1;
}

static void clamp_column(void) {
    size_t length = line_length(G->Row);
    size_t start = G->LineOffset[G->Row];

    if (length == 0) {
        G->Column = 0;
        return;
    }

    if (G->Mode == INSERT_MODE) {
        if (G->Column > length) G->Column = length;
    } else {
        if (G->Column >= length) G->Column = length - 1;
    }

    while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
}

static void output_flush(void) {
    if (!G->OutputLength) return;
    write(1, G->OutputBuffer, G->OutputLength);

    G->OutputLength = 0;
}

static void output_bytes(const char8_t* s, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (G->OutputLength == OUTPUT_MAX) output_flush();

        G->OutputBuffer[G->OutputLength++] = s[i];
    }
}

static void output_string(const char8_t* s) {
    size_t length = strlen((const char*) s);

    output_bytes(s, length);
}

static void output_byte(char8_t c) {
    if (G->OutputLength == OUTPUT_MAX) output_flush();

    G->OutputBuffer[G->OutputLength++] = c;
}

static void output_csi(const char8_t* sequence) {
    output_byte(0x1b);
    output_byte('[');
    output_string(sequence);
}

static void cursor_goto(unsigned short row, unsigned short column) {
    char8_t rowBuffer[22], columnBuffer[22];
    char8_t* rowString = u64toa(row + 1, rowBuffer);
    char8_t* columnString = u64toa(column + 1, columnBuffer);

    output_byte(0x1b);
    output_byte('[');
    output_string(rowString);
    output_byte(';');
    output_string(columnString);
    output_byte('H');
}

static void set_status(const char8_t* message) {
    size_t length = strlen((const char*) message);

    if (length >= sizeof(G->Status)) length = sizeof(G->Status) - 1;

    memcpy(G->Status, message, length);

    G->StatusLength = length;
}

static void save_file(void) {
    int fd;
    size_t written = 0;
    ssize_t n;
    char8_t numberBuffer[22];
    char8_t* numberString;
    size_t fileNameLength = 0;
    const char8_t* s_w = " written ";
    const char8_t* s_b = "B";
    size_t numLen = 0;

    if (!G->FileName[0]) {
        set_status("No filename.");

        return;
    }

    fd = open(G->FileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        set_status("Failed to write file.");

        return;
    }

    while (written < G->BufferLength) {
        n = write(fd, G->Buffer + written, G->BufferLength - written);
        if (n <= 0) break;
        written += (size_t) n;
    }

    close(fd);
    G->Dirty = false;

    numberString = u64toa(written, numberBuffer);

    fileNameLength = strlen((const char*) G->FileName);
    G->StatusLength = 0;

    memcpy(G->Status + G->StatusLength, G->FileName, fileNameLength);
    G->StatusLength += fileNameLength;

    memcpy(G->Status + G->StatusLength, s_w, 9);
    G->StatusLength += 9;

    numLen = strlen((const char*) numberString);
    memcpy(G->Status + G->StatusLength, numberString, numLen);
    G->StatusLength += numLen;

    memcpy(G->Status + G->StatusLength, s_b, 1);
    G->StatusLength += 1;
}

static void render(void) {
    unsigned short screenRows = G->Rows - 1;
    unsigned short screenRow = 0;
    size_t columns = G->Columns ? G->Columns : 1;
    size_t line = G->Top;

    output_csi("?25l");
    output_csi("?7l");

    while (screenRow < screenRows && line < G->LineCount) {
        size_t length = line_length(line);
        size_t start = G->LineOffset[line];
        size_t rowsForLine = line_visual_row_count(line);
        size_t subRow;
        size_t byteOfs = 0;

        for (subRow = 0; subRow < rowsForLine && screenRow < screenRows; subRow++) {
            size_t visCol = 0;

            cursor_goto(screenRow, 0);
            output_csi("2K");

            while (byteOfs < length) {
                char8_t c = G->Buffer[start + byteOfs];

                if (c == '\t') {
                    size_t width = TAB_WIDTH - (visCol % TAB_WIDTH);
                    size_t k;

                    if (visCol + width > columns && visCol > 0) break;

                    for (k = 0; k < width; k++) output_byte(' ');

                    visCol += width;
                    byteOfs++;
                } else if ((c & 0xC0) == 0x80) {
                    output_byte(c);
                    byteOfs++;
                } else {
                    if (visCol + 1 > columns && visCol > 0) break;

                    output_byte(c);
                    visCol++;
                    byteOfs++;

                    while (byteOfs < length && (G->Buffer[start + byteOfs] & 0xC0) == 0x80) {
                        output_byte(G->Buffer[start + byteOfs]);
                        byteOfs++;
                    }
                }
            }

            screenRow++;
        }

        line++;
    }

    while (screenRow < screenRows) {
        cursor_goto(screenRow, 0);
        output_csi("2K");
        output_byte('~');

        screenRow++;
    }

    cursor_goto(G->Rows - 1, 0);
    output_csi("2K");

    switch (G->Mode) {
        case COMMAND_MODE: output_byte(':'); output_bytes(G->CommandLine, G->CommandLineLength); break;

        default: {
            switch (G->Mode) {
                case INSERT_MODE: output_csi("7m"); output_string(" INSERT "); output_csi("m"); output_byte(' '); break;

                default: output_csi("7m"); output_string(" NORMAL "); output_csi("m"); output_byte(' '); break;
            }

            if (G->StatusLength) output_bytes(G->Status, G->StatusLength);
            else {
                output_string(G->FileName[0] ? G->FileName : "[No Name]");
                if (G->Dirty) output_string(" [+]");
            }

            {
                char8_t rowBuffer[22], columnBuffer[22];
                char8_t* rowString = u64toa(G->Row + 1, rowBuffer);
                char8_t* columnString = u64toa(G->Column + 1, columnBuffer);
                size_t rowStrLen = strlen((const char*) rowString);
                size_t colStrLen = strlen((const char*) columnString);
                size_t infoLength = rowStrLen + 1 + colStrLen;

                if (G->Columns > infoLength + 1) {
                    cursor_goto(G->Rows - 1, (unsigned short) (G->Columns - infoLength - 1));
                    output_string(rowString);
                    output_byte(':');
                    output_string(columnString);
                }
            }

            break;
        }
    }

    switch (G->Mode) {
        case COMMAND_MODE: cursor_goto(G->Rows - 1, (unsigned short) (1 + G->CommandLineLength)); break;

        default: {
            size_t visualRow = 0;
            size_t lineIndex;
            size_t rowInLine, colInLine;
            for (lineIndex = G->Top; lineIndex < G->Row; lineIndex++) visualRow += line_visual_row_count(lineIndex);

            line_layout(G->Row, G->Column, &rowInLine, &colInLine);
            visualRow += rowInLine;
            cursor_goto((unsigned short) visualRow, (unsigned short) colInLine);

            break;
        }
    }

    output_csi("?7h");
    output_csi("?25h");
    output_flush();
}

static size_t cursor_offset(void) {
    size_t offset = G->LineOffset[G->Row] + G->Column;

    return offset > G->BufferLength ? G->BufferLength : offset;
}

static void buffer_insert(size_t offset, char8_t c) {
    size_t count_ins;

    ensure_buffer_capacity(G->BufferLength + 1);

    count_ins = G->BufferLength - offset;
    memmove(G->Buffer + offset + 1, G->Buffer + offset, count_ins);

    G->Buffer[offset] = c;
    G->BufferLength++;
    rebuild_line_offsets();

    G->Dirty = true;
}

static void buffer_delete(size_t offset) {
    size_t count_del;

    if (offset >= G->BufferLength) return;

    count_del = G->BufferLength - offset - 1;
    memmove(G->Buffer + offset, G->Buffer + offset + 1, count_del);

    G->BufferLength--;
    rebuild_line_offsets();

    G->Dirty = true;
}

static void delete_char_at_cursor(void) {
    size_t len = line_length(G->Row);
    if (len > 0 && G->Column < len) {
        size_t start = G->LineOffset[G->Row];
        size_t rem = len - G->Column;
        size_t cLen = utf8_char_length(G->Buffer[start + G->Column]);
        size_t k;

        if (cLen > rem) cLen = rem;

        for (k = 0; k < cLen; k++) buffer_delete(cursor_offset());
    }
}

static void adjust_scroll(void) {
    unsigned short screenRows = G->Rows - 1;

    size_t visualRows;
    size_t line;
    size_t rowInLine;

    if (G->Row < G->Top) G->Top = G->Row;

    visualRows = 0;
    for (line = G->Top; line < G->Row; line++) visualRows += line_visual_row_count(line);

    line_layout(G->Row, G->Column, &rowInLine, NULL);
    visualRows += rowInLine + 1;

    while (visualRows > screenRows && G->Top < G->Row) {
        visualRows -= line_visual_row_count(G->Top);

        G->Top++;
    }
}

static void move_cursor(int direction) {
    size_t length = line_length(G->Row);
    size_t start = G->LineOffset[G->Row];

    switch (direction) {
        case 'h': {
            if (G->Column > 0) {
                G->Column--;
                while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
            }
            break;
        }
        case 'l': {
            if (length > 0 && G->Column < length) {
                G->Column++;
                while (G->Column < length && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column++;
            }
            break;
        }
        case 'j': if (G->Row + 1 < G->LineCount) G->Row++; break;
        case 'k': if (G->Row > 0) G->Row--; break;

        case '0': G->Column = 0; break;

        case '$': {
            if (length > 0) {
                if (G->Mode == INSERT_MODE) {
                    G->Column = length;
                } else {
                    G->Column = length - 1;
                    while (G->Column > 0 && (G->Buffer[start + G->Column] & 0xC0) == 0x80) G->Column--;
                }
            } else {
                G->Column = 0;
            }
            break;
        }

        case 0x06: {
            G->Row += (G->Rows - 1) / 2;
            if (G->Row >= G->LineCount && G->LineCount > 0) G->Row = G->LineCount - 1;
            break;
        }

        case 0x02: {
            if (G->Row >= (G->Rows - 1) / 2) G->Row -= (G->Rows - 1) / 2;
            else G->Row = 0;
            break;
        }

        default: break;
    }
}
