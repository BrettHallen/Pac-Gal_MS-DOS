/* trace.c - state trace for the PAC-GAL (Al J. Jimenez, May 1982) remake.
 * Remake using Claude Opus 5.5, Nov 2026
 * Guided by Brett Hallen
 *
 * Runs the ported logic headless with a scripted input and prints the
 * game state after every main-loop step.  Analysis/tools/compare_trace.py runs
 * the original EXE with the same script and checks the two traces match.
 *
 * Script (both sides): two LCG streams.  Every RND(2) result comes from the
 * first; every 6th step a new arrow direction comes from the second; if lives
 * are down to 1 at the start of a step they are topped back up to 3.
 * usage: pacgal_trace [steps] [dump-step] [key-seed] [rnd-seed]
 */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"

static unsigned lcg_r = 12345, lcg_k = 777;
static unsigned lcg(unsigned *x) { *x = (*x * 1103515245u + 12345u) & 0x7fffffffu; return *x; }
static int rnd_bit(void *ud) { (void)ud; return (lcg(&lcg_r) >> 16) & 1; }
static int fixed_clock(void *ud) { (void)ud; return 1000; }

int main(int argc, char **argv)
{
    int steps = argc > 1 ? atoi(argv[1]) : 300;
    if (argc > 4) { lcg_k = (unsigned)atol(argv[3]); lcg_r = (unsigned)atol(argv[4]); }
    static Screen scr; static Sound snd; static Game g;
    GameOptions o; game_default_options(&o); o.speed = 0; o.fright_min_s = 0; o.ghost_randomness = 0; /* exact 1982 rules */
    snd_init(&snd, 8000);
    game_init(&g, &scr, &snd, &o, 1);
    g.rnd2_hook = rnd_bit; g.clock_hook = fixed_clock; g.sync = 1;
    game_setup_board(&g);
    g.phase = GS_PLAY;
    static const int dirs[4][2] = { {-1,0}, {0,-1}, {0,1}, {1,0} };
    for (int n = 1; n <= steps && g.phase == GS_PLAY; n++) {
        if (n % 6 == 1) { int d = (lcg(&lcg_k) >> 17) % 4; g.KR[1] = dirs[d][0]; g.KC[1] = dirs[d][1]; }
        if (g.LV == 1) g.LV = 3;
        game_step(&g);
        unsigned long sum = 0;
        for (int r = 0; r < SCR_ROWS; r++)
            for (int c = 0; c < SCR_COLS; c++) {
                unsigned ch = scr.cell[r][c].ch ? scr.cell[r][c].ch : 32;
                sum = (sum * 31 + (ch | (unsigned)scr.cell[r][c].attr << 8)) % 1000000007ul;
            }
        printf("%d", n);
        for (int k = 1; k <= 5; k++) printf(" %d,%d,%d,%d,%d,%d,%d", g.R[k], g.C[k], g.DR[k], g.DC[k], g.U[k], g.F[k], g.CL[k]);
        printf(" LV=%d DT=%d scr=%lu\n", g.LV, g.DT, sum);
        if (argc > 2 && n == atoi(argv[2]))
            for (int r = 0; r < SCR_ROWS; r++) { printf("ROW%02d", r + 1); for (int c = 0; c < SCR_COLS; c++) printf(" %02x%02x", scr.cell[r][c].ch ? scr.cell[r][c].ch : 32, scr.cell[r][c].attr); printf("\n"); }
    }
    return 0;
}
