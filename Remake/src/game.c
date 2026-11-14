/* game.c - PAC-GAL (Al J. Jimenez, May 1982) game logic.
 * Line numbers in comments refer to Analysis/PACGAL_reconstructed.bas.
 */
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- strings from the original program (CP437) ------------------------ */
static const char *E_TUNE = "t255mbl64o1afgao4d";                      /* eat a dot     */
static const char *W_TUNE = "mbl64abceabceebceabceagaa";               /* power pellet  */
static const char *I_TUNE = "mbt190o2l8bbbl16cecl8bbp8bbbl16cl8edcc";  /* intro         */
static const char *D_TUNE = "mbl8t255o4fego3abcdefgo0l1g-g";           /* death         */
static const char *UP_SCALE = "cc#dd#eff#gg#aa#b";                      /* UP$           */

/* The maze exactly as the original draws it (rows 1-24), one char per screen
 * column, using ASCII stand-ins decoded by maze_code(). */
static const char *MAZE[24] = {
    "_______________________________________________________________________________",
    "# . . . . . . . . | . . . . . . . . . | . . . . . . . . . | . . . . . . . . . #",
    "# . | . r====== . | . ========T==== . | . ====T======== . | . ========7 . | . #",
    "# . | . | . . . . . . . . . . | . . . | . . . | . . . . . . . . . . . | . | . #",
    "# . | . | . =============== . | . ====W==== . | . ================= . | . | . #",
    "# . | . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . | . #",
    "# . | . ========= . ======7 . r=======T=======T==== . r==== . ========= . | . #",
    "# . | . . . . . . . . . r=J . | . . . | . . . | . . . | . . . . . . . . . | . #",
    "# . | . | . ========7 . | . . . . | . . . | . . . | . | . r======== . | . | . #",
    "# . . . | . . . . . | . | . | . ==W=======W==== . | . | . | . . . . . | . . . #",
    "^^^^^^^^^ . | . | . . . | . | . . . . . . . . . . | . | . . . | . | . ^^^^^^^^^",
    "--- . . . . | . | . | . | . | . r====     ====7 . | . | . | . | . | . . . . ---",
    "_________ . | . | . | . | . . . |             | . . . | . | . | . | . _________",
    "# . . . | . . . | . | . | . | . |             | . | . | . | . | . . . | . . . #",
    "# . | . | . | . . . | . | . | . |             | . | . | . | . . . | . | . | . #",
    "# . | . | . L==== . | . | . | . L=============J . | . | . | . ====J . | . | . #",
    "# . | . . . . . . . | . . . . . . . . . . . . . . . . . . | . . . . . . . | . #",
    "# . | . ============J . L=============================J . L============ . | . #",
    "# . | . . . . . . . . . . . . . . . .   . . . . . . . . . . . . . . . . . | . #",
    "# . | . | . =============== . | . ====T==== . | . ================= . | . | . #",
    "# . | . | . . . . . . . . . . | . . . | . . . | . . . . . . . . . . . | . | . #",
    "# . | . L====== . | . ========W==== . | . ====W======== . | . ========J . | . #",
    "# . . . . . . . . | . . . . . . . . . | . . . . . . . . . | . . . . . . . . . #",
    "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^",
};

static unsigned char maze_code(char c)
{
    switch (c) {
    case '#': return 219; case '_': return 220; case '^': return 223;
    case '=': return 205; case '|': return 186; case '-': return 196;
    case 'r': return 201; case '7': return 187; case 'L': return 200;
    case 'J': return 188; case 'T': return 203; case 'W': return 202;
    case '.': return 249; default:  return 32;
    }
}

/* ---- helpers ------------------------------------------------------------ */
enum { EV_NONE, EV_COLLIDE, EV_WIN, EV_GAMEOVER };
enum { RES_AFTER_PLAYER, RES_PASSES };
enum { FLASH_DEATH = 1, FLASH_EAT = 2 };

#define A7 0x07                               /* COLOR 7,0  */
#define A26 0x8A                              /* COLOR 26,0 */

static int cint(double x) { return x >= 0 ? (int)floor(x + 0.5) : -(int)floor(-x + 0.5); }
static int sgn(int x) { return (x > 0) - (x < 0); }

