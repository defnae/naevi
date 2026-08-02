// main.c
/* naevi -- zero dependency vi clone
 * C89 + Clang extensions, portable across Linux, macOS, and MSYS2
 * build: clang -std=c89 -O2 -o naevi main.c
 */

#include <headers/std/int.h>
#include <headers/std/char.h>
#include <headers/std/bool.h>
#include <headers/std/def.h>
#include <headers/std/str.h>
#include <headers/std/attr.h>
#include <headers/platform.h>
#include <headers/mem.h>
#include <headers/term.h>

#ifdef _WIN32
#define ABI MSABI
#else
#define ABI SYSVABI
#endif

/* ── tiny utilities ──────────────────────────────────────────────────────── */

static size_t strlen8(const char8_t *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void memcpy8(char8_t *dst, const char8_t *src, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) dst[i] = src[i];
}

static void memmove8(char8_t *dst, const char8_t *src, size_t n) {
    int64_t i;
    if (dst < src) {
        size_t j;
        for (j = 0; j < n; j++) dst[j] = src[j];
    } else {
        for (i = (int64_t)n - 1; i >= 0; i--) dst[i] = src[i];
    }
}

static void memset8(char8_t *p, char8_t v, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) p[i] = v;
}

static int32_t streq(const char8_t *a, const char8_t *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* uint64 -> decimal string, returns start pointer in buf (buf must be 21 bytes) */
static char8_t *u64toa(uint64_t v, char8_t *buf) {
    char8_t *p = buf + 20;
    *p = '\0';
    if (v == 0) { *--p = '0'; return p; }
    while (v) { *--p = (char8_t)('0' + v % 10); v /= 10; }
    return p;
}

/* ── editor buffer (dynamic) ─────────────────────────────────────────────── */

static char8_t *g_buf     = NULL;
static size_t   g_buf_len = 0;
static size_t   g_buf_cap = 0;

/* line index: byte offset of the start of each line (dynamic) */
static size_t  *g_line_off   = NULL;
static size_t   g_line_count = 0;
static size_t   g_line_cap   = 0;

static void buf_ensure(size_t needed) {
    if (needed <= g_buf_cap) return;
    {
        size_t new_cap = g_buf_cap ? g_buf_cap : 4096;
        while (new_cap < needed) new_cap *= 2;
        g_buf = (char8_t *)realloc(g_buf, new_cap);
        g_buf_cap = new_cap;
    }
}

static void lines_ensure(size_t needed) {
    if (needed <= g_line_cap) return;
    {
        size_t new_cap = g_line_cap ? g_line_cap : 256;
        while (new_cap < needed) new_cap *= 2;
        g_line_off = (size_t *)realloc(g_line_off, new_cap * sizeof(size_t));
        g_line_cap = new_cap;
    }
}

/* rebuild line index from g_buf.
 *
 * NOTE: this rescans the whole buffer on every call, same as before.
 * insert/delete now grow without a hard cap, but this pass didn't
 * change rebuild_lines to be incremental (patch line offsets in place
 * instead of a full rescan) -- that's a real perf issue on big files,
 * deliberately deferred to a follow-up pass rather than rolled into
 * this one alongside the buffer/portability work. */
static void rebuild_lines(void) {
    size_t i;
    g_line_count = 0;
    lines_ensure(1);
    g_line_off[g_line_count++] = 0;
    for (i = 0; i < g_buf_len; i++) {
        if (g_buf[i] == '\n') {
            if (i + 1 <= g_buf_len) {
                lines_ensure(g_line_count + 1);
                g_line_off[g_line_count++] = i + 1;
            }
        }
    }
    /* if last byte is '\n', last "line" is empty and already recorded */
}

/* length of line l (not including the \n) */
static size_t line_len(size_t l) {
    size_t start, end;
    if (l >= g_line_count) return 0;
    start = g_line_off[l];
    if (l + 1 < g_line_count)
        end = g_line_off[l + 1] - 1; /* strip \n */
    else
        end = g_buf_len;
    return (end > start) ? (end - start) : 0;
}

/* ── cursor ──────────────────────────────────────────────────────────────── */

static size_t g_row = 0;   /* line index in buffer */
static size_t g_col = 0;   /* byte column within line */
static size_t g_top = 0;   /* first visible line (scroll offset) */

/* clamp column to line */
static void clamp_col(void) {
    size_t len = line_len(g_row);
    if (len == 0) { g_col = 0; return; }
    if (g_col >= len) g_col = len - 1;
}

/* ── mode ────────────────────────────────────────────────────────────────── */

#define MODE_NORMAL  0
#define MODE_INSERT  1
#define MODE_COMMAND 2

static uint8_t g_mode = MODE_NORMAL;

/* ── filename ────────────────────────────────────────────────────────────── */

static char8_t g_filename[1024];
static uint8_t g_dirty = 0;

/* ── status/command bar ──────────────────────────────────────────────────── */

static char8_t g_status[256];
static size_t  g_status_len = 0;

/* command line buffer (for : commands) */
static char8_t g_cmdline[256];
static size_t  g_cmdline_len = 0;

/* ── output write buffer ─────────────────────────────────────────────────── */

#define OUT_MAX (1024 * 64)
static char8_t  g_out[OUT_MAX];
static size_t   g_out_len = 0;

static void out_flush(void) {
    if (g_out_len) {
        write(1, g_out, g_out_len);
        g_out_len = 0;
    }
}

static void out_bytes(const char8_t *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (g_out_len == OUT_MAX) out_flush();
        g_out[g_out_len++] = s[i];
    }
}

