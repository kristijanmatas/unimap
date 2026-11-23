/*
 * unimap - a TUI Unicode character map (in the spirit of GNOME Character Map)
 *
 * Pure C, libc only (termios + ANSI escapes, no ncurses).
 *
 * Build:  cc -O2 -Wall -o unimap unimap.c
 * Run:    ./unimap        (needs a UTF-8 terminal and a font with the glyphs)
 *
 * Keys
 *   Tab / Shift-Tab    switch between block list and character grid
 *   arrows / hjkl      move
 *   PgUp / PgDn        page
 *   Home / End         start / end of row (grid) or first / last block (list)
 *   n / p  or ] / [    next / previous block
 *   Enter / Space      add the character to the text buffer (Enter in the
 *                      block list jumps to the grid)
 *   Backspace / x      delete last char / clear the text buffer
 *   c (or y)           copy buffer (or current char) to the clipboard
 *   g                  go to code point (hex)
 *   /                  find block by name
 *   mouse              click to select, double-click to add, wheel to scroll
 *   q / Ctrl-C         quit
 */
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <locale.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>

/* ------------------------------------------------------------------ */
/* Unicode blocks (starts are all 16-aligned)                          */
/* ------------------------------------------------------------------ */
typedef struct { uint32_t lo, hi; const char *name; } Block;