static int SCR(Game *g, int r, int c) { return scr_char(g->scr, r, c * 2 + 1); }
static int ATTR(Game *g, int r, int c) { return scr_attr(g->scr, r, c * 2 + 1); }
static void PUT(Game *g, int r, int c, int ch, int color)
{
    scr_put_at(g->scr, r, c * 2 + 1, (unsigned char)ch, scr_color_attr(color, 0));
}

static double rnd01(Game *g)              /* xorshift32 */
{
    unsigned x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x ? x : 0x12345678u;
    return (g->rng >> 8) / 16777216.0;
}

/* SG=RND(2) into an integer: CINT(RND) -> 0 or 1 */
static int rnd2(Game *g)
{
    if (g->rnd2_hook) return g->rnd2_hook(g->hook_ud);
    return cint(rnd01(g));
}

/* DEF FNT - seconds-of-the-day clock */
static int fnt(Game *g)
{
    if (g->clock_hook) return g->clock_hook(g->hook_ud);
    if (g->opt.faithful_clock) {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        return lt->tm_hour * 360 + lt->tm_min * 60 + lt->tm_sec; /* sic: *360 */
    }
    return (int)(g->now / 1000.0);
}

static double step_ms(Game *g)
{
    int sp = g->SP < 0 ? 0 : g->SP;
    return g->opt.step_base_ms + sp * g->opt.step_per_speed_ms;
}

static void print_lives(Game *g, int trailing)       /* PRINT STRING$(LV,1);A$;A$;A$;A$; */
{
    scr_locate(g->scr, 25, 15);
    for (int i = 0; i < g->LV; i++) scr_putc(g->scr, 1);
    if (trailing) scr_print(g->scr, "    ");
}

static void print_dots(Game *g)                       /* LOCATE 25,5:PRINT DT; */
{
    scr_locate(g->scr, 25, 5);
    scr_print_int(g->scr, g->DT);
}

/* ---- options / init ----------------------------------------------------- */
void game_default_options(GameOptions *o)
{
    o->speed = -1;
    o->faithful_clock = 0;
    /* Rough timing of the original on a 4.77 MHz IBM PC (see the analysis). */
    o->step_base_ms = 60.0;
    o->step_per_speed_ms = 0.06;
    o->intro_step_ms = 40.0;
    o->death_flash_ms = 1000.0;
    o->eat_flash_ms = 500.0;
    o->fright_min_s = 6.0;   /* the original's 13 steps was ~1.5 s: far too short */
    o->ghost_randomness = 0.0;
    o->fright_scale = 1.0;
    o->scared_ghosts = 0;
}

static void restart_intro(Game *g);

void game_init(Game *g, Screen *scr, Sound *snd, const GameOptions *opt, unsigned seed)
{
    memset(g, 0, sizeof *g);
    g->scr = scr;
    g->snd = snd;
    if (opt) g->opt = *opt; else game_default_options(&g->opt);
    g->seed_base = seed ? seed : 0x9E3779B9u;
    g->rng = g->seed_base;
    scr_init(scr);

    /* 110-120: CLS : PRINT "How fast (0-30000)";:INPUT SP */
    if (g->opt.speed >= 0) {
        g->SP = g->opt.speed;
        restart_intro(g);
    } else {
        scr_print(scr, "How fast (0-30000)? ");
        scr->show_cursor = 1;
        g->phase = GS_PROMPT;
    }
}

/* ---- 180-280: intro ----------------------------------------------------- */
static void restart_intro(Game *g)
{
    scr_color(g->scr, 7, 0);
    scr_cls(g->scr);
    g->scr->show_cursor = 0;
    g->board_ready = 0;
    snd_play(g->snd, I_TUNE);
    g->intro_pass = 1;
    g->intro_i = 60;
    g->phase = GS_INTRO;
    g->next_at = g->now;
}

static void intro_tick(Game *g)
{
    Screen *s = g->scr;
    if (g->intro_pass == 1) {
        scr_locate(s, 14, g->intro_i);
        scr_putc(s, 2); scr_putc(s, ' ');
        if (--g->intro_i < 1) {
            snd_play(g->snd, I_TUNE);
            g->intro_pass = 2;
            g->intro_i = 1;
        }
    } else {
        static const unsigned char parade[] = { ' ', 3, ' ', ' ', ' ', 4, 5, 6,
                                                ' ', ' ', ' ', ' ', ' ', ' ', ' ', 2, 0 };
        scr_locate(s, 14, g->intro_i);
        scr_print(s, (const char *)parade);
        if (++g->intro_i > 60) {
            game_setup_board(g);
            g->phase = GS_PLAY;
            g->next_at = g->now;
        }
    }
}

