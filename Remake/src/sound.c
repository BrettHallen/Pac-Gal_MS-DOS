/* sound.c - PLAY/SOUND square-wave music for the PAC-GAL (Al J. Jimenez, May 1982) remake.
 * Remake using Claude Opus 5.5, Nov 2026
 * Guided by Brett Hallen
 */
#include "sound.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

void snd_reset_state(Sound *s)
{
    s->octave = 4;
    s->length = 4;
    s->tempo = 120;
    s->articulation = 7.0 / 8.0;
}

void snd_init(Sound *s, int rate)
{
    memset(s, 0, sizeof *s);
    s->rate = rate > 0 ? rate : 44100;
    s->level = 0.12f;
    s->slew = s->level / (0.001f * s->rate);   /* 1 ms ramps */
    snd_reset_state(s);
}

int snd_pending(const Sound *s) { return s->head != s->tail; }

static void enqueue(Sound *s, float freq, double seconds)
{
    unsigned n = (unsigned)(seconds * s->rate + 0.5);
    int next = (s->tail + 1) % SND_QUEUE;
    if (n == 0 || next == s->head) return;   /* queue full: BASIC would block; we drop */
    s->q[s->tail].freq = freq;
    s->q[s->tail].samples = n;
    s->tail = next;
}

/* Octave 3 starts at middle C (C4, 261.63 Hz); O0 C is C1. */
static float note_freq(int octave, int semitone)
{
    int midi = 12 * octave + semitone + 24;
    return (float)(440.0 * pow(2.0, (midi - 69) / 12.0));
}

static int read_num(const char **p, int *out)
{
    const char *q = *p;
    int v = 0, any = 0;
    while (isdigit((unsigned char)*q)) { v = v * 10 + (*q - '0'); q++; any = 1; }
    if (any) { *out = v; *p = q; }
    return any;
}

static double note_seconds(const Sound *s, int len, int dots)
{
    double d = (60.0 / s->tempo) * (4.0 / len);
    while (dots-- > 0) d *= 1.5;
    return d;
}

static int read_dots(const char **p)
{
    int n = 0;
    while (**p == '.') { n++; (*p)++; }
    return n;
}

static void play_tone(Sound *s, float freq, double seconds)
{
    if (freq <= 0) { enqueue(s, 0, seconds); return; }
    enqueue(s, freq, seconds * s->articulation);
    if (s->articulation < 1.0) enqueue(s, 0, seconds * (1.0 - s->articulation));
}

void snd_play(Sound *s, const char *mml)
{
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* a b c d e f g */
    const char *p = mml;
    int v;
    while (*p) {
        int c = tolower((unsigned char)*p++);
        if (c >= 'a' && c <= 'g') {
            int semi = base[c - 'a'], len = s->length;
            if (*p == '#' || *p == '+') { semi++; p++; }
            else if (*p == '-') { semi--; p++; }
            if (read_num(&p, &v) && v >= 1 && v <= 64) len = v;
            int dots = read_dots(&p);
            play_tone(s, note_freq(s->octave, semi), note_seconds(s, len, dots));
        } else if (c == 'n') {
            if (read_num(&p, &v)) {
                int dots = read_dots(&p);
                double sec = note_seconds(s, s->length, dots);
                if (v == 0) enqueue(s, 0, sec);
                else if (v <= 84) play_tone(s, note_freq((v - 1) / 12, (v - 1) % 12), sec);
            }
        } else if (c == 'o') {
            if (read_num(&p, &v) && v <= 6) s->octave = v;
        } else if (c == 'l') {
            if (read_num(&p, &v) && v >= 1 && v <= 64) s->length = v;
        } else if (c == 't') {
            if (read_num(&p, &v) && v >= 32 && v <= 255) s->tempo = v;
        } else if (c == 'p') {
            if (read_num(&p, &v) && v >= 1 && v <= 64) {
                int dots = read_dots(&p);
                enqueue(s, 0, note_seconds(s, v, dots));
            }
        } else if (c == '>') {
            if (s->octave < 6) s->octave++;
        } else if (c == '<') {
            if (s->octave > 0) s->octave--;
        } else if (c == 'm') {
            int m = tolower((unsigned char)*p);
            if (m) p++;
            if (m == 'n') s->articulation = 7.0 / 8.0;
            else if (m == 'l') s->articulation = 1.0;
            else if (m == 's') s->articulation = 3.0 / 4.0;
            /* MF / MB: everything is background here */
        }
        /* anything else (spaces, ';') is ignored */
    }
}

void snd_sound(Sound *s, int freq, double ticks)
{
    if (ticks <= 0) return;
    enqueue(s, (float)freq, ticks / 18.2065);
}

void snd_render(Sound *s, float *out, int frames)
{
    for (int i = 0; i < frames; i++) {
        float target = 0.0f;
        if (s->head != s->tail) {
            Note *n = &s->q[s->head];
            if (n->freq > 0) {
                s->phase += n->freq / s->rate;
                if (s->phase >= 1.0) s->phase -= floor(s->phase);
                target = s->level;
            }
            if (++s->done_in_note >= n->samples) {
                s->done_in_note = 0;
                s->head = (s->head + 1) % SND_QUEUE;
            }
        }
        if (s->muted) target = 0.0f;
        /* slew the amplitude so note starts and stops don't click */
        if (s->gain < target) { s->gain += s->slew; if (s->gain > target) s->gain = target; }
        else if (s->gain > target) { s->gain -= s->slew; if (s->gain < target) s->gain = target; }
        out[i] = s->phase < 0.5 ? s->gain : -s->gain;
    }
}
