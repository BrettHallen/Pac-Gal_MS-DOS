/* main.c - SDL3 front end for the PAC-GAL recreation.
 *
 * Draws the 80x25 text screen with an 8x8 CP437 font, doubling every row
 * (640x400), which is what an IBM CGA card's 640x200 text looks like on a
 * 4:3 monitor.  Audio is a square wave, like the PC speaker.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font_cp437.h"
#include "game.h"

#define TEX_W (SCR_COLS * 8)
#define TEX_H (SCR_ROWS * 16)
#define AUDIO_RATE 44100

static const Uint32 cga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

typedef struct {
    int square_pixels;   /* 0 = 4:3 like a CRT, 1 = 640x400 square pixels */
    int fullscreen;
    int mute;
    const char *screenshot;
    long quit_after_ms;
} AppOptions;

/* Runs on SDL's audio thread whenever the device wants more samples, so the
 * sound never depends on the frame rate.  SDL holds the stream lock while
 * calling this; the main thread takes the same lock while it changes the
 * note queue. */
static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
    Sound *snd = (Sound *)userdata;
    float buf[1024];
    (void)total;
    int frames = additional / (int)sizeof(float);
    while (frames > 0) {
        int n = frames < 1024 ? frames : 1024;
        snd_render(snd, buf, n);
        SDL_PutAudioStreamData(stream, buf, n * (int)sizeof(float));
        frames -= n;
    }
}

static void usage(const char *argv0)
{
    printf("PAC-GAL (1982, Al J. Jimenez) - SDL3 recreation\n\n"
           "usage: %s [options]\n"
           "  --speed N          skip the \"How fast\" prompt (0 fastest .. 30000 slowest)\n"
           "  --faithful-clock   use the original hh*360+mm*60+ss clock, bugs included\n"
           "  --square-pixels    640x400 square pixels instead of a 4:3 CRT shape\n"
           "  --fullscreen       start full screen\n"
           "  --mute             start with sound off\n"
           "\nGameplay (the only change from 1982 by default: power pellets last 6+ seconds):\n"
           "  --original         exact 1982 rules (power pellets last ~13 ghost moves)\n"
           "  --fright-min S     power pellets last at least S seconds (default 6)\n"
           "  --easy             same as --wander 0.3 --fright 2 --scared-ghosts\n"
           "  --wander P         chance (0-1) a ghost wanders instead of chasing; turns on\n"
           "                     the author's unused difficulty curve\n"
           "  --fright X         power pellets last X times as long\n"
           "  --scared-ghosts    frightened ghosts run away instead of chasing you\n"
           "\nkeys: arrows move, F2 aspect, F3 sound on/off, F11 or Alt+Enter full screen, Esc quit\n",
           argv0);
}