/* ---- 290-590: board ----------------------------------------------------- */
void game_setup_board(Game *g)
{
    Screen *s = g->scr;
    g->S = g->NR = g->NC = 0;
    scr_color(s, 7, 0);
    scr_cls(s);

    /* 360-380: title line */
    scr_color(s, 15, 0);
    scr_locate(s, 25, 28);
    scr_print(s, "    P A C - G A L ");
    scr_color(s, 7, 0);
    scr_print(s, "        Al J. Jim\x82nez, May 1982");

    /* 390: maze */
    for (int r = 0; r < 24; r++)
        for (int c = 0; MAZE[r][c]; c++)
            scr_put_at(s, r + 1, c + 1, maze_code(MAZE[r][c]), A7);

    /* 400-450: power pellets (" " + CHR$(249) printed in COLOR 26) */
    static const int pel[10][2] = { {6,2},{6,76},{19,2},{2,30},{2,50},{23,30},{23,50},{19,76},{12,18},{12,60} };
    scr_color(s, 26, 0);
    for (int i = 0; i < 10; i++) {
        scr_locate(s, pel[i][0], pel[i][1]);
        scr_putc(s, ' '); scr_putc(s, 249);
    }
    scr_color(s, 7, 0);

    /* 460-490 */
    g->LV = 3; g->DT = 468; g->P = g->opt.ghost_randomness;   /* original: P=0 */
    g->T0 = fnt(g);
    g->rng = g->seed_base ^ (unsigned)(g->T0 * 2654435761u);   /* RANDOMIZE T0 */
    scr_locate(s, 25, 1);
    scr_print(s, "dots");
    scr_print_int(s, g->DT);
    print_lives(g, 0);

    /* 500-560: entities */
    g->CH[1] = 1; g->CL[1] = 7; g->R[1] = 19; g->C[1] = 19; g->U[1] = 32;
    g->F[1] = 0; g->DR[1] = 0; g->DC[1] = 0; g->KR[1] = 0; g->KC[1] = 0;
    for (int k = 2; k <= 5; k++) {
        g->CH[k] = k + 1; g->CL[k] = 7; g->R[k] = 14; g->C[k] = k + 16; g->U[k] = 32;
        int sg = rnd2(g); if (sg == 0) sg = -1;
        g->F[k] = 0;
        g->DR[k] = rnd2(g);
        g->DC[k] = (1 - g->DR[k]) * sg;
        g->DR[k] = g->DR[k] * sg;
        g->KR[k] = 0; g->KC[k] = 0;
    }
    g->board_ready = 1;
}

/* ---- 1300-1360: Pac-Gal arrives on (NR,NC) ------------------------------ */
static int finish_player_move(Game *g, int nr, int nc, int s)
{
    PUT(g, g->R[1], g->C[1], g->U[1], 7);
    PUT(g, nr, nc, g->CH[1], g->CL[1]);
    g->U[1] = 32; g->R[1] = nr; g->C[1] = nc;
    if (s == 249 && g->DT > 0) {
        g->DT--;
        print_dots(g);
        snd_play(g->snd, E_TUNE);
    } else {
        snd_sound(g->snd, 150, .1);
    }
    if (g->DT < 1) return EV_WIN;
    if (g->DT < 50) g->P = g->P * .5;
    return EV_NONE;
}

/* ---- 1000-1360: move Pac-Gal --------------------------------------------- */
static int free_cell(int s) { return s == 32 || s == 249; }