static const Block blocks[] = {
    {0x0000, 0x007F, "Basic Latin"},
    {0x0080, 0x00FF, "Latin-1 Supplement"},
    {0x0100, 0x017F, "Latin Extended-A"},
    {0x0180, 0x024F, "Latin Extended-B"},
    {0x0250, 0x02AF, "IPA Extensions"},
    {0x02B0, 0x02FF, "Spacing Modifier Letters"},
    {0x0300, 0x036F, "Combining Diacritical Marks"},
    {0x0370, 0x03FF, "Greek and Coptic"},
    {0x0400, 0x04FF, "Cyrillic"},
    {0x0500, 0x052F, "Cyrillic Supplement"},
    {0x0530, 0x058F, "Armenian"},
    {0x0590, 0x05FF, "Hebrew"},
    {0x0600, 0x06FF, "Arabic"},
    {0x0700, 0x074F, "Syriac"},
    {0x0780, 0x07BF, "Thaana"},
    {0x0900, 0x097F, "Devanagari"},
    {0x0980, 0x09FF, "Bengali"},
    {0x0A00, 0x0A7F, "Gurmukhi"},
    {0x0A80, 0x0AFF, "Gujarati"},
    {0x0B80, 0x0BFF, "Tamil"},
    {0x0C00, 0x0C7F, "Telugu"},
    {0x0C80, 0x0CFF, "Kannada"},
    {0x0D00, 0x0D7F, "Malayalam"},
    {0x0D80, 0x0DFF, "Sinhala"},
    {0x0E00, 0x0E7F, "Thai"},
    {0x0E80, 0x0EFF, "Lao"},
    {0x0F00, 0x0FFF, "Tibetan"},
    {0x1000, 0x109F, "Myanmar"},
    {0x10A0, 0x10FF, "Georgian"},
    {0x1100, 0x11FF, "Hangul Jamo"},
    {0x1200, 0x137F, "Ethiopic"},
    {0x13A0, 0x13FF, "Cherokee"},
    {0x1400, 0x167F, "Unified Canadian Aboriginal Syllabics"},
    {0x1680, 0x169F, "Ogham"},
    {0x16A0, 0x16FF, "Runic"},
    {0x1780, 0x17FF, "Khmer"},
    {0x1800, 0x18AF, "Mongolian"},
    {0x1E00, 0x1EFF, "Latin Extended Additional"},
    {0x1F00, 0x1FFF, "Greek Extended"},
    {0x2000, 0x206F, "General Punctuation"},
    {0x2070, 0x209F, "Superscripts and Subscripts"},
    {0x20A0, 0x20CF, "Currency Symbols"},
    {0x20D0, 0x20FF, "Combining Marks for Symbols"},
    {0x2100, 0x214F, "Letterlike Symbols"},
    {0x2150, 0x218F, "Number Forms"},
    {0x2190, 0x21FF, "Arrows"},
    {0x2200, 0x22FF, "Mathematical Operators"},
    {0x2300, 0x23FF, "Miscellaneous Technical"},
    {0x2400, 0x243F, "Control Pictures"},
    {0x2440, 0x245F, "Optical Character Recognition"},
    {0x2460, 0x24FF, "Enclosed Alphanumerics"},
    {0x2500, 0x257F, "Box Drawing"},
    {0x2580, 0x259F, "Block Elements"},
    {0x25A0, 0x25FF, "Geometric Shapes"},
    {0x2600, 0x26FF, "Miscellaneous Symbols"},
    {0x2700, 0x27BF, "Dingbats"},
    {0x27C0, 0x27EF, "Misc Mathematical Symbols-A"},
    {0x27F0, 0x27FF, "Supplemental Arrows-A"},
    {0x2800, 0x28FF, "Braille Patterns"},
    {0x2900, 0x297F, "Supplemental Arrows-B"},
    {0x2980, 0x29FF, "Misc Mathematical Symbols-B"},
    {0x2A00, 0x2AFF, "Supplemental Math Operators"},
    {0x2B00, 0x2BFF, "Misc Symbols and Arrows"},
    {0x2C00, 0x2C5F, "Glagolitic"},
    {0x2C60, 0x2C7F, "Latin Extended-C"},
    {0x2C80, 0x2CFF, "Coptic"},
    {0x2E00, 0x2E7F, "Supplemental Punctuation"},
    {0x2E80, 0x2EFF, "CJK Radicals Supplement"},
    {0x2F00, 0x2FDF, "Kangxi Radicals"},
    {0x3000, 0x303F, "CJK Symbols and Punctuation"},
    {0x3040, 0x309F, "Hiragana"},
    {0x30A0, 0x30FF, "Katakana"},
    {0x3100, 0x312F, "Bopomofo"},
    {0x3130, 0x318F, "Hangul Compatibility Jamo"},
    {0x3200, 0x32FF, "Enclosed CJK Letters and Months"},
    {0x3300, 0x33FF, "CJK Compatibility"},
    {0x3400, 0x4DBF, "CJK Unified Ideographs Ext-A"},
    {0x4DC0, 0x4DFF, "Yijing Hexagram Symbols"},
    {0x4E00, 0x9FFF, "CJK Unified Ideographs"},
    {0xA000, 0xA48F, "Yi Syllables"},
    {0xAC00, 0xD7AF, "Hangul Syllables"},
    {0xE000, 0xF8FF, "Private Use Area"},
    {0xF900, 0xFAFF, "CJK Compatibility Ideographs"},
    {0xFB00, 0xFB4F, "Alphabetic Presentation Forms"},
    {0xFB50, 0xFDFF, "Arabic Presentation Forms-A"},
    {0xFE00, 0xFE0F, "Variation Selectors"},
    {0xFE20, 0xFE2F, "Combining Half Marks"},
    {0xFE30, 0xFE4F, "CJK Compatibility Forms"},
    {0xFE50, 0xFE6F, "Small Form Variants"},
    {0xFE70, 0xFEFF, "Arabic Presentation Forms-B"},
    {0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms"},
    {0xFFF0, 0xFFFF, "Specials"},
    {0x10000, 0x1007F, "Linear B Syllabary"},
    {0x10330, 0x1034F, "Gothic"},
    {0x13000, 0x1342F, "Egyptian Hieroglyphs"},
    {0x1D400, 0x1D7FF, "Mathematical Alphanumeric Symbols"},
    {0x1F000, 0x1F02F, "Mahjong Tiles"},
    {0x1F030, 0x1F09F, "Domino Tiles"},
    {0x1F0A0, 0x1F0FF, "Playing Cards"},
    {0x1F100, 0x1F1FF, "Enclosed Alphanumeric Supplement"},
    {0x1F200, 0x1F2FF, "Enclosed Ideographic Supplement"},
    {0x1F300, 0x1F5FF, "Misc Symbols and Pictographs"},
    {0x1F600, 0x1F64F, "Emoticons"},
    {0x1F650, 0x1F67F, "Ornamental Dingbats"},
    {0x1F680, 0x1F6FF, "Transport and Map Symbols"},
    {0x1F700, 0x1F77F, "Alchemical Symbols"},
    {0x1F780, 0x1F7FF, "Geometric Shapes Extended"},
    {0x1F800, 0x1F8FF, "Supplemental Arrows-C"},
    {0x1F900, 0x1F9FF, "Supplemental Symbols/Pictographs"},
    {0x1FA00, 0x1FA6F, "Chess Symbols"},
    {0x1FA70, 0x1FAFF, "Symbols and Pictographs Ext-A"},
    {0x1FB00, 0x1FBFF, "Symbols for Legacy Computing"},
    {0x20000, 0x2A6DF, "CJK Unified Ideographs Ext-B"},
    {0xE0000, 0xE007F, "Tags"},
};
#define NBLOCKS ((int)(sizeof blocks / sizeof blocks[0]))

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
enum { KEY_UP = 1001, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END,
       KEY_PGUP, KEY_PGDN, KEY_DEL, KEY_BTAB, KEY_MOUSE };

#define MAXBUF 256

static struct termios orig_tio;
static int W = 80, H = 24;
static int blk = 0, btop = 0, top = 0;
static uint32_t cp = 'A';
static int focus = 1;                 /* 0 = block list, 1 = grid */
static uint32_t buf[MAXBUF];
static int nbuf = 0;
static char msg[160];
static int mode = 0;                  /* 0 normal, 1 goto, 2 search */
static char inp[32];
static int ninp = 0;
static int running = 1;
static int mb, mx, my;                /* last mouse event */

/* layout (recomputed in layout()) */
static int lw, gx0, gx, cw;

/* ------------------------------------------------------------------ */
/* Output buffer                                                       */
/* ------------------------------------------------------------------ */
static char *ob;
static size_t olen, ocap;

static void oadd(const char *s, size_t n)
{
    if (olen + n > ocap) {
        ocap = (olen + n) * 2 + 4096;
        ob = realloc(ob, ocap);
        if (!ob) _exit(1);
    }
    memcpy(ob + olen, s, n);
    olen += n;
}
static void oputs(const char *s) { oadd(s, strlen(s)); }
static void ofmt(const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof tmp) n = sizeof tmp - 1;
    oadd(tmp, (size_t)n);
}
static void wr(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t r = write(1, s, n);
        if (r <= 0) return;
        s += r;
        n -= (size_t)r;
    }
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static int u8enc(uint32_t c, char *o)
{
    if (c < 0x80)    { o[0] = (char)c; return 1; }
    if (c < 0x800)   { o[0] = (char)(0xC0 | (c >> 6));
                       o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xE0 | (c >> 12));
                       o[1] = (char)(0x80 | ((c >> 6) & 0x3F));
                       o[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | (c >> 18));
    o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((c >> 6) & 0x3F));
    o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

