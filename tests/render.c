/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for GRVBOX: plays each factory pattern twice to build/render-out/N-NAME.wav, records a live line
 * (off the grid) and checks where it landed, runs the arpeggiator in every style, measures the CPU, then fuzzes.
 *   tests/run.sh */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gb.h"
#include "patterns.h"

int32_t sine(uint32_t phase) {
  int32_t x = (int32_t)phase >> 16, ax = x < 0 ? -x : x, y, ay;
  y = (x * (32768 - ax)) >> 13;
  ay = y < 0 ? -y : y;
  y += ((((y * ay) >> 15) - y) * 7373) >> 15;
  return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
static const uint32_t INC_C9[12] = {815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
                                    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
uint32_t note_inc(int note) {
  if (note < 0) note = 0;
  if (note > 127) note = 127;
  return INC_C9[note % 12] >> (10 - note / 12);
}
static uint32_t rs = 0x1234567u;
uint32_t random32(void) {
  rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
  return rs;
}

#define MAXF (44100 * 24)
static int16_t buf[MAXF * 2];
static uint32_t pos;
static int fail;

static void render(uint32_t frames) {
  static int32_t blk[512];
  while (frames) {
    uint32_t n = frames > 256 ? 256 : frames;
    memset(blk, 0, sizeof blk);
    seq_render(blk, n);
    for (uint32_t i = 0; i < 2 * n; i++) {
      if (blk[i] > 32767 || blk[i] < -32767) fail = 1;
      if (pos * 2 + i < MAXF * 2) buf[pos * 2 + i] = (int16_t)blk[i];
    }
    pos += n, frames -= n;
  }
}

static void save(const char *dir, const char *name) {
  char path[600];
  snprintf(path, sizeof path, "%s/%s.wav", dir, name);
  for (char *c = path + strlen(dir) + 1; *c; c++)
    if (*c == ' ') *c = '_';
  FILE *f = fopen(path, "wb");
  uint32_t frames = pos < MAXF ? pos : MAXF, data = frames * 4, riff = 36 + data, rate = 44100, br = rate * 4, fmt = 16;
  uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
  fwrite("RIFF", 1, 4, f), fwrite(&riff, 4, 1, f), fwrite("WAVEfmt ", 1, 8, f), fwrite(&fmt, 4, 1, f);
  fwrite(&pcm, 2, 1, f), fwrite(&ch, 2, 1, f), fwrite(&rate, 4, 1, f), fwrite(&br, 4, 1, f);
  fwrite(&al, 2, 1, f), fwrite(&bits, 2, 1, f), fwrite("data", 1, 4, f), fwrite(&data, 4, 1, f);
  fwrite(buf, 2, frames * 2, f);
  fclose(f);
  double sq = 0;
  int peak = 0, clip = 0;
  for (uint32_t i = 0; i < frames * 2; i++) {
    int v = abs(buf[i]);
    sq += (double)v * v, peak = v > peak ? v : peak, clip += v >= 32700;
  }
  printf("%-16s peak %6d rms %6.0f clip %.3f%%\n", name, peak, sqrt(sq / (frames * 2)), 100.0 * clip / (frames * 2));
  pos = 0;
}

static uint32_t pattern_frames(const pattern_t *p) { return (uint32_t)(44100.0 * 60 / p->bpm * 4 * p->bars); }

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  for (int i = 0; i < PATTERNS; i++) PAT[i] = FACTORY_PAT[i];
  voice_init();
  for (int k = 0; k < PATTERNS; k++) {
    seq_select(k), render(64);
    seq_play(1);
    uint32_t f = pattern_frames(&PAT[k]) * 2;
    if (f > MAXF) f = MAXF;
    clock_t t0 = clock();
    render(f);
    double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    seq_play(0), render(22050);
    char n[40];
    snprintf(n, sizeof n, "%d-%s", k + 1, PAT[k].name);
    printf("  (%.1f ms CPU for %.1f s) ", ms, f / 44100.0);
    save(dir, n);
  }

  /* recording: an empty pattern 8, part 3, eight notes played a little early or late */
  pattern_t *p = &PAT[7];
  p->nev = 0, p->bars = 1, p->bpm = 120;
  seq_select(7), render(64);
  seq_rec(1), render(64);
  uint32_t step = 44100 * 60 / 120 / 4, t = 0; /* t: frames since the recording started */
  for (int s = 0; s < 16; s += 2) {
    int early = (s % 3 == 0) ? -900 : (s % 3 == 1) ? 900 : 0; /* +-20 ms off the grid */
    uint32_t at = s * step + (uint32_t)(early + (s == 0 ? 1000 : 0));
    render(at - t), t = at;
    seq_key(3, 60 + s, 1), render(step / 2), t += step / 2;
    seq_key(3, 60 + s, 0);
  }
  render(16 * step - t);
  seq_rec(0), seq_play(0), render(4410);
  int ok = p->nev == 8;
  for (int i = 0; i < p->nev; i++) ok &= p->ev[i].step == p->ev[i].note - 60 && p->ev[i].part == 3;
  printf("record: %d events, quantised %s\n", p->nev, ok ? "ok" : "WRONG");
  for (int i = 0; ok == 0 && i < p->nev; i++) printf("  step %d note %d len %d\n", p->ev[i].step, p->ev[i].note, p->ev[i].len);
  if (!ok) fail = 1;
  seq_toggle_step(5, 2), seq_toggle_step(5, 2), seq_toggle_step(9, 0), render(256);
  int tr = 0;
  for (int i = 0; i < p->nev; i++) tr += p->ev[i].part == 0;
  printf("TR-REC: %s\n", tr == 1 ? "ok" : "WRONG"), fail |= tr != 1;

  /* the arpeggiator, every style, on a chord */
  pos = 0;
  ARP.on = 1, ARP.range = 2, ARP.rate = 1, ARP.accent = 60;
  for (int st = 0; st < ARP_STYLES; st++) {
    ARP.style = (uint8_t)st;
    seq_key(6, 57, 1), seq_key(6, 60, 1), seq_key(6, 64, 1), render(44100);
    seq_key(6, 57, 0), seq_key(6, 60, 0), seq_key(6, 64, 0), render(8000);
  }
  ARP.on = 0;
  save(dir, "arp-styles");

  /* fuzz */
  for (int i = 0; i < PATTERNS; i++) PAT[i] = FACTORY_PAT[i];
  srand(9);
  for (int b = 0; b < 20000; b++) {
    switch (rand() % 10) {
      case 0: seq_play(rand() & 1); break;
      case 1: seq_rec(rand() & 1); break;
      case 2: seq_select(rand() % PATTERNS); break;
      case 3: seq_toggle_step(rand() % 64, rand() % 16); break;
      case 4: ARP.on = rand() & 1, ARP.style = (uint8_t)(rand() % ARP_STYLES), ARP.rate = (uint8_t)(rand() % 4); break;
      case 5: PAT[seq_cur].part[rand() % 8].v[rand() % PP_COUNT] = (int8_t)(rand() % 256 - 128); break;
      case 6: PAT[seq_cur].shuffle = (uint8_t)(rand() % 101), PAT[seq_cur].bpm = (uint8_t)(40 + rand() % 200); break;
      case 7: if (rand() % 50 == 0) seq_erase_part(rand() % 8); break;
      default: seq_key(rand() % 8, rand() % 128, rand() & 1); break;
    }
    render(256);
    pos = 0;
  }
  int ffail = fail;
  printf("fuzz: 20000 blocks, %s\n", ffail ? "FAILED" : "ok");
  return fail;
}
