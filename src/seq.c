/* SPDX-License-Identifier: GPL-3.0-only */
/* GRVBOX's clock, sequencer and arpeggiator, run in audio().
 *
 * The clock: 96 ticks a beat (24 a sixteenth step), at the pattern's tempo, running always (the arpeggiator
 * plays even when the sequencer is stopped). SHUFFLE delays the odd sixteenths, up to a triplet feel.
 * A pattern is a list of events (step, part, note, velocity, length in steps), kept sorted by step.
 * Recording is an overdub, quantised to the sixteenth: a note lands on the nearest step; if that step is still
 * ahead of the playhead this pass, it is marked so it does not sound twice. TR-REC toggles part R's hits on a
 * step. The arpeggiator takes the keys held for the current part and plays them in one of ten styles. */
#include "fm1.h"
#include "gb.h"

#ifndef RING_PUBLISH
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#endif

#define TPS 24 /* ticks a step */
#define SKIP 0x80u

pattern_t PAT[PATTERNS];
arp_t ARP = {0, ARP_UP, 1, 1, 40};
const char *const ARP_NAMES[ARP_STYLES] = {"UP", "DOWN", "UP&DOWN", "RANDOM", "AS PLAYED", "OCTAVES", "ALBERTI",
                                           "RHYTHM GTR", "REGGAE", "WALKING"};
volatile uint8_t seq_cur, seq_next, seq_playing, seq_recording, seq_click = 1, seq_part = 1;
volatile uint8_t seq_step, seq_mute;
volatile uint16_t seq_dirty;

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---------------------------------------------------------------- the queue from the panel */

enum { Q_KEY_ON, Q_KEY_OFF, Q_PLAY, Q_STOP, Q_REC_ON, Q_REC_OFF, Q_TOGGLE, Q_ERASE, Q_SELECT };
static struct {
  uint8_t t, a, b;
} q[64];
static volatile uint8_t qw, qr;
static void push(uint8_t t, uint8_t a, uint8_t b) {
  uint8_t n = (uint8_t)((qw + 1) & 63);
  if (n == qr) return;
  q[qw].t = t, q[qw].a = a, q[qw].b = b;
  RING_PUBLISH();
  qw = n;
}
void seq_key(int part, int note, int on) { push(on ? Q_KEY_ON : Q_KEY_OFF, (uint8_t)part, (uint8_t)note); }
void seq_play(int on) { push(on ? Q_PLAY : Q_STOP, 0, 0); }
void seq_rec(int on) { push(on ? Q_REC_ON : Q_REC_OFF, 0, 0); }
void seq_toggle_step(int step, int drum) { push(Q_TOGGLE, (uint8_t)step, (uint8_t)drum); }
void seq_erase_part(int part) { push(Q_ERASE, (uint8_t)part, 0); }
void seq_select(int p) { push(Q_SELECT, (uint8_t)p, 0); }

/* ---------------------------------------------------------------- events */

static int steps_of(const pattern_t *p) { return clampi(p->bars, 1, 4) * 16; }

static int ev_insert(pattern_t *p, int step, int part, int note, int vel, int len) {
  if (p->nev >= MAX_EV) return -1;
  int i = p->nev;
  while (i > 0 && p->ev[i - 1].step > step) i--;
  for (int j = p->nev; j > i; j--) p->ev[j] = p->ev[j - 1];
  p->ev[i] = (ev_t){(uint8_t)step, (uint8_t)part, (uint8_t)note, (uint8_t)vel, (uint8_t)len};
  p->nev++;
  seq_dirty++;
  return i;
}
static void ev_remove(pattern_t *p, int i) {
  for (int j = i; j + 1 < p->nev; j++) p->ev[j] = p->ev[j + 1];
  p->nev--;
  seq_dirty++;
}
static int ev_find(const pattern_t *p, int step, int part, int note) {
  for (int i = 0; i < p->nev; i++)
    if (p->ev[i].step == step && p->ev[i].part == part && p->ev[i].note == note) return i;
  return -1;
}

/* ---------------------------------------------------------------- notes sounding, to end them */

#define NOFF 48
static struct {
  uint8_t part, note;
  uint16_t ticks;
} off[NOFF];
static int noff;

static void play(int part, int note, int vel, int ticks) {
  voice_note_on(part, note, vel);
  if (part == 0) return;
  for (int i = 0; i < noff; i++) /* the same note again: it ends at the new time */
    if (off[i].part == part && off[i].note == note) {
      off[i].ticks = (uint16_t)ticks;
      return;
    }
  if (noff < NOFF) off[noff].part = (uint8_t)part, off[noff].note = (uint8_t)note, off[noff].ticks = (uint16_t)ticks, noff++;
}
static void offs_tick(void) {
  for (int i = 0; i < noff;) {
    if (off[i].ticks <= 1) {
      voice_note_off(off[i].part, off[i].note);
      off[i] = off[--noff];
    } else {
      off[i].ticks--, i++;
    }
  }
}
static void offs_all(void) {
  for (int i = 0; i < noff; i++) voice_note_off(off[i].part, off[i].note);
  noff = 0;
}