/* Safe to print raw / put into the text buffer? */
static int safe_cp(uint32_t c)
{
    if (c < 0x20 || (c >= 0x7F && c < 0xA0)) return 0;
    if (c >= 0xD800 && c <= 0xDFFF) return 0;
    if ((c & 0xFFFE) == 0xFFFE) return 0;
    if (c >= 0xFDD0 && c <= 0xFDEF) return 0;
    return c <= 0x10FFFF;
}

/* Fill out[] with what to draw for c. Returns 1 normal, 2 control
 * picture, 0 not displayable. *w = display width (0 = combining). */
static int glyph(uint32_t c, char *out, int *w)
{
    if (c < 0x20) { out[u8enc(0x2400 + c, out)] = 0; *w = 1; return 2; }
    if (c == 0x7F) { out[u8enc(0x2421, out)] = 0; *w = 1; return 2; }
    if (!safe_cp(c)) { strcpy(out, "\xC2\xB7"); *w = 1; return 0; }
    int ww = wcwidth((wchar_t)c);
    if (ww < 0 || ww > 2) {
        if ((c >= 0xE000 && c <= 0xF8FF) || c >= 0xF0000) ww = 1; /* PUA */
        else { strcpy(out, "\xC2\xB7"); *w = 1; return 0; }
    }
    out[u8enc(c, out)] = 0;
    *w = ww;
    return 1;
}

