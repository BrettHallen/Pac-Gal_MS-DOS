/* term_main.c - text-mode terminal front end for the PAC-GAL (Al J. Jimenez, May 1982) remake.
 * Remake using Claude Opus 5.5, Nov 2026
 * Guided by Brett Hallen
 *
 * Plays the game in any terminal of at least 80x25 using ANSI escape codes,
 * so it needs no libraries: just a C compiler.  POSIX systems (macOS, Linux,
 * BSD) use termios; Windows 10+ uses the console API with VT processing.
 *
 * If PACGAL_TEXT_SDL_AUDIO is defined (CMake does this when SDL3 is
 * available) the PC-speaker sound is played through SDL3's audio; otherwise
 * the game is silent.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L    /* clock_gettime, select */
#define _DEFAULT_SOURCE            /* glibc: TIOCGWINSZ, SIGWINCH */
#define _DARWIN_C_SOURCE           /* macOS: the same */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cp437_unicode.h"
#include "game.h"

#ifdef PACGAL_TEXT_SDL_AUDIO
#include <SDL3/SDL.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

/* ---- keys ------------------------------------------------------------------ */
enum { K_NONE = -1, K_UP = 1000, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_BACKSPACE, K_ESC, K_F3 };

/* ---- output buffer ----------------------------------------------------------- */
static char obuf[1 << 17];
static size_t olen;

static void out(const char *s)
{
    size_t n = strlen(s);
    if (olen + n >= sizeof obuf) { fwrite(obuf, 1, olen, stdout); olen = 0; }
    memcpy(obuf + olen, s, n);
    olen += n;
}

static void outf(const char *fmt, int a, int b)
{
    char t[32];
    snprintf(t, sizeof t, fmt, a, b);
    out(t);
}

static void flush_out(void)
{
    fwrite(obuf, 1, olen, stdout);
    olen = 0;
    fflush(stdout);
}

/* ---- platform layer ------------------------------------------------------------ */
static volatile int quit_signal = 0;
static volatile int resized = 1;

#if defined(_WIN32)
static HANDLE h_in, h_out;
static DWORD old_in_mode, old_out_mode;
static UINT old_cp;

static double now_ms(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static void term_size(int *w, int *h)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    *w = 80; *h = 25;
    if (GetConsoleScreenBufferInfo(h_out, &info)) {
        *w = info.srWindow.Right - info.srWindow.Left + 1;
        *h = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

static BOOL WINAPI on_ctrl(DWORD type) { (void)type; quit_signal = 1; return TRUE; }

static int term_init(void)
{
    h_in = GetStdHandle(STD_INPUT_HANDLE);
    h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(h_in, &old_in_mode) || !GetConsoleMode(h_out, &old_out_mode)) {
        fprintf(stderr, "PAC-GAL needs to run in a console window.\n");
        return 0;
    }
    if (!SetConsoleMode(h_out, old_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT)) {
        fprintf(stderr, "This console does not support ANSI escape codes (Windows 10 or later needed).\n");
        return 0;
    }
    SetConsoleMode(h_in, ENABLE_WINDOW_INPUT);
    old_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(on_ctrl, TRUE);
    return 1;
}

static void term_restore(void)
{
    SetConsoleMode(h_in, old_in_mode);
    SetConsoleMode(h_out, old_out_mode);
    SetConsoleOutputCP(old_cp);
}

static int read_key(int timeout_ms)
{
    if (WaitForSingleObject(h_in, (DWORD)timeout_ms) != WAIT_OBJECT_0) return K_NONE;
    INPUT_RECORD rec;
    DWORD n = 0;
    while (GetNumberOfConsoleInputEvents(h_in, &n) && n > 0) {
        if (!ReadConsoleInputW(h_in, &rec, 1, &n) || n == 0) return K_NONE;
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) { resized = 1; continue; }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        switch (rec.Event.KeyEvent.wVirtualKeyCode) {
        case VK_UP: return K_UP;
        case VK_DOWN: return K_DOWN;
        case VK_LEFT: return K_LEFT;
        case VK_RIGHT: return K_RIGHT;
        case VK_RETURN: return K_ENTER;
        case VK_BACK: return K_BACKSPACE;
        case VK_ESCAPE: return K_ESC;
        case VK_F3: return K_F3;
        default: {
            WCHAR c = rec.Event.KeyEvent.uChar.UnicodeChar;
            if (c >= 32 && c < 127) return (int)c;
        }
        }
    }
    return K_NONE;
}

