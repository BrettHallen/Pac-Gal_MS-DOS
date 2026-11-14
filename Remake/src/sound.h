/* sound.h - PC-speaker style background music: IBM BASIC PLAY (MML) and SOUND.
 *
 * Everything PAC-GAL plays uses "MB" (music background), so notes simply go
 * into a queue that the audio output drains while the game keeps running.
 * PLAY state (octave, length, tempo, articulation) persists between calls,
 * exactly as in BASIC.
 */
#ifndef PACGAL_SOUND_H
#define PACGAL_SOUND_H

#define SND_QUEUE 1024

typedef struct {
    float freq;          /* Hz, 0 = silence */
    unsigned samples;    /* duration */
} Note;

typedef struct {
    int rate;            /* output sample rate */
    /* MML state */
    int octave, length, tempo;
    double articulation; /* MN 7/8, ML 1, MS 3/4 */
    /* queue */
    Note q[SND_QUEUE];
    int head, tail;
    /* synth */
    unsigned done_in_note;
    double phase;
    float level;
    float gain;          /* current amplitude, slewed to avoid clicks */
    float slew;          /* gain change per sample */
    int muted;
} Sound;

void snd_init(Sound *s, int rate);
void snd_reset_state(Sound *s);             /* BASIC defaults: O4 L4 T120 MN */
void snd_play(Sound *s, const char *mml);   /* PLAY "..." (no X/VARPTR$; expand first) */
void snd_sound(Sound *s, int freq, double ticks); /* SOUND freq, duration-in-clock-ticks */
void snd_render(Sound *s, float *out, int frames); /* mono float32 */
int  snd_pending(const Sound *s);

#endif