static const char *classify(uint32_t c)
{
    if (c < 0x20 || (c >= 0x7F && c < 0xA0)) return "control";
    if (!safe_cp(c)) return "non-character";
    wint_t w = (wint_t)c;
    if (iswalpha(w)) return "letter";
    if (iswdigit(w)) return "digit";
    if (iswspace(w)) return "space";
    if (iswpunct(w)) return "punct/symbol";
    if (iswprint(w)) return "other";
    return "unassigned/other";
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void setmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Terminal                                                            */
/* ------------------------------------------------------------------ */
static void term_restore(void)
{
    static int done;
    if (done) return;
    done = 1;
    const char *s = "\x1b[0m\x1b[?1000l\x1b[?1006l\x1b[?7h\x1b[?25h\x1b[?1049l";
    wr(s, strlen(s));
    tcsetattr(0, TCSAFLUSH, &orig_tio);
}
static void on_signal(int sig)
{
    (void)sig;
    term_restore();
    _exit(1);
}

static int get_size(void)
{
    struct winsize ws;
    int w = 80, h = 24;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        w = ws.ws_col;
        h = ws.ws_row;
    }
    if (w == W && h == H) return 0;
    W = w;
    H = h;
    return 1;
}

static int rows(void) { int r = H - 6; return r < 1 ? 1 : r; }

static void layout(void)
{
    lw  = W >= 100 ? 30 : 22;
    gx0 = lw + 2;
    gx  = gx0 + 8;
    cw  = (W - gx + 1 >= 64) ? 4 : 3;
}

/* ------------------------------------------------------------------ */
/* Navigation                                                          */
/* ------------------------------------------------------------------ */
static void fix_view(void)
{
    int nr = rows();
    uint32_t lo = blocks[blk].lo, hi = blocks[blk].hi;
    int row = (int)((cp - lo) / 16);
    int total = (int)((hi - lo) / 16) + 1;
    if (row < top) top = row;
    if (row >= top + nr) top = row - nr + 1;
    if (top > total - nr) top = total - nr;
    if (top < 0) top = 0;
    if (blk < btop) btop = blk;
    if (blk >= btop + nr) btop = blk - nr + 1;
    if (btop > NBLOCKS - nr) btop = NBLOCKS - nr;
    if (btop < 0) btop = 0;
}

static void set_block(int i)
{
    if (i < 0) i = 0;
    if (i >= NBLOCKS) i = NBLOCKS - 1;
    blk = i;
    cp = blocks[i].lo;
    top = 0;
    fix_view();
}

static void move_cp(long d)
{
    long n = (long)cp + d;
    if (n < (long)blocks[blk].lo) n = blocks[blk].lo;
    if (n > (long)blocks[blk].hi) n = blocks[blk].hi;
    cp = (uint32_t)n;
    fix_view();
}

static void move_v(int dir)
{
    uint32_t lo = blocks[blk].lo, hi = blocks[blk].hi;
    long n = (long)cp + dir * 16L;
    if (n < (long)lo) return;
    if (n > (long)hi) {
        if ((cp - lo) / 16 < (hi - lo) / 16) n = hi;
        else return;
    }
    cp = (uint32_t)n;
    fix_view();
}