#else /* POSIX */
static struct termios old_tio;
static int have_tio = 0;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

static void term_size(int *w, int *h)
{
    struct winsize ws;
    *w = 80; *h = 25;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) { *w = ws.ws_col; *h = ws.ws_row; }
}

static void on_signal(int sig) { (void)sig; quit_signal = 1; }
static void on_winch(int sig) { (void)sig; resized = 1; }

static int term_init(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "PAC-GAL needs to run in a terminal.\n");
        return 0;
    }
    if (tcgetattr(STDIN_FILENO, &old_tio) == 0) {
        struct termios t = old_tio;
        t.c_lflag &= ~(ICANON | ECHO);      /* keep ISIG so Ctrl-C still quits */
        t.c_iflag &= ~(IXON | ICRNL);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
        have_tio = 1;
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGWINCH, on_winch);
    return 1;
}

static void term_restore(void)
{
    if (have_tio) tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

static int wait_byte(int timeout_ms)
{
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
}

static int read_key(int timeout_ms)
{
    int c = wait_byte(timeout_ms);
    if (c < 0) return K_NONE;
    if (c == 0x1B) {                       /* escape sequence or a lone Esc */
        int c1 = wait_byte(30);
        if (c1 < 0) return K_ESC;
        if (c1 != '[' && c1 != 'O') return K_NONE;
        int c2 = wait_byte(30), num = 0;
        while (c2 >= '0' && c2 <= '9') { num = num * 10 + (c2 - '0'); c2 = wait_byte(30); }
        while (c2 == ';' || (c2 >= '0' && c2 <= '9')) c2 = wait_byte(30);   /* modifiers */
        switch (c2) {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case 'R': return c1 == 'O' ? K_F3 : K_NONE;
        case '~': return num == 13 ? K_F3 : K_NONE;
        default: return K_NONE;
        }
    }
    if (c == '\r' || c == '\n') return K_ENTER;
    if (c == 0x7F || c == 0x08) return K_BACKSPACE;
    if (c >= 32 && c < 127) return c;
    return K_NONE;
}
#endif

/* ---- drawing ----------------------------------------------------------------------- */
typedef struct { unsigned char ch, fg, bg; } Disp;
static Disp shown[SCR_ROWS][SCR_COLS];
static int shown_valid = 0;
static int truecolor = 0;

static const unsigned cga_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};
static const int cga_to_ansi[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };   /* CGA order -> ANSI order */

static void set_colour(int fg, int bg)
{
    char t[64];
    if (truecolor) {
        unsigned f = cga_rgb[fg], b = cga_rgb[bg];
        snprintf(t, sizeof t, "\x1b[38;2;%u;%u;%u;48;2;%u;%u;%um",
                 f >> 16, (f >> 8) & 255, f & 255, b >> 16, (b >> 8) & 255, b & 255);
    } else {
        snprintf(t, sizeof t, "\x1b[%d;%dm",
                 fg < 8 ? 30 + cga_to_ansi[fg] : 90 + cga_to_ansi[fg - 8], 40 + cga_to_ansi[bg & 7]);
    }
    out(t);
}