static int move_player(Game *g)
{
    int nr, nc, s, took_wanted = 0;

    nr = g->KR[1] + g->R[1]; nc = g->KC[1] + g->C[1];          /* wanted direction */
    s = SCR(g, nr, nc);
    if (free_cell(s)) { took_wanted = 1; goto moved; }
    if (s > 2 && s < 7) goto collide;
    if (nr == 12 && s == 196) {
        nc = 39 - g->C[1]; s = SCR(g, nr, nc);
        if (free_cell(s)) { took_wanted = 1; goto moved; }
    }

    nr = g->DR[1] + g->R[1]; nc = g->DC[1] + g->C[1];          /* current direction */
    s = SCR(g, nr, nc);
    if (free_cell(s)) goto moved;
    if (s > 2 && s < 7) goto collide;
    if (nr == 12 && s == 196) {
        nc = 39 - g->C[1]; s = SCR(g, nr, nc);
        if (free_cell(s)) goto moved;
    }

    g->DR[1] = 0; g->DC[1] = 0;                                /* blocked */
    snd_sound(g->snd, 4000, .2);
    return EV_NONE;

moved:
    if (took_wanted) { g->DR[1] = g->KR[1]; g->DC[1] = g->KC[1]; }
    if (ATTR(g, nr, nc) > 7) {                                 /* power pellet */
        snd_play(g->snd, W_TUNE);
        int i = cint(((double)g->DT / 5.0 + 20.0) / (g->LV * g->LV) * g->opt.fright_scale);
        if (g->opt.fright_min_s > 0) {                         /* not in the original */
            int min_steps = (int)ceil(g->opt.fright_min_s * 1000.0 / step_ms(g));
            if (i < min_steps) i = min_steps;
        }
        for (int k = 2; k <= 5; k++) { g->CL[k] = 26; g->F[k] = i; }
    }
    return finish_player_move(g, nr, nc, s);

collide:
    g->NR = nr; g->NC = nc; g->S = s;
    return EV_COLLIDE;
}

/* ---- 2000-2900: move the ghosts ------------------------------------------ */
static int move_ghosts(Game *g)
{
    for (int k = 2; k <= 5; k++) {
        int nr, nc, s, sg, towards;
        g->F[k]--;
        /* optional: a frightened ghost turns away from Pac-Gal instead */
        towards = (g->opt.scared_ghosts && g->F[k] > 0) ? -1 : 1;
        nr = g->R[k]; nc = g->C[k];
        if (nc >= 18 && nc <= 23 && (nr == 11 || nr == 12)) goto straight;
        if (nc >= 18 && nc <= 23 && nr > 12 && nr < 16) goto turn;
        if (rnd01(g) < g->P) goto straight;                     /* P is 0 unless changed */

        if (!(g->DR[k] != 0 || g->R[k] == g->R[1])) {           /* chase: row */
            nr = nr + towards * sgn(g->R[1] - nr);
            s = SCR(g, nr, nc);
            if (free_cell(s)) goto accept;
            if (s == 1) goto hit;
        }
        if (!(g->DC[k] != 0 || g->C[k] == g->C[1])) {           /* chase: column */
            nr = g->R[k];
            nc = nc + towards * sgn(g->C[1] - nc);
            s = SCR(g, nr, nc);
            if (free_cell(s)) goto accept;
            if (s == 1) goto hit;
        }
    straight:
        nr = g->R[k] + g->DR[k]; nc = g->C[k] + g->DC[k];
        s = SCR(g, nr, nc);
        if (free_cell(s)) goto accept;
        if (s == g->CH[1]) goto hit;
    turn:
        sg = rnd2(g); if (sg == 0) sg = -1;
        nr = g->R[k] + g->DC[k] * sg; nc = g->C[k] + g->DR[k] * sg;
        s = SCR(g, nr, nc);
        if (free_cell(s)) goto accept;
        if (s == g->CH[1]) goto hit;
        nr = g->R[k] - g->DC[k] * sg; nc = g->C[k] - g->DR[k] * sg;
        s = SCR(g, nr, nc);
        if (free_cell(s)) goto accept;
        if (s == g->CH[1]) goto hit;
        nr = g->R[k] - g->DR[k]; nc = g->C[k] - g->DC[k];
        s = SCR(g, nr, nc);
        if (free_cell(s)) goto accept;
        if (s == g->CH[1]) goto hit;
        nr = g->R[k]; nc = g->C[k]; s = g->U[k];                 /* boxed in */
        goto draw;
    accept:
        g->DR[k] = nr - g->R[k]; g->DC[k] = nc - g->C[k];
    draw:
        PUT(g, g->R[k], g->C[k], g->U[k], 7);
        if (g->F[k] <= 0) g->CL[k] = 7;
        PUT(g, nr, nc, g->CH[k], g->CL[k]);
        g->U[k] = s; g->R[k] = nr; g->C[k] = nc;
        continue;
    hit:
        g->NR = g->R[k]; g->NC = g->C[k]; g->S = k + 1;
        return EV_COLLIDE;
    }
    return EV_NONE;
}

