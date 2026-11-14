/* Logic tests for the PAC-GAL port.  The expected values were measured by
 * running the original PAC-GAL.EXE in an emulator (see Analysis/). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Screen scr;
static Sound snd;
static Game g;

static int fixed_clock(void *ud) { (void)ud; return 1000; }

static void fresh(void)
{
    GameOptions o;
    game_default_options(&o);
    o.speed = 0;
    o.fright_min_s = 0;          /* the expected values are the original's */
    snd_init(&snd, 8000);
    game_init(&g, &scr, &snd, &o, 1234);
    g.clock_hook = fixed_clock;
    g.sync = 1;
    game_setup_board(&g);
    g.phase = GS_PLAY;
}

static void test_board(void)
{
    fresh();
    int dots = 0, pellets = 0;
    for (int r = 1; r <= 25; r++)
        for (int c = 1; c <= 80; c++) {
            if (scr_char(&scr, r, c) == 249) dots++;
            if (scr_char(&scr, r, c) == 249 && scr_attr(&scr, r, c) == 0x8A) pellets++;
        }
    CHECK(dots == 468, "dots on screen %d", dots);
    CHECK(pellets == 10, "power pellets %d", pellets);
    CHECK(g.DT == 468 && g.LV == 3, "DT %d LV %d", g.DT, g.LV);
    CHECK(g.R[1] == 19 && g.C[1] == 19, "player start");
    for (int k = 2; k <= 5; k++) {
        CHECK(g.R[k] == 14 && g.C[k] == k + 16, "ghost %d start", k);
        CHECK(g.CH[k] == k + 1, "ghost glyph");
        CHECK(abs(g.DR[k]) + abs(g.DC[k]) == 1, "ghost %d has a unit direction", k);
    }
    const char *title = "    P A C - G A L ";
    for (int i = 0; title[i]; i++)
        CHECK(scr_char(&scr, 25, 28 + i) == (unsigned char)title[i] && scr_attr(&scr, 25, 28 + i) == 0x0F, "title");
    CHECK(scr_char(&scr, 25, 1) == 'd' && scr_char(&scr, 25, 6) == '4' && scr_char(&scr, 25, 8) == '8', "dots label");
    CHECK(scr_char(&scr, 25, 15) == 1 && scr_char(&scr, 25, 17) == 1 && scr_char(&scr, 25, 18) == ' ', "lives");
}

/* Power pellet at (6,1): step onto it from (7,1). */
static int fright_after(int lives, int dots)
{
    fresh();
    g.LV = lives; g.DT = dots;
    g.R[1] = 7; g.C[1] = 1; g.KR[1] = -1; g.KC[1] = 0;
    game_step(&g);
    return g.F[2] + 1;  /* the ghost pass already counted one step off */
}

static void test_fright(void)
{
    static const int cases[][3] = { {3,468,13}, {2,468,28}, {1,468,114}, {3,100,4}, {1,10,22}, {2,10,6}, {2,30,7} };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int f = fright_after(cases[i][0], cases[i][1]);
        CHECK(f == cases[i][2], "fright lives=%d dots=%d: got %d want %d", cases[i][0], cases[i][1], f, cases[i][2]);
        CHECK(g.CL[2] == 26, "ghosts turn blinking green");
    }
}

static void test_tunnel(void)
{
    fresh();
    g.R[1] = 12; g.C[1] = 3; g.KR[1] = 0; g.KC[1] = -1;
    game_step(&g);
    CHECK(g.R[1] == 12 && g.C[1] == 2, "tunnel step 1: %d,%d", g.R[1], g.C[1]);
    game_step(&g);
    CHECK(g.R[1] == 12 && g.C[1] == 37, "tunnel wrap: %d,%d", g.R[1], g.C[1]);
}

/* Same scenario as the emulator run: eat a pellet with 2 lives, then a
 * frightened ghost walks into Pac-Gal. */
static void test_eat_ghost(void)
{
    fresh();
    g.LV = 2;
    g.R[1] = 7; g.C[1] = 1; g.KR[1] = -1; g.KC[1] = 0;
    game_step(&g);
    CHECK(g.R[1] == 6 && g.DT == 467, "on the pellet");
    g.R[2] = 4; g.C[2] = 1; g.U[2] = 249; g.DR[2] = 1; g.DC[2] = 0;
    scr_put_at(&scr, 4, 3, 3, 0x8A);
    game_step(&g);
    CHECK(g.LV == 3, "bonus life: %d", g.LV);
    CHECK(g.R[2] == 14 && g.C[2] == 18 && g.CL[2] == 7, "ghost home: %d,%d", g.R[2], g.C[2]);
    CHECK(g.R[1] == 4 && g.C[1] == 1, "Pac-Gal jumps onto the ghost's cell: %d,%d", g.R[1], g.C[1]);
    CHECK(g.DT == 465, "dot under the ghost counted: %d", g.DT);
}