static void out_str(const char8_t *s) {
    out_bytes(s, strlen8(s));
}

static void out_byte(char8_t c) {
    if (g_out_len == OUT_MAX) out_flush();
    g_out[g_out_len++] = c;
}

/* ESC sequence helpers */
static void out_csi(const char8_t *seq) {
    out_byte(0x1b);
    out_byte('[');
    out_str(seq);
}

static void cursor_goto(uint16_t row, uint16_t col) {
    /* ESC[row;colH  (1-based) */
    char8_t rbuf[22], cbuf[22];
    char8_t *rs = u64toa((uint64_t)row + 1, rbuf);
    char8_t *cs = u64toa((uint64_t)col + 1, cbuf);
    out_byte(0x1b); out_byte('[');
    out_str(rs); out_byte(';'); out_str(cs); out_byte('H');
}

/* ── file I/O ────────────────────────────────────────────────────────────── */

static void load_file(const char8_t *path) {
    int32_t fd;
    int64_t n;
    int64_t size;
    size_t  pathlen = strlen8((const char8_t *)path);

    if (pathlen >= sizeof(g_filename)) pathlen = sizeof(g_filename) - 1;
    memset8(g_filename, 0, sizeof(g_filename));
    memcpy8(g_filename, (const char8_t *)path, pathlen);

    fd = open((const char *)path, O_RDONLY, 0);
    if (fd < 0) {
        /* new file */
        g_buf_len = 0;
        rebuild_lines();
        return;
    }

    size = file_size(fd);
    if (size > 0) {
        size_t total = 0;
        buf_ensure((size_t)size);
        while (total < (size_t)size) {
            n = read(fd, g_buf + total, (size_t)size - total);
            if (n <= 0) break;
            total += (size_t)n;
        }
        g_buf_len = total;
    }
    close(fd);
    rebuild_lines();
    g_dirty = 0;
}

