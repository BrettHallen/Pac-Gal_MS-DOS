/* soak.c - soak test for the PAC-GAL (Al J. Jimenez, May 1982) remake.
 * Remake using Claude Opus 5.5, Nov 2026
 * Guided by Brett Hallen
 *
 * Drives the real-time (non-test) game path for many simulated hours:
 * random arrow keys, flashes, deaths, "Play again" answered with y.  Audio is
 * rendered at real-time rate.  Fails if a phase stalls.
 */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"

int main(int argc, char **argv)
{
    long frames = argc > 1 ? atol(argv[1]) : 2000000;   /* 16.7 ms frames */
    static Screen scr; static Sound snd; static Game g;
    static float buf[4096];
    GameOptions o; game_default_options(&o); o.speed = 1000;
    snd_init(&snd, 44100);
    game_init(&g, &scr, &snd, &o, 42);
    srand(7);
    double now = 0, last_change = 0;
    int last_phase = -1, games = 0, max_backlog = 0; double max_secs = 0;
    for (long f = 0; f < frames; f++) {
        now += 1000.0 / 60.0;
        if (rand() % 20 == 0) {
            static const int d[4][2] = { {-1,0},{0,-1},{0,1},{1,0} };
            int k = rand() % 4; game_arrow(&g, d[k][0], d[k][1]);
        }
        if (g.phase == GS_AGAIN) { game_text(&g, "y"); games++; }
        game_update(&g, now);
        int n = 735; snd_render(&snd, buf, n);            /* 1/60 s of audio */
        int backlog = (snd.tail - snd.head + SND_QUEUE) % SND_QUEUE;
        if (backlog > max_backlog) max_backlog = backlog;
        double secs = 0; for (int i = snd.head; i != snd.tail; i = (i + 1) % SND_QUEUE) secs += snd.q[i].samples / 44100.0;
        if (secs > max_secs) max_secs = secs;
        if ((int)g.phase != last_phase) { last_phase = g.phase; last_change = now; }
        if (g.phase != GS_PLAY && now - last_change > 20000) {
            printf("STALL: phase %d for %.0f s at frame %ld\n", g.phase, (now - last_change) / 1000, f);
            return 1;
        }
    }
    printf("soak OK: %.1f simulated hours, %d games, max audio backlog %d notes / %.2f s\n", now / 3.6e6, games, max_backlog, max_secs);
    return 0;
}