static void draw(const Screen *s, int blink_off, int cursor_blink_on)
{
    int tw, th;
    term_size(&tw, &th);
    if (tw < SCR_COLS || th < SCR_ROWS) {
        out("\x1b[0m\x1b[2J\x1b[H\x1b[?25l");
        char t[96];
        snprintf(t, sizeof t, "PAC-GAL needs a terminal of at least 80x25 (this one is %dx%d).", tw, th);
        out(t);
        shown_valid = 0;
        return;
    }
    int top = (th - SCR_ROWS) / 2, left = (tw - SCR_COLS) / 2;
    if (resized || !shown_valid) {
        out("\x1b[0m\x1b[2J");
        memset(shown, 0xFF, sizeof shown);
        shown_valid = 1;
        resized = 0;
    }
    int cur_fg = -1, cur_bg = -1, cur_r = -1, cur_c = -1;
    for (int r = 0; r < SCR_ROWS; r++) {
        for (int c = 0; c < SCR_COLS; c++) {
            Cell cell = s->cell[r][c];
            Disp d;
            d.ch = cell.ch ? cell.ch : ' ';
            d.fg = cell.attr & 15;
            d.bg = (cell.attr >> 4) & 7;
            if ((cell.attr & 0x80) && blink_off) d.ch = ' ';
            if (d.ch == ' ') d.fg = d.bg;      /* blanks: only the background matters */
            Disp *o = &shown[r][c];
            if (o->ch == d.ch && o->fg == d.fg && o->bg == d.bg) continue;
            *o = d;
            /* position every changed cell, so no glyph width surprise can drift */
            if (r != cur_r || c != cur_c) outf("\x1b[%d;%dH", top + r + 1, left + c + 1);
            if (d.fg != cur_fg || d.bg != cur_bg) { set_colour(d.fg, d.bg); cur_fg = d.fg; cur_bg = d.bg; }
            out(cp437_utf8[d.ch]);
            cur_r = r;
            cur_c = c + 1;
        }
    }
    if (s->show_cursor && cursor_blink_on) {
        outf("\x1b[%d;%dH\x1b[?25h", top + s->row, left + s->col);
    } else {
        out("\x1b[?25l");
    }
}

/* ---- sound (optional) -------------------------------------------------------------- */
static Sound snd;
#ifdef PACGAL_TEXT_SDL_AUDIO
static SDL_AudioStream *stream;

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *st, int additional, int total)
{
    float buf[1024];
    (void)total;
    int frames = additional / (int)sizeof(float);
    while (frames > 0) {
        int n = frames < 1024 ? frames : 1024;
        snd_render((Sound *)userdata, buf, n);
        SDL_PutAudioStreamData(st, buf, n * (int)sizeof(float));
        frames -= n;
    }
}

static void audio_open(int mute)
{
    snd_init(&snd, 44100);
    snd.muted = mute;
    if (!SDL_Init(SDL_INIT_AUDIO)) return;
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, 44100 };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, &snd);
    if (stream) SDL_ResumeAudioStreamDevice(stream);
}
static void audio_lock(void) { if (stream) SDL_LockAudioStream(stream); }
static void audio_unlock(void) { if (stream) SDL_UnlockAudioStream(stream); else snd.head = snd.tail; }
static void audio_close(void) { if (stream) SDL_DestroyAudioStream(stream); SDL_Quit(); }
#else
static void audio_open(int mute) { snd_init(&snd, 8000); snd.muted = mute; }
static void audio_lock(void) {}
static void audio_unlock(void) { snd.head = snd.tail; }   /* no audio: discard notes */
static void audio_close(void) {}
#endif