static void save_file(void) {
    int32_t fd;
    size_t  written = 0;
    int64_t n;

    if (!g_filename[0]) {
        memcpy8(g_status, (const char8_t *)"No filename", 11);
        g_status_len = 11;
        return;
    }

    fd = open((const char *)g_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        memcpy8(g_status, (const char8_t *)"Can't write file", 16);
        g_status_len = 16;
        return;
    }

    while (written < g_buf_len) {
        n = write(fd, g_buf + written, g_buf_len - written);
        if (n <= 0) break;
        written += (size_t)n;
    }
    close(fd);
    g_dirty = 0;

    {
        char8_t nbuf[22];
        char8_t *ns = u64toa((uint64_t)written, nbuf);
        size_t   fnlen = strlen8(g_filename);
        g_status_len = 0;
        memcpy8(g_status + g_status_len, g_filename, fnlen);
        g_status_len += fnlen;
        memcpy8(g_status + g_status_len, (const char8_t *)" written ", 9);
        g_status_len += 9;
        memcpy8(g_status + g_status_len, ns, strlen8(ns));
        g_status_len += strlen8(ns);
        memcpy8(g_status + g_status_len, (const char8_t *)"B", 1);
        g_status_len += 1;
    }
}

/* ── screen render ───────────────────────────────────────────────────────── */

static void render(void) {
    uint16_t screen_rows = (uint16_t)(g_rows - 1); /* reserve last row for status */
    uint16_t r;
    size_t   line_idx;

    /* hide cursor during render to avoid flicker */
    out_csi((const char8_t *)"?25l");

    /* move to top-left */
    cursor_goto(0, 0);

    for (r = 0; r < screen_rows; r++) {
        line_idx = g_top + r;

        out_csi((const char8_t *)"2K"); /* erase line */

        if (line_idx < g_line_count) {
            size_t start = g_line_off[line_idx];
            size_t len   = line_len(line_idx);
            size_t draw  = len;
            if (draw > g_cols) draw = g_cols;
            out_bytes(g_buf + start, draw);
        } else {
            out_byte('~');
        }

        out_byte('\r');
        out_byte('\n');
    }

    /* ── status bar ── */
    out_csi((const char8_t *)"2K");

    if (g_mode == MODE_COMMAND) {
        out_byte(':');
        out_bytes(g_cmdline, g_cmdline_len);
    } else {
        /* mode indicator */
        if (g_mode == MODE_INSERT) {
            out_csi((const char8_t *)"7m"); /* reverse video */
            out_str((const char8_t *)" INSERT ");
            out_csi((const char8_t *)"m");
            out_byte(' ');
        } else {
            out_csi((const char8_t *)"7m");
            out_str((const char8_t *)" NORMAL ");
            out_csi((const char8_t *)"m");
            out_byte(' ');
        }

        /* status message */
        if (g_status_len) {
            out_bytes(g_status, g_status_len);
        } else {
            /* file info */
            out_str(g_filename[0] ? g_filename
                                  : (const char8_t *)"[No Name]");
            if (g_dirty) out_str((const char8_t *)" [+]");
        }

        /* right side: row/col, only drawn if it actually fits --
         * the previous version computed a "pad" for this check but
         * never used it before unconditionally jumping the cursor,
         * so on a narrow terminal the unsigned subtraction below
         * could underflow to a huge column value. */
        {
            char8_t rbuf[22], cbuf[22];
            char8_t *rs = u64toa((uint64_t)g_row + 1, rbuf);
            char8_t *cs = u64toa((uint64_t)g_col + 1, cbuf);
            size_t info_len = strlen8(rs) + 1 + strlen8(cs);

            if ((size_t)g_cols > info_len + 1) {
                cursor_goto((uint16_t)(g_rows - 1),
                            (uint16_t)((size_t)g_cols - info_len - 1));
                out_str(rs); out_byte(':'); out_str(cs);
            }
        }
    }

    /* place physical cursor */
    if (g_mode == MODE_COMMAND) {
        cursor_goto((uint16_t)(g_rows - 1),
                    (uint16_t)(1 + g_cmdline_len));
    } else {
        size_t vis_row = g_row - g_top;
        size_t vis_col = g_col;
        cursor_goto((uint16_t)vis_row, (uint16_t)vis_col);
    }

    out_csi((const char8_t *)"?25h"); /* show cursor */
    out_flush();
}

