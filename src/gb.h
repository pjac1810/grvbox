/* SPDX-License-Identifier: GPL-3.0-only */
/* GRVBOX: a groovebox for the M-VAVE FM-1, after the Roland MC-303: eight parts (R = rhythm, 1..7 melodic),
 * patterns of 1, 2 or 4 bars recorded in real time or step by step (TR-REC), an arpeggiator, realtime modify
 * knobs, reverb / delay / chorus sends and a low boost. Shared by the panel (main.c), the sounds (voice.c,
 * tones.c) and the sequencer (seq.c). */
#ifndef GRVBOX_GB_H
#define GRVBOX_GB_H
#include <stdint.h>

#define PARTS 8 /* 0 = R, 1..7 */
#define STEPS_BAR 16
#define MAX_STEPS 64
#define MAX_EV 240
#define PATTERNS 8
#define DRUMS 16
#define KITS 4

/* ---- tones (tones.c) */
enum { OSC_SAW, OSC_SQR, OSC_PULSE, OSC_2SAW, OSC_SINE, OSC_FM, OSC_ORGAN, OSC_NOISE };
typedef struct {
  const char *name;
  uint8_t osc, mono;
  uint8_t detune;          /* cents (2SAW) */
  uint8_t pw;              /* PULSE: 64 = 50 % */
  uint8_t fmratio, fmindex, fmdec; /* FM: ratio x 2, peak index (x 1/16), its decay (time 0..127) */
  uint8_t sub;             /* a square an octave down, 0..127 */
  uint8_t cut, res;        /* the filter, 0..127 */
  int8_t fenv;             /* filter envelope amount */
  uint8_t fa, fd;          /* its attack and decay (time 0..127) */
  uint8_t a, d, s, r;      /* loudness */
  int8_t pitch_env;        /* semitones at the start, falling to 0 */
  uint8_t pdec;
  uint8_t drive, porta, level;
} tone_t;
#define TONES 24
extern const tone_t TONE[TONES];
extern const char *const KIT_NAMES[KITS];
const char *drum_name(int kit, int d);  /* "BD 1" .. */
const char *drum_short(int kit, int d); /* "BD" .. */

/* ---- a part's realtime settings: stored with the pattern, as on the original */
enum { PP_TONE, PP_LEVEL, PP_PAN, PP_CUT, PP_RES, PP_ENV, PP_LRATE, PP_LDEPTH, PP_LFILT, PP_PORTA, PP_REV, PP_DLY,
       PP_CHO, PP_OCT, PP_COUNT };
typedef struct {
  int8_t v[PP_COUNT];
} partset_t;

/* ---- patterns */
typedef struct {
  uint8_t step, part, note, vel, len; /* len in steps; vel bit 7: just recorded ahead, skip once */
} ev_t;
typedef struct {
  char name[12];
  uint8_t bars, bpm, shuffle, kit;
  partset_t part[PARTS];
  uint16_t nev;
  ev_t ev[MAX_EV];
} pattern_t;

extern pattern_t PAT[PATTERNS];

/* ---- voice.c: the sound */
void voice_init(void);
void voice_note_on(int part, int note, int vel);
void voice_note_off(int part, int note);
void voice_part_off(int part);
void voice_all_off(void);
void voice_click(int accent);
void voice_render(int32_t *out, uint32_t frames);
extern const pattern_t *voice_pat;      /* the playing pattern: its kit and parts' settings */
extern volatile uint8_t voice_low_boost; /* 0..127 */
extern volatile uint8_t voice_part_lit[PARTS]; /* bumped when a part sounds (the panel's lights) */

/* ---- seq.c: the clock, the sequencer, the arpeggiator (run in audio()) */
void seq_render(int32_t *out, uint32_t frames);
void seq_key(int part, int note, int on);  /* a live key: through the arpeggiator if on; recorded when recording */
void seq_play(int on);
void seq_rec(int on);
void seq_toggle_step(int step, int drum);  /* TR-REC on R */
void seq_erase_part(int part);
void seq_select(int pattern);              /* now when stopped, at the end of the pattern when playing */

enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_ORDER, ARP_OCTAVE, ARP_ALBERTI, ARP_GUITAR, ARP_REGGAE,
       ARP_WALKING, ARP_STYLES };
typedef struct {
  volatile uint8_t on, style, range, rate, accent; /* range 1..4 octaves; rate 0: 1/8, 1: 1/16, 2: 1/16T, 3: 1/32 */
} arp_t;
extern arp_t ARP;
extern const char *const ARP_NAMES[ARP_STYLES];

extern volatile uint8_t seq_cur, seq_next, seq_playing, seq_recording, seq_click, seq_part;
extern volatile uint8_t seq_step;    /* the step playing */
extern volatile uint8_t seq_mute;    /* bit p: part p muted */
extern volatile uint16_t seq_dirty;  /* bumped when the pattern changes */

#endif