/* ---- 3000-3660: collision ------------------------------------------------- */
static void collision_begin(Game *g)
{
    if (ATTR(g, g->NR, g->NC) > 7) {                           /* 3500: eat it */
        char tune[96];
        strcpy(tune, "mbl24o2"); strcat(tune, UP_SCALE);
        strcat(tune, "l32o3");   strcat(tune, UP_SCALE);
        strcat(tune, "l64o4");   strcat(tune, UP_SCALE);
        snd_play(g->snd, tune);
        PUT(g, g->R[1], g->C[1], g->U[1], 7);
        PUT(g, g->NR, g->NC, 2, 26);
        g->flash_kind = FLASH_EAT;
    } else {                                                   /* 3010: caught */
        snd_play(g->snd, D_TUNE);
        PUT(g, g->R[1], g->C[1], g->U[1], 7);
        PUT(g, g->NR, g->NC, 168, 26);
        g->flash_kind = FLASH_DEATH;
    }
}

static int collision_end(Game *g)
{
    if (g->flash_kind == FLASH_DEATH) {
        if (g->DT > 300 && g->P < .1) g->P = g->P + g->P;     /* 3050 (no-op when P=0) */
        if (g->LV <= 1) return EV_GAMEOVER;
        g->LV--;
        print_lives(g, 1);
        /* 3090 ends with a comma: pad with spaces to the next 14-column print zone */
        while ((g->scr->col - 1) % 14 != 0) scr_putc(g->scr, ' ');
        for (int k = 1; k <= 5; k++) {
            PUT(g, g->R[k], g->C[k], g->U[k], 7);
            g->U[k] = 32; g->CL[k] = 7; g->R[k] = 14; g->C[k] = k + 17; g->F[k] = 0;
            if (k == 1) { g->DR[k] = 0; g->DC[k] = 0; }
            g->KR[k] = 0; g->KC[k] = 0;
        }
        g->R[1] = 19; g->C[1] = 19;
        g->T0 = fnt(g);
        return EV_NONE;
    }
    /* eat ghost: 3540-3660 */
    g->T0 = fnt(g);
    g->P = g->P * .5;
    if (g->LV < 3) { g->LV++; print_lives(g, 1); }
    for (int k = 2; k <= 5; k++) {
        g->F[k]--;
        if (g->R[k] != g->NR || g->C[k] != g->NC) continue;
        g->CL[k] = 7; g->R[k] = 14; g->C[k] = k + 16;
        if (g->U[k] == 249) { g->DT--; print_dots(g); }
        g->U[k] = 32;
    }
    return finish_player_move(g, g->NR, g->NC, g->S);
}

/* ---- end-of-game screens --------------------------------------------------- */
static void begin_again(Game *g)                              /* 4100 */
{
    scr_locate(g->scr, 25, 10);
    scr_color(g->scr, 26, 0);
    scr_print(g->scr, "Play again???");
    scr_color(g->scr, 7, 0);
    g->phase = GS_AGAIN;
}

static void begin_win(Game *g)                                /* 4000 */
{
    scr_locate(g->scr, 25, 10);
    scr_color(g->scr, 26, 0);
    scr_print(g->scr, "You did it!!!");
    scr_color(g->scr, 7, 0);
    g->T0 = fnt(g);
    g->phase = GS_WIN;
    if (g->sync) begin_again(g);
}

static void begin_bye(Game *g)                                /* 4150-4170 */
{
    scr_color(g->scr, 7, 0);
    scr_cls(g->scr);
    scr_print(g->scr, "...Hope you had a good time");
    g->phase = GS_BYE;
    g->next_at = g->now + 3000.0;
}

/* ---- the main loop, made resumable --------------------------------------- */
static void run_passes(Game *g);
static void after_player(Game *g);

static void start_flash(Game *g, int resume);