static void render_text(const Screen *s, Uint32 *px, Uint64 ticks)
{
    int blink_off = (ticks / 267) & 1;            /* CGA blink: ~1.9 Hz */
    int cursor_on = s->show_cursor && !((ticks / 133) & 1);
    for (int r = 0; r < SCR_ROWS; r++) {
        for (int c = 0; c < SCR_COLS; c++) {
            Cell cell = s->cell[r][c];
            Uint32 fg = cga_palette[cell.attr & 15];
            Uint32 bg = cga_palette[(cell.attr >> 4) & 7];
            if ((cell.attr & 0x80) && blink_off) fg = bg;
            const unsigned char *gl = font_cp437[cell.ch];
            int is_cursor = cursor_on && r == s->row - 1 && c == s->col - 1;
            for (int y = 0; y < 8; y++) {
                unsigned bits = gl[y];
                if (is_cursor && y >= 6) bits = 0xFF;
                Uint32 *line0 = px + (r * 16 + y * 2) * TEX_W + c * 8;
                Uint32 *line1 = line0 + TEX_W;
                for (int x = 0; x < 8; x++) {
                    Uint32 v = (bits >> x) & 1 ? fg : bg;
                    line0[x] = v;
                    line1[x] = v;
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    GameOptions gopt;
    AppOptions aopt = { 0 };
    game_default_options(&gopt);

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--speed") && i + 1 < argc) gopt.speed = atoi(argv[++i]);
        else if (!strcmp(a, "--faithful-clock")) gopt.faithful_clock = 1;
        else if (!strcmp(a, "--square-pixels")) aopt.square_pixels = 1;
        else if (!strcmp(a, "--fullscreen")) aopt.fullscreen = 1;
        else if (!strcmp(a, "--mute")) aopt.mute = 1;
        else if (!strcmp(a, "--original")) gopt.fright_min_s = 0;
        else if (!strcmp(a, "--fright-min") && i + 1 < argc) gopt.fright_min_s = atof(argv[++i]);
        else if (!strcmp(a, "--easy")) { gopt.ghost_randomness = 0.3; gopt.fright_scale = 2.0; gopt.scared_ghosts = 1; }
        else if (!strcmp(a, "--wander") && i + 1 < argc) gopt.ghost_randomness = atof(argv[++i]);
        else if (!strcmp(a, "--fright") && i + 1 < argc) gopt.fright_scale = atof(argv[++i]);
        else if (!strcmp(a, "--scared-ghosts")) gopt.scared_ghosts = 1;
        else if (!strcmp(a, "--screenshot") && i + 1 < argc) aopt.screenshot = argv[++i];
        else if (!strcmp(a, "--quit-after") && i + 1 < argc) aopt.quit_after_ms = atol(argv[++i]);
        else { usage(argv[0]); return !strcmp(a, "--help") || !strcmp(a, "-h") ? 0 : 1; }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_APP_NAME, "PAC-GAL");

    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_WindowFlags wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (aopt.fullscreen) wflags |= SDL_WINDOW_FULLSCREEN;
    if (!SDL_CreateWindowAndRenderer("PAC-GAL", 1280, 960, wflags, &win, &ren)) {
        SDL_Log("Could not create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(win, 320, 240);
    if (!SDL_SetRenderVSync(ren, 1)) SDL_SetRenderVSync(ren, SDL_RENDERER_VSYNC_DISABLED);

    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, TEX_W, TEX_H);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_Texture *big = NULL;          /* integer-scaled copy for sharp-bilinear output */
    int big_k = 0;
    Uint32 *pixels = (Uint32 *)SDL_malloc(TEX_W * TEX_H * sizeof(Uint32));

    /* audio (optional: the game runs silently if there is no device) */
    static Sound snd;
    snd_init(&snd, AUDIO_RATE);
    snd.muted = aopt.mute;
    SDL_AudioStream *stream = NULL;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, AUDIO_RATE };
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, &snd);
        if (stream) SDL_ResumeAudioStreamDevice(stream);
        else SDL_Log("No audio: %s", SDL_GetError());
    }

    static Screen scr;
    static Game game;
    game_init(&game, &scr, &snd, &gopt, (unsigned)SDL_GetPerformanceCounter());
    SDL_StartTextInput(win);

    Uint64 start = SDL_GetTicks();
    int running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (stream) SDL_LockAudioStream(stream);   /* the game may queue notes */
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = 0;
                break;
            case SDL_EVENT_KEY_DOWN: {
                SDL_Keycode k = ev.key.key;
                if (k == SDLK_ESCAPE) running = 0;
                else if (k == SDLK_UP || k == SDLK_KP_8) game_arrow(&game, -1, 0);
                else if (k == SDLK_LEFT || k == SDLK_KP_4) game_arrow(&game, 0, -1);
                else if (k == SDLK_RIGHT || k == SDLK_KP_6) game_arrow(&game, 0, 1);
                else if (k == SDLK_DOWN || k == SDLK_KP_2) game_arrow(&game, 1, 0);
                else if ((k == SDLK_RETURN && (ev.key.mod & SDL_KMOD_ALT)) || k == SDLK_F11) {
                    aopt.fullscreen = !aopt.fullscreen;
                    SDL_SetWindowFullscreen(win, aopt.fullscreen);
                } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) game_enter(&game);
                else if (k == SDLK_BACKSPACE) game_backspace(&game);
                else if (k == SDLK_F2) aopt.square_pixels = !aopt.square_pixels;
                else if (k == SDLK_F3) snd.muted = !snd.muted;
                if (!ev.key.repeat) game_any_key(&game);
                break;
            }
            case SDL_EVENT_TEXT_INPUT:
                game_text(&game, ev.text.text);
                break;
            default:
                break;
            }
            if (stream) SDL_UnlockAudioStream(stream);
        }

        Uint64 now = SDL_GetTicks() - start;
        if (stream) SDL_LockAudioStream(stream);
        game_update(&game, (double)now);
        if (stream) SDL_UnlockAudioStream(stream);
        else snd.head = snd.tail;                    /* no device: discard */
        if (game.phase == GS_QUIT) running = 0;
        if (aopt.quit_after_ms > 0 && (long)now >= aopt.quit_after_ms) running = 0;

        /* draw */
        render_text(&scr, pixels, SDL_GetTicks());
        SDL_UpdateTexture(tex, NULL, pixels, TEX_W * (int)sizeof(Uint32));

        int ow, oh;
        SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
        float aspect = aopt.square_pixels ? (float)TEX_W / TEX_H : 4.0f / 3.0f;
        float dw = (float)ow, dh = dw / aspect;
        if (dh > oh) { dh = (float)oh; dw = dh * aspect; }
        SDL_FRect dst = { (ow - dw) / 2, (oh - dh) / 2, dw, dh };

        /* sharp-bilinear: nearest-neighbour to an integer multiple, then smooth */
        int k = (int)SDL_ceilf(dh / TEX_H);
        int kw = (int)SDL_ceilf(dw / TEX_W);
        if (kw > k) k = kw;
        if (k < 1) k = 1;
        if (k > 8) k = 8;
        if (k != big_k) {
            if (big) SDL_DestroyTexture(big);
            big = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET,
                                    TEX_W * k, TEX_H * k);
            if (big) SDL_SetTextureScaleMode(big, SDL_SCALEMODE_LINEAR);
            big_k = k;
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        if (big) {
            SDL_SetRenderTarget(ren, big);
            SDL_RenderTexture(ren, tex, NULL, NULL);
            SDL_SetRenderTarget(ren, NULL);
            SDL_RenderClear(ren);
            SDL_RenderTexture(ren, big, NULL, &dst);
        } else {
            SDL_RenderClear(ren);
            SDL_RenderTexture(ren, tex, NULL, &dst);
        }

        if (!running && aopt.screenshot) {
            SDL_Surface *shot = SDL_RenderReadPixels(ren, NULL);
            if (shot) { SDL_SaveBMP(shot, aopt.screenshot); SDL_DestroySurface(shot); }
        }
        SDL_RenderPresent(ren);
        SDL_Delay(1);
    }

    SDL_StopTextInput(win);
    if (stream) SDL_DestroyAudioStream(stream);
    if (big) SDL_DestroyTexture(big);
    SDL_DestroyTexture(tex);
    SDL_free(pixels);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