/* ---- main --------------------------------------------------------------------------- */
static void usage(const char *argv0)
{
    printf("PAC-GAL (1982, Al J. Jimenez) - text-mode remake for the terminal\n\n"
           "usage: %s [options]\n"
           "  --speed N          skip the \"How fast\" prompt (0 fastest .. 30000 slowest)\n"
           "  --truecolor        exact CGA colours (for terminals with 24-bit colour)\n"
           "  --faithful-clock   use the original hh*360+mm*60+ss clock, bugs included\n"
#ifdef PACGAL_TEXT_SDL_AUDIO
           "  --mute             start with sound off (F3 toggles)\n"
#endif
           "\nGameplay (changes from 1982 by default: power pellets last 6+ seconds,\n"
           "ghosts wander 25%% of the time):\n"
           "  --original         exact 1982 rules (short power pellets, ghosts always chase)\n"
           "  --fright-min S     power pellets last at least S seconds (default 6)\n"
           "  --easy             same as --wander 0.3 --fright 2 --scared-ghosts\n"
           "  --wander P         chance (0-1) a ghost wanders instead of chasing (default 0.25)\n"
           "  --fright X         power pellets last X times as long\n"
           "  --scared-ghosts    frightened ghosts run away instead of chasing you\n"
           "\nkeys: arrows move, Esc quits.  The terminal must be at least 80x25.\n",
           argv0);
}

int main(int argc, char **argv)
{
    GameOptions gopt;
    int mute = 0;
    game_default_options(&gopt);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--speed") && i + 1 < argc) gopt.speed = atoi(argv[++i]);
        else if (!strcmp(a, "--truecolor")) truecolor = 1;
        else if (!strcmp(a, "--faithful-clock")) gopt.faithful_clock = 1;
        else if (!strcmp(a, "--mute")) mute = 1;
        else if (!strcmp(a, "--original")) { gopt.fright_min_s = 0; gopt.ghost_randomness = 0; }
        else if (!strcmp(a, "--fright-min") && i + 1 < argc) gopt.fright_min_s = atof(argv[++i]);
        else if (!strcmp(a, "--easy")) { gopt.ghost_randomness = 0.3; gopt.fright_scale = 2.0; gopt.scared_ghosts = 1; }
        else if (!strcmp(a, "--wander") && i + 1 < argc) gopt.ghost_randomness = atof(argv[++i]);
        else if (!strcmp(a, "--fright") && i + 1 < argc) gopt.fright_scale = atof(argv[++i]);
        else if (!strcmp(a, "--scared-ghosts")) gopt.scared_ghosts = 1;
        else { usage(argv[0]); return !strcmp(a, "--help") || !strcmp(a, "-h") ? 0 : 1; }
    }

    if (!term_init()) return 1;
    audio_open(mute);
    out("\x1b[?1049h\x1b[?25l\x1b[0m\x1b[2J");   /* alternate screen, hide cursor */
    flush_out();

    static Screen scr;
    static Game game;
    double start = now_ms();
    game_init(&game, &scr, &snd, &gopt, (unsigned)(start * 1000.0) ^ 0x5DEECE6u);

    while (!quit_signal && game.phase != GS_QUIT) {
        int k = read_key(5);
        audio_lock();
        for (; k != K_NONE; k = read_key(0)) {
            if (k == K_ESC) { quit_signal = 1; break; }
            switch (k) {
            case K_UP: game_arrow(&game, -1, 0); break;
            case K_DOWN: game_arrow(&game, 1, 0); break;
            case K_LEFT: game_arrow(&game, 0, -1); break;
            case K_RIGHT: game_arrow(&game, 0, 1); break;
            case K_ENTER: game_enter(&game); break;
            case K_BACKSPACE: game_backspace(&game); break;
            case K_F3: snd.muted = !snd.muted; break;
            default:
                if (k >= 32 && k < 127) { char t[2] = { (char)k, 0 }; game_text(&game, t); }
            }
            game_any_key(&game);
        }
        double now = now_ms() - start;
        game_update(&game, now);
        audio_unlock();

        long t = (long)now;
        draw(&scr, (t / 267) & 1, !((t / 133) & 1));
        flush_out();
    }

    int said_bye = game.phase == GS_QUIT;
    out("\x1b[0m\x1b[2J\x1b[?25h\x1b[?1049l");   /* back to the normal screen */
    if (said_bye) out("...Hope you had a good time\n");   /* as the original left it */
    flush_out();
    audio_close();
    term_restore();
    return 0;
}