/* ---------------------------------------------------------------- the clock */

static uint32_t tick, acc, free_tick; /* tick: in the pattern; free_tick: always running */

/* live notes being recorded: where they landed, to set their length when the key is let go */
static struct {
  uint8_t part, note, step, on;
  uint32_t start;
} recn[16];

static void record(int part, int note, int vel) {
  pattern_t *p = &PAT[seq_cur];
  int steps = steps_of(p), t = (int)tick;
  int s = (t + TPS / 2) / TPS, ahead = s * TPS > t;
  if (s >= steps) s -= steps;
  int i = ev_find(p, s, part, note);
  if (i < 0) i = ev_insert(p, s, part, note, vel, 1);
  if (i < 0) return;
  if (ahead) p->ev[i].vel |= SKIP;
  if (part > 0)
    for (int k = 0; k < 16; k++)
      if (!recn[k].on) {
        recn[k].part = (uint8_t)part, recn[k].note = (uint8_t)note, recn[k].step = (uint8_t)s, recn[k].on = 1;
        recn[k].start = free_tick;
        break;
      }
}
static void record_end(int part, int note) {
  for (int k = 0; k < 16; k++)
    if (recn[k].on && recn[k].part == part && recn[k].note == note) {
      recn[k].on = 0;
      pattern_t *p = &PAT[seq_cur];
      int i = ev_find(p, recn[k].step, part, note);
      if (i >= 0) p->ev[i].len = (uint8_t)clampi((int)((free_tick - recn[k].start + TPS / 2) / TPS), 1, steps_of(p));
    }
}

/* ---------------------------------------------------------------- the arpeggiator */

static uint8_t held[8], nheld; /* as played */
static uint32_t arp_k;
static uint8_t arp_part;

static const uint8_t GTR_MASK[16] = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1};

static int sorted_note(int i) { /* the i-th held note, low to high */
  uint8_t s[8];
  for (int a = 0; a < nheld; a++) s[a] = held[a];
  for (int a = 1; a < nheld; a++)
    for (int b = a; b > 0 && s[b - 1] > s[b]; b--) {
      uint8_t t = s[b];
      s[b] = s[b - 1], s[b - 1] = t;
    }
  return s[clampi(i, 0, nheld - 1)];
}

static void arp_step(void) {
  if (!nheld) return;
  int R = ARP.rate == 0 ? 48 : ARP.rate == 1 ? 24 : ARP.rate == 2 ? 16 : 12;
  int range = clampi(ARP.range, 1, 4), n = nheld * range, k = (int)arp_k++;
  int vel = 96, gate = R * 6 / 10;
  if (ARP.accent) vel = (k % 4 == 0) ? clampi(100 + ARP.accent / 4, 1, 127) : clampi(100 - ARP.accent / 3, 30, 127);
  int note = -1;
  switch (ARP.style) {
    case ARP_DOWN: note = sorted_note(nheld - 1 - k % nheld) + 12 * (range - 1 - (k / nheld) % range); break;
    case ARP_UPDOWN: {
      int period = n > 1 ? 2 * n - 2 : 1, j = k % period;
      if (j >= n) j = period - j;
      note = sorted_note(j % nheld) + 12 * (j / nheld);
      break;
    }
    case ARP_RANDOM: {
      uint32_t r = random32();
      note = sorted_note((int)(r % (uint32_t)nheld)) + 12 * (int)((r >> 8) % (uint32_t)range);
      break;
    }
    case ARP_ORDER: note = held[k % nheld] + 12 * ((k / nheld) % range); break;
    case ARP_OCTAVE: { /* a bass line: the root, its octave, the fifth */
      static const int8_t O[8] = {0, 12, 0, 12, 7, 12, 0, 19};
      note = sorted_note(0) + O[k % 8] + 12 * ((k / 8) % range);
      break;
    }
    case ARP_ALBERTI: {
      static const uint8_t A[4] = {0, 2, 1, 2};
      note = sorted_note(A[k % 4] % nheld) + 12 * ((k / 4) % range);
      break;
    }
    case ARP_GUITAR:
    case ARP_REGGAE: { /* chords: every held note at once, on a strum pattern or the off-beats */
      int hit = ARP.style == ARP_GUITAR ? GTR_MASK[k % 16] : (k % 4 == 2);
      if (hit)
        for (int i = 0; i < nheld; i++) play(arp_part, held[i] + 12 * ((k / 16) % range), vel, R * 4 / 10);
      return;
    }
    case ARP_WALKING: { /* through the held notes, then a half step below the root leads back */
      int c = nheld + 1, j = k % c;
      note = j < nheld ? sorted_note(j) : sorted_note(0) - 1;
      note += 12 * ((k / c) % range);
      gate = R * 9 / 10;
      break;
    }
    default: note = sorted_note(k % nheld) + 12 * ((k / nheld) % range); break;
  }
  if (note >= 0) {
    play(arp_part, clampi(note, 0, 127), vel, gate);
    if (seq_recording && seq_playing) record(arp_part, clampi(note, 0, 127), vel), record_end(arp_part, clampi(note, 0, 127));
  }
}