static int goto_cp(uint32_t c)
{
    for (int i = 0; i < NBLOCKS; i++) {
        if (c >= blocks[i].lo && c <= blocks[i].hi) {
            blk = i;
            cp = c;
            top = 0;
            focus = 1;
            fix_view();
            return 1;
        }
    }
    return 0;
}

static int ci_contains(const char *h, const char *n)
{
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            a++;
            b++;
        }
        if (!*b) return 1;
    }
    return 0;
}

static void search_block(const char *needle)
{
    for (int k = 1; k <= NBLOCKS; k++) {
        int i = (blk + k) % NBLOCKS;
        if (ci_contains(blocks[i].name, needle)) {
            set_block(i);
            return;
        }
    }
    setmsg("No block matching \"%s\"", needle);
}

/* ------------------------------------------------------------------ */
/* Text buffer + clipboard                                             */
/* ------------------------------------------------------------------ */
static size_t buf_utf8(char *out)
{
    size_t n = 0;
    for (int i = 0; i < nbuf; i++) n += (size_t)u8enc(buf[i], out + n);
    out[n] = 0;
    return n;
}

static void add_char(void)
{
    if (!safe_cp(cp)) { setmsg("U+%04X can't be added to the text", cp); return; }
    if (nbuf >= MAXBUF) { setmsg("Text buffer full"); return; }
    buf[nbuf++] = cp;
}

static void b64(const unsigned char *s, size_t n, char *o)
{
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t j = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)s[i] << 16;
        if (i + 1 < n) v |= (unsigned)s[i + 1] << 8;
        if (i + 2 < n) v |= s[i + 2];
        o[j++] = t[(v >> 18) & 63];
        o[j++] = t[(v >> 12) & 63];
        o[j++] = (i + 1 < n) ? t[(v >> 6) & 63] : '=';
        o[j++] = (i + 2 < n) ? t[v & 63] : '=';
    }
    o[j] = 0;
}

static void do_copy(void)
{
    char text[MAXBUF * 4 + 8];
    size_t n;
    int count;
    if (nbuf > 0) {
        n = buf_utf8(text);
        count = nbuf;
    } else if (safe_cp(cp)) {
        n = (size_t)u8enc(cp, text);
        text[n] = 0;
        count = 1;
    } else {
        setmsg("Nothing to copy");
        return;
    }

    /* 1) OSC 52 (works in most modern terminals, also over SSH) */
    char *e = malloc(((n + 2) / 3) * 4 + 1);
    if (e) {
        b64((unsigned char *)text, n, e);
        wr("\x1b]52;c;", 7);
        wr(e, strlen(e));
        wr("\x07", 1);
        free(e);
    }

    /* 2) external helpers */
    static const char *cmds[] = {
        "wl-copy >/dev/null 2>&1",
        "xclip -selection clipboard >/dev/null 2>&1",
        "xsel -ib >/dev/null 2>&1",
        "pbcopy >/dev/null 2>&1",
        NULL
    };
    const char *used = NULL;
    for (int i = 0; cmds[i]; i++) {
        FILE *f = popen(cmds[i], "w");
        if (!f) continue;
        fwrite(text, 1, n, f);
        if (pclose(f) == 0) {
            used = cmds[i];
            break;
        }
    }
    if (used) {
        char name[16];
        snprintf(name, sizeof name, "%.*s", (int)strcspn(used, " "), used);
        setmsg("Copied %d char%s (%s + OSC 52)", count, count == 1 ? "" : "s", name);
    } else {
        setmsg("Copied %d char%s via OSC 52 (no wl-copy/xclip/xsel found)",
               count, count == 1 ? "" : "s");
    }
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static int rd(void)
{
    unsigned char c;
    return read(0, &c, 1) == 1 ? c : -1;
}

static int read_key(void)
{
    int c = rd();
    if (c < 0) return 0;
    if (c != 27) return c;
    int c1 = rd();
    if (c1 < 0) return 27;
    if (c1 == '[') {
        int c2 = rd();
        if (c2 < 0) return 0;
        if (c2 == '<') {                       /* SGR mouse */
            int v[3] = {0, 0, 0}, i = 0, ch;
            while ((ch = rd()) >= 0) {
                if (ch >= '0' && ch <= '9') v[i] = v[i] * 10 + (ch - '0');
                else if (ch == ';') { if (i < 2) i++; }
                else {
                    if (ch == 'M') { mb = v[0]; mx = v[1]; my = v[2]; return KEY_MOUSE; }
                    return 0;
                }
            }
            return 0;
        }
        switch (c2) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        case 'Z': return KEY_BTAB;
        }
        if (c2 >= '0' && c2 <= '9') {
            int n = c2 - '0', ch;
            while ((ch = rd()) >= '0' && ch <= '9') n = n * 10 + (ch - '0');
            switch (n) {
            case 1: case 7: return KEY_HOME;
            case 4: case 8: return KEY_END;
            case 5: return KEY_PGUP;
            case 6: return KEY_PGDN;
            case 3: return KEY_DEL;
            }
        }
        return 0;
    }
    if (c1 == 'O') {
        int c2 = rd();
        switch (c2) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        }
    }
    return 0;
}