static void end_flash(Game *g)
{
    int ev = collision_end(g);
    g->phase = GS_PLAY;
    g->next_at = g->now + step_ms(g);
    if (ev == EV_GAMEOVER) { begin_again(g); return; }
    if (ev == EV_WIN) { begin_win(g); return; }
    if (g->resume == RES_AFTER_PLAYER) after_player(g);
    else run_passes(g);
}

static void start_flash(Game *g, int resume)
{
    g->resume = resume;
    collision_begin(g);
    if (g->sync) { end_flash(g); return; }
    g->phase = GS_FLASH;
    g->next_at = g->now + (g->flash_kind == FLASH_DEATH ? g->opt.death_flash_ms : g->opt.eat_flash_ms);
}

static void run_passes(Game *g)                               /* 730: FOR T=1 TO EL STEP 180 */
{
    while (g->passes_left > 0) {
        g->passes_left--;
        if (move_ghosts(g) == EV_COLLIDE) { start_flash(g, RES_PASSES); return; }
    }
}

static void after_player(Game *g)                             /* 720-730 */
{
    int el = fnt(g) - g->T0;
    if (el <= 1) el = 1;
    g->passes_left = (el - 1) / 180 + 1;
    run_passes(g);
}

void game_step(Game *g)                                       /* 700-740 */
{
    if (!g->board_ready || g->phase != GS_PLAY) return;
    int ev = move_player(g);
    if (ev == EV_COLLIDE) { start_flash(g, RES_AFTER_PLAYER); return; }
    if (ev == EV_WIN) { begin_win(g); return; }
    after_player(g);
}

void game_update(Game *g, double now_ms)
{
    g->now = now_ms;
    switch (g->phase) {
    case GS_INTRO:
        if (g->now - g->next_at > 500.0) g->next_at = g->now;
        while (g->phase == GS_INTRO && g->now >= g->next_at) {
            g->next_at += g->opt.intro_step_ms;
            intro_tick(g);
        }
        break;
    case GS_PLAY: {
        /* Keep a steady step rate even if frames arrive late; after a long
         * stall (window dragged, machine asleep) just carry on from now. */
        double step = step_ms(g);
        if (g->now - g->next_at > 250.0) g->next_at = g->now;
        for (int n = 0; n < 4 && g->phase == GS_PLAY && g->now >= g->next_at; n++) {
            g->next_at += step;
            game_step(g);
        }
        break;
    }
    case GS_FLASH:
        if (g->now >= g->next_at) end_flash(g);
        break;
    case GS_WIN:
        if (fnt(g) - g->T0 >= 15) begin_again(g);
        break;
    case GS_BYE:
        if (g->now >= g->next_at) g->phase = GS_QUIT;
        break;
    default:
        break;
    }
}

/* ---- input ------------------------------------------------------------------ */
void game_arrow(Game *g, int dr, int dc)                      /* 640-670: ON KEY(11..14) */
{
    if (!g->board_ready) return;
    g->KR[1] = dr;
    g->KC[1] = dc;
}

void game_text(Game *g, const char *utf8)
{
    for (const char *p = utf8; *p; p++) {
        char ch = *p;
        if (g->phase == GS_PROMPT) {
            if (((ch >= '0' && ch <= '9') || (ch == '-' && g->input_len == 0)) &&
                g->input_len < 6) {
                g->input[g->input_len++] = ch;
                g->input[g->input_len] = 0;
                scr_putc(g->scr, (unsigned char)ch);
            }
        } else if (g->phase == GS_AGAIN) {                    /* 4120-4140 */
            if (ch == 'y' || ch == 'Y') { restart_intro(g); return; }
            if (ch == 'n') { begin_bye(g); return; }
        }
    }
}

void game_backspace(Game *g)
{
    if (g->phase != GS_PROMPT || g->input_len == 0) return;
    g->input[--g->input_len] = 0;
    g->scr->col--;
    scr_put_at(g->scr, g->scr->row, g->scr->col, ' ', 0x07);
}

void game_enter(Game *g)
{
    if (g->phase != GS_PROMPT) return;
    long v = atol(g->input);
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    g->SP = (int)v;
    g->scr->show_cursor = 0;
    restart_intro(g);                                          /* 130 CLS ... 180 */
}

void game_any_key(Game *g)
{
    if (g->phase == GS_BYE) g->phase = GS_QUIT;
}