/* ---------------------------------------------------------------- one tick */

static void on_tick(void) {
  pattern_t *p = &PAT[seq_cur];
  int steps = steps_of(p);
  if (seq_playing) {
    int s = (int)(tick / TPS), sub = (int)(tick % TPS);
    int sw = (clampi(p->shuffle, 0, 100) * 8) / 100;
    if (sub == 0) seq_step = (uint8_t)s;
    if ((s % 2 == 0 && sub == 0) || (s % 2 == 1 && sub == sw)) {
      for (int i = 0; i < p->nev; i++) {
        ev_t *e = &p->ev[i];
        if (e->step != s) continue;
        if (e->vel & SKIP) {
          e->vel &= (uint8_t)~SKIP;
          continue;
        }
        if (seq_mute >> e->part & 1u) continue;
        play(e->part, e->note, e->vel, clampi(e->len * TPS - 3, 2, 60000));
      }
    }
    if (seq_click && seq_recording && tick % (TPS * 4) == 0) voice_click(tick % (TPS * 16) == 0);
  }
  int R = ARP.rate == 0 ? 48 : ARP.rate == 1 ? 24 : ARP.rate == 2 ? 16 : 12;
  if (ARP.on && free_tick % (uint32_t)R == 0) arp_step();
  offs_tick();
  free_tick++;
  if (seq_playing && ++tick >= (uint32_t)(steps * TPS)) { /* the pattern's end */
    tick = 0;
    if (seq_next != seq_cur) {
      offs_all();
      seq_cur = seq_next;
      voice_pat = &PAT[seq_cur];
    }
  }
}

static void take(void) {
  while (qr != qw) {
    uint8_t t = q[qr].t, a = q[qr].a, b = q[qr].b;
    qr = (uint8_t)((qr + 1) & 63);
    pattern_t *p = &PAT[seq_cur];
    switch (t) {
      case Q_KEY_ON:
        if (ARP.on && a > 0) { /* the arpeggiator takes it */
          if (nheld < 8) held[nheld++] = b;
          if (nheld == 1) arp_k = 0, arp_part = a;
          break;
        }
        voice_note_on(a, b, 100);
        if (seq_recording && seq_playing) record(a, b, 100);
        break;
      case Q_KEY_OFF: {
        int j = 0;
        for (int i = 0; i < nheld; i++)
          if (held[i] != b) held[j++] = held[i];
        nheld = (uint8_t)j;
        if (!(ARP.on && a > 0)) voice_note_off(a, b);
        record_end(a, b);
        break;
      }
      case Q_PLAY:
        if (!seq_playing) tick = 0, acc = 0, seq_playing = 1;
        break;
      case Q_STOP:
        seq_playing = 0, seq_recording = 0;
        offs_all();
        voice_all_off();
        if (seq_next != seq_cur) seq_cur = seq_next, voice_pat = &PAT[seq_cur];
        break;
      case Q_REC_ON:
        seq_recording = 1;
        if (!seq_playing) tick = 0, acc = 0, seq_playing = 1;
        break;
      case Q_REC_OFF: seq_recording = 0; break;
      case Q_TOGGLE: {
        int i = ev_find(p, a, 0, b);
        if (i >= 0) ev_remove(p, i);
        else ev_insert(p, a, 0, b, 100, 1);
        break;
      }
      case Q_ERASE:
        for (int i = p->nev - 1; i >= 0; i--)
          if (p->ev[i].part == a) ev_remove(p, i);
        if (a) voice_part_off(a);
        break;
      case Q_SELECT:
        seq_next = (uint8_t)(a % PATTERNS);
        if (!seq_playing) seq_cur = seq_next, voice_pat = &PAT[seq_cur];
        break;
    }
  }
}

void seq_render(int32_t *out, uint32_t frames) {
  if (!voice_pat) voice_pat = &PAT[seq_cur];
  take();
  int32_t bpm = clampi(PAT[seq_cur].bpm, 40, 240);
  uint32_t spt = 7056000u / (uint32_t)bpm; /* samples a tick, Q8: 44100 x 60 x 256 / (bpm x 96) */
  uint32_t done = 0;
  while (done < frames) {
    if (acc >= spt) acc = spt - 1;
    uint32_t until = (spt - acc + 255u) >> 8, n = frames - done;
    if (until < n) n = until;
    if (n == 0) n = 1;
    voice_render(out + 2 * done, n);
    done += n, acc += n << 8;
    if (acc >= spt) acc -= spt, on_tick();
  }
}