static void handle_mouse(void)
{
    static long last_t;
    static uint32_t last_cp = 0xFFFFFFFF;
    int nr = rows();

    if (mb == 64 || mb == 65) {                /* wheel */
        int dir = (mb == 64) ? -1 : 1;
        if (mx <= lw) set_block(blk + dir);
        else { focus = 1; move_cp(dir * 48L); }
        return;
    }
    if (mb != 0) return;                       /* left button only */
    if (my < 3 || my > 2 + nr) return;

    if (mx <= lw) {
        int idx = btop + (my - 3);
        if (idx < NBLOCKS) { set_block(idx); focus = 0; }
    } else if (mx >= gx && mx < gx + 16 * cw) {
        int col = (mx - gx) / cw;
        int row = top + (my - 3);
        uint32_t c = blocks[blk].lo + (uint32_t)row * 16 + (uint32_t)col;
        if (c > blocks[blk].hi) return;
        long t = now_ms();
        focus = 1;
        cp = c;
        fix_view();
        if (c == last_cp && t - last_t < 400) { add_char(); last_cp = 0xFFFFFFFF; }
        else { last_cp = c; last_t = t; }
    }
}

static void prompt_key(int k)
{
    if (k == 27) { mode = 0; return; }
    if (k == '\r' || k == '\n') {
        if (ninp > 0) {
            if (mode == 1) {
                unsigned long v = strtoul(inp, NULL, 16);
                if (v > 0x10FFFF) setmsg("Code point out of range");
                else if (!goto_cp((uint32_t)v))
                    setmsg("U+%04lX is not in any listed block", v);
            } else {
                search_block(inp);
            }
        }
        mode = 0;
        return;
    }
    if (k == 127 || k == 8) { if (ninp) inp[--ninp] = 0; return; }
    if (k >= 32 && k < 127 && ninp < (mode == 1 ? 6 : 28)) {
        if (mode == 1 && !isxdigit(k)) return;
        inp[ninp++] = (char)k;
        inp[ninp] = 0;
    }
}

