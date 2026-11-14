/* game.h - PAC-GAL (Al J. Jimenez, May 1982) game logic, ported from the compiled BASIC.
 * Remake using Claude Opus 5.5, Nov 2026
 * Guided by Brett Hallen
 *
 * The logic is a line-for-line port of Analysis/PACGAL.bas.
 * Variable names match that listing (R(), C(), DR(), KR(), LV, DT, P, T0...).
 * Index 1 is Pac-Gal, 2..5 are the ghosts.  The original's blocking loops
 * (intro animation, death/eat flashes, the 15-second wait) are turned into
 * timed states so the window stays responsive.
 */
#ifndef PACGAL_GAME_H
#define PACGAL_GAME_H

#include "screen.h"
#include "sound.h"

typedef enum {
    GS_PROMPT,   /* "How fast (0-30000)?" */
    GS_INTRO,    /* running smiley + parade */
    GS_PLAY,     /* main loop */
    GS_FLASH,    /* death "?" or eat-ghost flash */
    GS_WIN,      /* "You did it!!!" 15-second pause */
    GS_AGAIN,    /* "Play again???" */
    GS_BYE,      /* "...Hope you had a good time" */
    GS_QUIT
} GamePhase;

typedef struct {
    int speed;               /* "How fast" value; -1 = ask like the original */
    int faithful_clock;      /* 1 = use the original's hh*360+mm*60+ss clock (with its bugs) */
    double step_base_ms;     /* time of one game step at speed 0 */
    double step_per_speed_ms;/* extra time per unit of speed */
    double intro_step_ms;
    double death_flash_ms;
    double eat_flash_ms;
    /* Changes from the 1982 game.  fright_min_s and ghost_randomness are on by
     * default; --original sets both to 0 for the exact original.  The rest are
     * off by default. */
    double fright_min_s;     /* power pellets last at least this many seconds */
    double ghost_randomness; /* starting value for the original's unused P: chance a ghost wanders instead of chasing */
    double fright_scale;     /* multiplies how long power pellets last */
    int scared_ghosts;       /* frightened ghosts turn away from Pac-Gal instead of towards her */
} GameOptions;

void game_default_options(GameOptions *o);

typedef struct Game {
    Screen *scr;
    Sound *snd;
    GameOptions opt;
    GamePhase phase;
    double now, next_at;       /* ms */

    /* INPUT prompt */
    char input[8];
    int input_len;

    /* intro animation */
    int intro_pass, intro_i;

    /* BASIC variables (DEFINT A-Z) */
    int SP, LV, DT, T0;
    double P;                  /* integer 0 in the original, so it never mattered */
    int CH[6], CL[6], R[6], C[6], U[6], F[6], DR[6], DC[6], KR[6], KC[6];
    int NR, NC, S;
    int board_ready;

    /* resumable main-loop step */
    int passes_left;
    int resume;                /* where to continue after a flash */
    int flash_kind;            /* 1 = death, 2 = eat ghost */

    /* randomness and time */
    unsigned rng, seed_base;
    int (*rnd2_hook)(void *ud);   /* tests: supplies the 0/1 results of RND(2) */
    int (*clock_hook)(void *ud);  /* tests: supplies FNT(2) */
    void *hook_ud;
    int sync;                     /* tests: resolve flashes instantly */
} Game;

void game_init(Game *g, Screen *scr, Sound *snd, const GameOptions *opt, unsigned seed);
void game_update(Game *g, double now_ms);

/* input */
void game_arrow(Game *g, int dr, int dc);  /* Up (-1,0) Left (0,-1) Right (0,1) Down (1,0) */
void game_text(Game *g, const char *utf8); /* typed characters */
void game_enter(Game *g);
void game_backspace(Game *g);
void game_any_key(Game *g);

/* lower-level entry points, used by the tests */
void game_setup_board(Game *g);  /* everything from the maze draw up to the main loop */
void game_step(Game *g);         /* one main-loop iteration */

#endif