/* ── buffer mutation helpers ─────────────────────────────────────────────── */

/* byte offset of cursor */
static size_t cursor_offset(void) {
    size_t off = g_line_off[g_row] + g_col;
    if (off > g_buf_len) off = g_buf_len;
    return off;
}

/* insert one byte at offset */
static void buf_insert(size_t off, char8_t c) {
    buf_ensure(g_buf_len + 1);
    memmove8(g_buf + off + 1, g_buf + off, g_buf_len - off);
    g_buf[off] = c;
    g_buf_len++;
    rebuild_lines();
    g_dirty = 1;
}

/* delete one byte at offset */
static void buf_delete(size_t off) {
    if (off >= g_buf_len) return;
    memmove8(g_buf + off, g_buf + off + 1, g_buf_len - off - 1);
    g_buf_len--;
    rebuild_lines();
    g_dirty = 1;
}

/* delete entire line l */
static void delete_line(size_t l) {
    size_t start, end, len;
    if (l >= g_line_count) return;
    start = g_line_off[l];
    if (l + 1 < g_line_count)
        end = g_line_off[l + 1];
    else
        end = g_buf_len;
    len = end - start;
    if (len == 0) return;
    memmove8(g_buf + start, g_buf + end, g_buf_len - end);
    g_buf_len -= len;
    rebuild_lines();
    g_dirty = 1;
}

/* ── scroll adjustment ───────────────────────────────────────────────────── */

static void adjust_scroll(void) {
    uint16_t screen_rows = (uint16_t)(g_rows - 1);
    if (g_row < g_top) g_top = g_row;
    if (g_row >= g_top + screen_rows)
        g_top = g_row - screen_rows + 1;
}

/* ── command execution ───────────────────────────────────────────────────── */

static uint8_t g_running = 1;
static uint8_t g_force_quit = 0;

/* one-byte pushback for input read during ESC-sequence disambiguation */
static char8_t g_pushback = 0;
static uint8_t g_has_pushback = 0;

static void exec_command(void) {
    char8_t *cmd = g_cmdline;

    if (streq(cmd, (const char8_t *)"q")) {
        if (g_dirty && !g_force_quit) {
            memcpy8(g_status,
                    (const char8_t *)"unsaved changes  :q! to force",
                    29);
            g_status_len = 29;
        } else {
            g_running = 0;
        }
    } else if (streq(cmd, (const char8_t *)"q!")) {
        g_running = 0;
    } else if (streq(cmd, (const char8_t *)"w")) {
        save_file();
    } else if (streq(cmd, (const char8_t *)"wq") ||
               streq(cmd, (const char8_t *)"x")) {
        save_file();
        g_running = 0;
    } else if (cmd[0] == 'w' && cmd[1] == ' ' && cmd[2]) {
        /* :w filename */
        size_t len = strlen8(cmd + 2);
        if (len >= sizeof(g_filename)) len = sizeof(g_filename) - 1;
        memcpy8(g_filename, cmd + 2, len);
        g_filename[len] = '\0';
        save_file();
    } else {
        memcpy8(g_status, (const char8_t *)"unknown command", 15);
        g_status_len = 15;
    }
}

/* ── key handling ────────────────────────────────────────────────────────── */

/* pending 'g' for gg, pending 'd' for dd */
static char8_t g_pending = 0;