static void handle_key(int k)
{
    int nr = rows();
    msg[0] = 0;
    if (mode) { prompt_key(k); return; }

    switch (k) {
    case 'q': case 3: running = 0; return;
    case '\t': case KEY_BTAB: focus ^= 1; return;
    case KEY_UP: case 'k':
        if (focus == 0) set_block(blk - 1); else move_v(-1);
        return;
    case KEY_DOWN: case 'j':
        if (focus == 0) set_block(blk + 1); else move_v(1);
        return;
    case KEY_LEFT: case 'h':
        if (focus == 1) { if (cp > blocks[blk].lo) move_cp(-1); else focus = 0; }
        return;
    case KEY_RIGHT: case 'l':
        if (focus == 0) focus = 1; else move_cp(1);
        return;
    case KEY_PGUP:
        if (focus == 0) set_block(blk - nr); else move_cp(-16L * nr);
        return;
    case KEY_PGDN:
        if (focus == 0) set_block(blk + nr); else move_cp(16L * nr);
        return;
    case KEY_HOME:
        if (focus == 0) set_block(0);
        else { cp = blocks[blk].lo + ((cp - blocks[blk].lo) & ~15u); fix_view(); }
        return;
    case KEY_END:
        if (focus == 0) set_block(NBLOCKS - 1);
        else {
            uint32_t e = blocks[blk].lo + ((cp - blocks[blk].lo) & ~15u) + 15;
            cp = e > blocks[blk].hi ? blocks[blk].hi : e;
            fix_view();
        }
        return;
    case 'n': case ']': set_block(blk + 1); return;
    case 'p': case '[': set_block(blk - 1); return;
    case '\r': case '\n':
        if (focus == 0) focus = 1; else add_char();
        return;
    case ' ': add_char(); return;
    case 127: case 8: case KEY_DEL: if (nbuf) nbuf--; return;
    case 'x': nbuf = 0; return;
    case 'c': case 'y': do_copy(); return;
    case 'g': mode = 1; ninp = 0; inp[0] = 0; return;
    case '/': mode = 2; ninp = 0; inp[0] = 0; return;
    case KEY_MOUSE: handle_mouse(); return;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void draw(void)
{
    int nr = rows(), i;
    const Block *b = &blocks[blk];
    char g[8], tmp[256];
    int w;

    layout();
    fix_view();
    olen = 0;
    oputs("\x1b[0m\x1b[?25l\x1b[H\x1b[2J");

    /* title bar */
    ofmt("\x1b[1;1H\x1b[7m%-*.*s\x1b[0m", W, W,
         " Unicode Map  -  pure C TUI character map");

    /* block list */
    snprintf(tmp, sizeof tmp, " Blocks %d/%d", blk + 1, NBLOCKS);
    ofmt("\x1b[2;1H\x1b[1m%-*.*s\x1b[0m", lw, lw, tmp);
    for (i = 0; i < nr; i++) {
        int idx = btop + i, y = 3 + i;
        ofmt("\x1b[%d;1H", y);
        if (idx < NBLOCKS) {
            if (idx == blk) oputs(focus == 0 ? "\x1b[7m" : "\x1b[1;4m");
            ofmt(" %-*.*s", lw - 1, lw - 1, blocks[idx].name);
            oputs("\x1b[0m");
        } else {
            ofmt("%*s", lw, "");
        }
    }
    for (i = 0; i <= nr; i++)
        ofmt("\x1b[%d;%dH\x1b[2m\xE2\x94\x82\x1b[0m", 2 + i, lw + 1);

    /* column header */
    oputs("\x1b[1m");
    for (i = 0; i < 16; i++) ofmt("\x1b[2;%dH%X", gx + i * cw + 1, i);
    oputs("\x1b[0m");

    /* grid */
    for (i = 0; i < nr; i++) {
        uint32_t rs = b->lo + (uint32_t)(top + i) * 16;
        int y = 3 + i;
        if (rs > b->hi) break;
        snprintf(tmp, sizeof tmp, "U+%03Xx", rs >> 4);
        ofmt("\x1b[%d;%dH\x1b[2m%-7s\x1b[0m", y, gx0, tmp);
        for (int col = 0; col < 16; col++) {
            uint32_t c = rs + (uint32_t)col;
            if (c > b->hi) break;
            int t = glyph(c, g, &w);
            int sel = (c == cp);
            ofmt("\x1b[%d;%dH", y, gx + col * cw);
            if (sel) oputs(focus == 1 ? "\x1b[7m" : "\x1b[1;4m");
            else if (t != 1) oputs("\x1b[2m");
            oputs(" ");
            int vis = w;
            if (w == 0) { oputs("\xE2\x97\x8C"); vis = 1; }   /* dotted circle */
            oputs(g);
            ofmt("%*s", cw - 1 - vis, "");
            oputs("\x1b[0m");
        }
    }

    /* info line */
    {
        char u8[8];
        int n = u8enc(cp, u8), t = glyph(cp, g, &w);
        char bytes[40] = "";
        for (i = 0; i < n; i++) {
            char one[5];
            snprintf(one, sizeof one, "%02X ", (unsigned char)u8[i]);
            strcat(bytes, one);
        }
        ofmt("\x1b[%d;1H\x1b[K\x1b[1m U+%04X\x1b[0m  %s%s  dec %u  UTF-8: %s %s  [%s]",
             H - 3, cp, w == 0 ? "\xE2\x97\x8C" : "", t == 1 ? g : "-",
             cp, bytes, b->name, classify(cp));
    }

    /* text buffer / prompt */
    ofmt("\x1b[%d;1H\x1b[K", H - 2);
    if (mode == 1) {
        ofmt("\x1b[1m Go to U+\x1b[0m%s_", inp);
    } else if (mode == 2) {
        ofmt("\x1b[1m Find block: \x1b[0m%s_", inp);
    } else {
        char out[MAXBUF * 4 + 8];
        int maxc = W - 10;
        int start = nbuf > maxc ? nbuf - maxc : 0;
        size_t n = 0;
        for (i = start; i < nbuf; i++) n += (size_t)u8enc(buf[i], out + n);
        out[n] = 0;
        ofmt("\x1b[1m Text:\x1b[0m %s\x1b[2m|\x1b[0m", out);
    }

    /* message + help */
    ofmt("\x1b[%d;1H\x1b[K\x1b[33m %s\x1b[0m", H - 1, msg);
    if (mode)
        ofmt("\x1b[%d;1H\x1b[7m%-*.*s\x1b[0m", H, W, W,
             " Enter = go   Esc = cancel");
    else
        ofmt("\x1b[%d;1H\x1b[7m%-*.*s\x1b[0m", H, W, W,
             " Tab pane  arrows/hjkl move  Enter add  c copy  Bksp del  x clear"
             "  g goto  / find  n/p block  q quit");

    wr(ob, olen);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    if (!isatty(0) || !isatty(1)) {
        fputs("unimap: must be run in a terminal\n", stderr);
        return 1;
    }

    setlocale(LC_ALL, "");
    if (!strstr(nl_langinfo(CODESET), "UTF-8")) {
        static const char *alt[] = {"C.UTF-8", "en_US.UTF-8", "en_GB.UTF-8", NULL};
        int ok = 0;
        for (int i = 0; alt[i] && !ok; i++)
            if (setlocale(LC_ALL, alt[i])) ok = 1;
        if (!ok) {
            fputs("unimap: a UTF-8 locale is required (try LANG=C.UTF-8)\n", stderr);
            return 1;
        }
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);

    tcgetattr(0, &orig_tio);
    struct termios raw = orig_tio;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= CS8;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;                       /* 100 ms read timeout */
    tcsetattr(0, TCSAFLUSH, &raw);
    atexit(term_restore);

    wr("\x1b[?1049h\x1b[?7l\x1b[?1000h\x1b[?1006h", 29);

    int dirty = 1;
    get_size();
    layout();
    fix_view();
    while (running) {
        if (get_size()) dirty = 1;
        if (dirty) { draw(); dirty = 0; }
        int k = read_key();
        if (!k) continue;
        handle_key(k);
        dirty = 1;
    }
    return 0;
}