static void test_death(void)
{
    fresh();
    /* ghost 2 right next to a stationary Pac-Gal, heading for her */
    g.R[2] = 19; g.C[2] = 18; g.DR[2] = 0; g.DC[2] = 1; g.U[2] = 249;
    scr_put_at(&scr, 19, 37, 3, 0x07);
    game_step(&g);       /* first step draws Pac-Gal, then the ghost walks in */
    CHECK(g.LV == 2, "lost a life: %d", g.LV);
    CHECK(g.R[1] == 19 && g.C[1] == 19 && g.DR[1] == 0 && g.KR[1] == 0, "Pac-Gal reset");
    for (int k = 2; k <= 5; k++) CHECK(g.R[k] == 14 && g.C[k] == k + 17, "ghost %d respawn", k);
    g.LV = 1;
    g.R[2] = 19; g.C[2] = 17; g.DR[2] = 0; g.DC[2] = 1;
    scr_put_at(&scr, 19, 35, 3, 0x07);
    for (int i = 0; i < 3 && g.phase == GS_PLAY; i++) game_step(&g);
    CHECK(g.phase == GS_AGAIN, "game over -> Play again (phase %d)", g.phase);
}

static void test_board_clear(void)
{
    fresh();
    g.DT = 1;
    g.R[1] = 19; g.C[1] = 19; g.KR[1] = 0; g.KC[1] = -1;   /* dot at (19,18) */
    game_step(&g);
    CHECK(g.DT == 0, "last dot eaten");
    CHECK(g.phase == GS_AGAIN, "board clear -> You did it -> Play again (phase %d)", g.phase);
    CHECK(scr_char(&scr, 25, 10) == 'P' && scr_attr(&scr, 25, 10) == 0x8A, "Play again??? in blinking green");
    game_text(&g, "N");
    CHECK(g.phase == GS_AGAIN, "capital N is ignored, like the original");
    game_text(&g, "n");
    CHECK(g.phase == GS_BYE, "n quits");
}

static void test_options(void)
{
    /* defaults are the original game */
    GameOptions o; game_default_options(&o);
    CHECK(o.ghost_randomness == 0 && o.fright_scale == 1.0 && !o.scared_ghosts, "other options off by default");
    CHECK(o.fright_min_s == 6.0, "pellets last 6 s by default");
    /* minimum fright time: 6 s at speed 750 (105 ms steps) = 58 steps */
    fresh();
    g.opt.fright_min_s = 6.0; g.SP = 750;
    g.R[1] = 7; g.C[1] = 1; g.KR[1] = -1; g.KC[1] = 0;
    game_step(&g);
    CHECK(g.F[2] + 1 == 58, "6 s minimum at speed 750: %d steps", g.F[2] + 1);
    /* ...but the original's longer time on the last life still wins */
    fresh();
    g.opt.fright_min_s = 6.0; g.SP = 750; g.LV = 1;
    g.R[1] = 7; g.C[1] = 1; g.KR[1] = -1; g.KC[1] = 0;
    game_step(&g);
    CHECK(g.F[2] + 1 == 114, "last life keeps 114: %d", g.F[2] + 1);
    /* fright scale */
    fresh();
    g.opt.fright_scale = 2.0;
    g.R[1] = 7; g.C[1] = 1; g.KR[1] = -1; g.KC[1] = 0;
    game_step(&g);
    CHECK(g.F[2] + 1 == 25, "fright x2: %d", g.F[2] + 1);   /* CINT(12.62*2) */
    /* a scared, frightened ghost on an open row steps away from Pac-Gal */
    fresh();
    g.opt.scared_ghosts = 1;
    g.R[1] = 6; g.C[1] = 10;            /* row 6 is a long open corridor */
    g.R[2] = 6; g.C[2] = 14; g.DR[2] = 1; g.DC[2] = 0; g.F[2] = 10; g.CL[2] = 26; g.U[2] = 249;
    scr_put_at(&scr, 6, 29, 3, 0x8A);
    g.KR[1] = 0; g.KC[1] = 0;
    game_step(&g);
    CHECK(g.C[2] == 15, "scared ghost moves away (C=%d)", g.C[2]);
}

static void test_mml(void)
{
    Sound s;
    snd_init(&s, 1000);
    snd_play(&s, "t255mbl64o1afgao4d");
    CHECK(s.octave == 4 && s.tempo == 255 && s.length == 64, "PLAY state carries over");
    CHECK(s.q[0].freq > 109.9f && s.q[0].freq < 110.1f, "o1 a = 110 Hz (%f)", s.q[0].freq);
}

int main(void)
{
    test_board();
    test_fright();
    test_tunnel();
    test_eat_ghost();
    test_death();
    test_board_clear();
    test_options();
    test_mml();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("all PAC-GAL logic tests passed\n");
    return 0;
}