static void handle_normal(char8_t c) {
    size_t llen;

    /* clear status on any key */
    g_status_len = 0;

    if (g_pending == 'd') {
        g_pending = 0;
        if (c == 'd') {
            delete_line(g_row);
            if (g_row >= g_line_count && g_line_count > 0)
                g_row = g_line_count - 1;
            clamp_col();
            adjust_scroll();
        }
        return;
    }

    if (g_pending == 'g') {
        g_pending = 0;
        if (c == 'g') {
            g_row = 0; g_col = 0; g_top = 0;
        }
        return;
    }

    switch (c) {
    /* ── motion ── */
    case 'h':
        if (g_col > 0) g_col--;
        break;
    case 'l':
        llen = line_len(g_row);
        if (llen > 0 && g_col + 1 < llen) g_col++;
        break;
    case 'j':
        if (g_row + 1 < g_line_count) {
            g_row++;
            clamp_col();
            adjust_scroll();
        }
        break;
    case 'k':
        if (g_row > 0) {
            g_row--;
            clamp_col();
            adjust_scroll();
        }
        break;
    case '0':
        g_col = 0;
        break;
    case '$': {
        size_t ll = line_len(g_row);
        g_col = ll > 0 ? ll - 1 : 0;
        break;
    }
    case 'G':
        if (g_line_count > 0) {
            g_row = g_line_count - 1;
            clamp_col();
            adjust_scroll();
        }
        break;
    case 'g':
        g_pending = 'g';
        break;

    /* ── mode switches ── */
    case 'i':
        g_mode = MODE_INSERT;
        break;
    case 'a': {
        size_t ll = line_len(g_row);
        g_mode = MODE_INSERT;
        if (ll > 0) g_col++;
        break;
    }
    case 'A': {
        size_t ll = line_len(g_row);
        g_mode = MODE_INSERT;
        g_col = ll;
        break;
    }
    case 'I':
        g_col = 0;
        g_mode = MODE_INSERT;
        break;
    case 'o': {
        /* open new line below.
         * NOTE: rebuild_lines() always records at least one line
         * (even for an empty buffer), so g_line_count is never 0
         * here -- no special-case branch needed for that. */
        size_t off = g_line_off[g_row] + line_len(g_row);
        if (off < g_buf_len && g_buf[off] == '\n') off++;
        buf_insert(off, '\n');
        g_row++;
        g_col = 0;
        adjust_scroll();
        g_mode = MODE_INSERT;
        break;
    }
    case 'O': {
        /* open new line above */
        size_t off = g_line_off[g_row];
        buf_insert(off, '\n');
        g_col = 0;
        adjust_scroll();
        g_mode = MODE_INSERT;
        break;
    }

    /* ── delete ── */
    case 'x': {
        size_t ll = line_len(g_row);
        if (ll > 0) {
            buf_delete(cursor_offset());
            clamp_col();
        }
        break;
    }
    case 'd':
        g_pending = 'd';
        break;

    /* ── command mode ── */
    case ':':
        g_mode = MODE_COMMAND;
        g_cmdline_len = 0;
        memset8(g_cmdline, 0, sizeof(g_cmdline));
        break;

    /* ── page movement ── */
    case 0x06: /* Ctrl-F */
        {
            uint16_t half = (uint16_t)((g_rows - 1) / 2);
            g_row += half;
            if (g_row >= g_line_count && g_line_count > 0)
                g_row = g_line_count - 1;
            clamp_col();
            adjust_scroll();
        }
        break;
    case 0x02: /* Ctrl-B */
        {
            uint16_t half = (uint16_t)((g_rows - 1) / 2);
            if (g_row >= half) g_row -= half; else g_row = 0;
            clamp_col();
            adjust_scroll();
        }
        break;

    default:
        break;
    }
}

static void handle_insert(char8_t c) {
    if (c == 0x1b) { /* ESC -> back to normal */
        if (g_col > 0) g_col--;
        clamp_col();
        g_mode = MODE_NORMAL;
        return;
    }

    if (c == 127 || c == 8) { /* backspace */
        size_t off = cursor_offset();
        if (off > 0) {
            /* if we're merging into the previous line, capture the
             * join column *before* buf_delete rebuilds the line
             * index -- otherwise line_len(g_row) below would measure
             * the post-merge (combined) line instead of the actual
             * join point. */
            uint8_t merging_up = (uint8_t)(g_col == 0 && g_row > 0);
            size_t  join_col = 0;
            if (merging_up) join_col = line_len(g_row - 1);

            buf_delete(off - 1);

            if (!merging_up) {
                g_col--;
            } else {
                g_row--;
                g_col = join_col;
                adjust_scroll();
            }
        }
        return;
    }

    if (c == '\n' || c == '\r') {
        size_t off = cursor_offset();
        buf_insert(off, '\n');
        g_row++;
        g_col = 0;
        adjust_scroll();
        return;
    }

    /* regular printable */
    if (c >= 32 && c < 127) {
        size_t off = cursor_offset();
        buf_insert(off, c);
        g_col++;
    }
}

static void handle_command(char8_t c) {
    if (c == 0x1b) {
        g_mode = MODE_NORMAL;
        g_cmdline_len = 0;
        return;
    }
    if (c == '\n' || c == '\r') {
        g_cmdline[g_cmdline_len] = '\0';
        exec_command();
        g_mode = MODE_NORMAL;
        g_cmdline_len = 0;
        return;
    }
    if ((c == 127 || c == 8) && g_cmdline_len > 0) {
        g_cmdline_len--;
        return;
    }
    if (c >= 32 && c < 127 && g_cmdline_len + 1 < (size_t)sizeof(g_cmdline)) {
        g_cmdline[g_cmdline_len++] = c;
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */

ABI int main(int argc, char **argv) {
    char8_t byte;

    get_winsize();

    if (argc >= 2) {
        load_file((const char8_t *)argv[1]);
    } else {
        g_buf_len = 0;
        rebuild_lines();
    }

    raw_on();
    out_csi((const char8_t *)"2J"); /* clear screen */
    render();

    while (g_running) {
        int64_t n;

        if (g_has_pushback) {
            byte = g_pushback;
            g_has_pushback = 0;
        } else {
            n = read(0, &byte, 1);
            if (n <= 0) break;
        }

        /* disambiguate a bare ESC keypress from the start of an arrow-key
         * escape sequence (ESC [ A/B/C/D). a fixed-length blind read here
         * used to unconditionally consume one more byte and discard it
         * whenever it wasn't part of a sequence -- that ate real
         * keystrokes typed right after ESC (e.g. hitting ESC then ':'
         * fast enough that ':' got silently swallowed), and a bare ESC
         * with nothing following would block forever waiting for a byte
         * that was never coming. poll() with a short timeout tells us
         * whether more input is actually queued before we try to read
         * it, and anything read that turns out not to be part of a
         * sequence gets pushed back instead of thrown away. */
        if (byte == 0x1b) {
            if (input_ready(20)) {
                char8_t next;
                n = read(0, &next, 1);
                if (n > 0 && next == '[' && input_ready(20)) {
                    char8_t code;
                    n = read(0, &code, 1);
                    if (n > 0) {
                        /* translate arrow keys to hjkl */
                        if      (code == 'A') byte = 'k';
                        else if (code == 'B') byte = 'j';
                        else if (code == 'C') byte = 'l';
                        else if (code == 'D') byte = 'h';
                        else                  byte = 0x1b; /* unrecognized seq, treat as bare ESC */
                    }
                } else if (n > 0) {
                    /* not an escape sequence -- the byte we peeked is a
                     * real keystroke, don't drop it. */
                    g_pushback = next;
                    g_has_pushback = 1;
                    byte = 0x1b;
                }
            }
            /* else: nothing followed within the timeout, genuinely bare ESC */

            if (byte == 0x1b) {
                if (g_mode == MODE_INSERT)
                    handle_insert(0x1b);
                else if (g_mode == MODE_COMMAND)
                    handle_command(0x1b);
                render();
                continue;
            }
        }

        switch (g_mode) {
        case MODE_NORMAL:  handle_normal(byte);  break;
        case MODE_INSERT:  handle_insert(byte);  break;
        case MODE_COMMAND: handle_command(byte); break;
        default:                                 break;
        }

        render();
    }

    raw_off();
    out_csi((const char8_t *)"2J");
    cursor_goto(0, 0);
    out_flush();
    exit(0);
}
