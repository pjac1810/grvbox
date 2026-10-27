/* SPDX-License-Identifier: GPL-3.0-only */
/* GRVBOX: a groovebox for the M-VAVE FM-1, after the Roland MC-303.
 *
 * The panel (README.md has the full map):
 *   SELECT      the part: R (rhythm), 1..7             ALGORITHM  the part's tone (R: the kit; TR-REC: the drum)
 *   PRESETS     the pattern P1..P8 (playing: at its end)
 *   PLAY        start / stop       REC  record in real time (overdub; stopped: starts too)
 *   SEQ         TR-REC: the 16 first keys are the steps of part R (OCT- OCT+ the bar); held 1 s: erase the part
 *   ARP         the arpeggiator on the current part        SCL  TAP tempo
 *   GLO         mute the current part (held + key 1..8: mute that part)
 *   HOME        REALTIME: CUTOFF, RESONANCE, ENV, PORTAMENTO   ENV  TONE, LEVEL, PAN, OCTAVE
 *   LFO         RATE, PITCH, FILTER                FX   REVERB, DELAY, CHORUS, LOW BOOST
 *   EDIT        ARPEGGIO: STYLE, RANGE, RATE, ACCENT (again: PATTERN: TEMPO, SHUFFLE, BARS, KIT)
 *   SAVE        keep the eight patterns in flash
 *   keys        play the part (R: the 16 drums)        OCT- OCT+  the keys' octave
 *   MIDI        channel 10: part R; channels 1..7: parts 1..7 */
#include "fm1.h"
#include "gb.h"
#include "patterns.h"

/* ---------------------------------------------------------------- colours */

#define C_BG RGB(16, 17, 20)
#define C_PANEL RGB(34, 36, 42)
#define C_TEXT RGB(228, 230, 236)
#define C_DIM RGB(120, 124, 136)
#define C_FAINT RGB(52, 55, 64)
#define C_LED RGB(255, 40, 30)
#define C_LED_OFF RGB(42, 10, 10)
#define C_ORANGE RGB(255, 132, 20)
#define C_GREEN RGB(90, 230, 110)
#define C_MUTE RGB(110, 24, 24)
#define C_BLUE RGB(80, 170, 255)

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static char *put(char *d, const char *s) {
  while (*s) *d++ = *s++;
  *d = 0;
  return d;
}
static char *put_int(char *d, int32_t v) {
  char b[12];
  format_int(b, v);
  return put(d, b);
}
static int center_text(int cx, int y, const char *s, uint16_t c, int font) {
  return text(cx - text_width(s, font) / 2, y, s, c, font);
}

/* ---------------------------------------------------------------- the 7-segment LED display */

static uint8_t seg_of(char c) {
  static const uint8_t DIG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  if (c >= '0' && c <= '9') return DIG[c - '0'];
  switch (c) {
    case 'A': case 'a': return 0x77;
    case 'B': case 'b': return 0x7C;
    case 'C': return 0x39;
    case 'c': return 0x58;
    case 'D': case 'd': return 0x5E;
    case 'E': case 'e': return 0x79;
    case 'F': case 'f': return 0x71;
    case 'G': case 'g': return 0x3D;
    case 'H': return 0x76;
    case 'h': return 0x74;
    case 'I': case 'i': return 0x30;
    case 'J': case 'j': return 0x1E;
    case 'L': case 'l': return 0x38;
    case 'N': case 'n': return 0x54;
    case 'O': return 0x3F;
    case 'o': return 0x5C;
    case 'P': case 'p': return 0x73;
    case 'Q': case 'q': return 0x67;
    case 'R': case 'r': return 0x50;
    case 'S': case 's': return 0x6D;
    case 'T': case 't': return 0x78;
    case 'U': return 0x3E;
    case 'u': case 'v': case 'V': return 0x1C;
    case 'Y': case 'y': return 0x6E;
    case '-': return 0x40;
    case '_': return 0x08;
    default: return 0;
  }
}

#define LED_Y 24
#define LED_X 8
#define DIG_W 26
#define DIG_H 40

static void seg_digit(int x, int y, uint8_t m, int dot) {
  const int w = 14, h = 16, t = 3; /* a segment's length and thickness */
  struct {
    int x, y, w, h;
  } S[7] = {{x + t, y, w, t}, {x + w + t, y + t, t, h}, {x + w + t, y + 2 * t + h, t, h}, {x + t, y + 2 * t + 2 * h, w, t},
            {x, y + 2 * t + h, t, h}, {x, y + t, t, h}, {x + t, y + t + h, w, t}};
  for (int i = 0; i < 7; i++) rect(S[i].x, S[i].y, S[i].w, S[i].h, (m >> i & 1u) ? C_LED : C_LED_OFF);
  rect(x + w + 2 * t + 1, y + 2 * t + 2 * h, t, t, dot ? C_LED : C_LED_OFF);
}

static void draw_led(const char *s, uint8_t dots) {
  rect(0, LED_Y - 4, 166, DIG_H + 10, RGB(22, 7, 7));
  for (int i = 0; i < 6; i++) seg_digit(LED_X + 6 + i * DIG_W, LED_Y + 2, s[i] ? seg_of(s[i]) : 0, dots >> i & 1u);
}

/* ---------------------------------------------------------------- the knobs and their pages */

enum { K_PART, K_ARP, K_PAT, K_BOOST };
typedef struct {
  const char *label, *led; /* the LED's two letters */
  uint8_t where;
  int8_t idx; /* PP_*, an ARP field, a pattern field */
  int16_t lo, hi;
} knob_t;

enum { AF_STYLE, AF_RANGE, AF_RATE, AF_ACCENT };
enum { PF_BPM, PF_SHUFFLE, PF_BARS, PF_KIT };

enum { PG_REAL, PG_TONE, PG_LFO, PG_FX, PG_ARP, PG_PAT, PG_COUNT };
static const char *const PAGE_NAMES[PG_COUNT] = {"REALTIME", "PART", "LFO", "EFFECTS", "ARPEGGIO", "PATTERN"};
static const knob_t KNOB[PG_COUNT][4] = {
    {{"CUTOFF", "CU", K_PART, PP_CUT, -64, 63}, {"RESO", "RE", K_PART, PP_RES, -64, 63},
     {"ENV", "EN", K_PART, PP_ENV, -64, 63}, {"PORTA", "PO", K_PART, PP_PORTA, 0, 127}},
    {{"TONE", "TO", K_PART, PP_TONE, 0, TONES - 1}, {"LEVEL", "LE", K_PART, PP_LEVEL, 0, 127},
     {"PAN", "PA", K_PART, PP_PAN, -64, 63}, {"OCTAVE", "OC", K_PART, PP_OCT, -3, 3}},
    {{"RATE", "LR", K_PART, PP_LRATE, 0, 127}, {"PITCH", "LP", K_PART, PP_LDEPTH, 0, 127},
     {"FILTER", "LF", K_PART, PP_LFILT, -64, 63}, {"", "", K_PART, -1, 0, 0}},
    {{"REVERB", "RV", K_PART, PP_REV, 0, 127}, {"DELAY", "DL", K_PART, PP_DLY, 0, 127},
     {"CHORUS", "CH", K_PART, PP_CHO, 0, 127}, {"BOOST", "LB", K_BOOST, 0, 0, 127}},
    {{"STYLE", "AS", K_ARP, AF_STYLE, 0, ARP_STYLES - 1}, {"RANGE", "AR", K_ARP, AF_RANGE, 1, 4},
     {"RATE", "AT", K_ARP, AF_RATE, 0, 3}, {"ACCENT", "AC", K_ARP, AF_ACCENT, 0, 127}},
    {{"TEMPO", "BP", K_PAT, PF_BPM, 40, 240}, {"SHUFFLE", "SH", K_PAT, PF_SHUFFLE, 0, 100},
     {"BARS", "BA", K_PAT, PF_BARS, 1, 3}, {"KIT", "KI", K_PAT, PF_KIT, 0, KITS - 1}},
};

static uint8_t page = PG_REAL, part = 1;

static int32_t knob_get(const knob_t *k) {
  pattern_t *p = &PAT[seq_cur];
  switch (k->where) {
    case K_PART: return k->idx < 0 ? 0 : p->part[part].v[k->idx];
    case K_ARP: return k->idx == AF_STYLE ? ARP.style : k->idx == AF_RANGE ? ARP.range : k->idx == AF_RATE ? ARP.rate : ARP.accent;
    case K_PAT: return k->idx == PF_BPM ? p->bpm : k->idx == PF_SHUFFLE ? p->shuffle : k->idx == PF_BARS ? (p->bars >= 4 ? 3 : p->bars) : p->kit;
    default: return voice_low_boost;
  }
}
static void knob_set(const knob_t *k, int32_t v) {
  v = clampi(v, k->lo, k->hi);
  pattern_t *p = &PAT[seq_cur];
  switch (k->where) {
    case K_PART:
      if (k->idx >= 0) p->part[part].v[k->idx] = (int8_t)v;
      break;
    case K_ARP:
      if (k->idx == AF_STYLE) ARP.style = (uint8_t)v;
      else if (k->idx == AF_RANGE) ARP.range = (uint8_t)v;
      else if (k->idx == AF_RATE) ARP.rate = (uint8_t)v;
      else ARP.accent = (uint8_t)v;
      break;
    case K_PAT:
      if (k->idx == PF_BPM) p->bpm = (uint8_t)v;
      else if (k->idx == PF_SHUFFLE) p->shuffle = (uint8_t)v;
      else if (k->idx == PF_BARS) p->bars = (uint8_t)(v == 3 ? 4 : v); /* 1, 2 or 4 */
      else p->kit = (uint8_t)v;
      break;
    default: voice_low_boost = (uint8_t)v; break;
  }
}
static void knob_text(char *d, const knob_t *k) {
  int32_t v = knob_get(k);
  if (k->where == K_ARP && k->idx == AF_STYLE) put(d, ARP_NAMES[clampi(v, 0, ARP_STYLES - 1)]);
  else if (k->where == K_ARP && k->idx == AF_RATE) put(d, v == 0 ? "1/8" : v == 1 ? "1/16" : v == 2 ? "1/16T" : "1/32");
  else if (k->where == K_ARP && k->idx == AF_RANGE) put(put_int(d, v), " OCT");
  else if (k->where == K_PAT && k->idx == PF_KIT) put(d, KIT_NAMES[clampi(v, 0, KITS - 1)]);
  else if (k->where == K_PAT && k->idx == PF_BARS) put_int(d, v == 3 ? 4 : v);
  else if (k->where == K_PART && k->idx == PP_TONE) put_int(d, v + 1);
  else if (k->lo < 0) put_int(put(d, v > 0 ? "+" : ""), v);
  else put_int(d, v);
}

/* ---------------------------------------------------------------- state */

#define STORE_MAGIC 0x47425831u /* "GBX1" */
static struct {
  uint32_t magic;
  pattern_t pat[PATTERNS];
} store;
_Static_assert(sizeof store <= SETTINGS_MAX, "the patterns must fit the settings' flash");

static int8_t oct;
static uint8_t trrec, tr_drum, tr_bar, key_note[KEY_COUNT];
static uint32_t led_until, seq_down, saved_t, tap_t[4], flash_t[PARTS];
static uint8_t seq_held_erased, tap_n, lit_seen[PARTS];
static char led_msg[8];
static uint8_t led_dots;

/* what is on the screen */
static uint32_t sh_led = ~0u, sh_state = ~0u, sh_parts = ~0u, sh_info = ~0u, sh_steps = ~0u, sh_knobs = ~0u, sh_foot = ~0u;
static uint32_t led_k = ~0u, led_b = ~0u;
static uint8_t drawn;

/* say something on the LED display for a while */
static void led_say(const char *code, int32_t v, uint32_t now) {
  char b[12], *d = put(b, code);
  int len = (int)(d - b);
  char num[12];
  format_int(num, v);
  int nl = 0;
  while (num[nl]) nl++;
  for (int i = 0; i < 6; i++) led_msg[i] = ' ';
  for (int i = 0; i < len && i < 2; i++) led_msg[i] = b[i];
  for (int i = 0; i < nl && i < 4; i++) led_msg[6 - nl + i] = num[i];
  led_msg[6] = 0, led_dots = 0x02, led_until = now + 1500;
}

/* ---------------------------------------------------------------- drawing */

#define PART_Y 76
#define INFO_Y 108
#define STEP_Y 128
#define KNOB_Y 156
#define FOOT_Y 202

static void draw_title(void) {
  rect(0, 0, SCREEN_W, 18, C_PANEL);
  text(6, 1, "GRVBOX", C_ORANGE, FONT_SMALL);
  text(SCREEN_W - 6 - text_width("GROOVEBOX", FONT_SMALL), 1, "GROOVEBOX", C_DIM, FONT_SMALL);
}

static void draw_state(void) {
  rect(168, LED_Y - 4, SCREEN_W - 168, DIG_H + 10, C_BG);
  const char *st = trrec ? "TR-REC" : seq_recording ? "REC" : seq_playing ? "PLAY" : "STOP";
  uint16_t c = trrec ? C_BLUE : seq_recording ? C_LED : seq_playing ? C_GREEN : C_DIM;
  center_text(204, LED_Y, st, c, FONT_SMALL);
  char b[12];
  put_int(put(b, "P"), seq_cur + 1);
  if (seq_next != seq_cur) put_int(put(b + 2, ">"), seq_next + 1);
  center_text(204, LED_Y + 20, b, C_TEXT, FONT_SMALL);
}

static void draw_parts(uint32_t now) {
  rect(0, PART_Y - 2, SCREEN_W, 30, C_BG);
  for (int i = 0; i < PARTS; i++) {
    int x = 4 + i * 29;
    int muted = seq_mute >> i & 1u, act = now - flash_t[i] < 90;
    uint16_t bg = i == part ? C_ORANGE : muted ? C_MUTE : C_PANEL;
    rect(x, PART_Y, 27, 24, bg);
    if (act) rect(x, PART_Y + 21, 27, 3, C_GREEN);
    if (muted) rect(x, PART_Y, 27, 3, C_LED);
    char b[2] = {i == 0 ? 'R' : (char)('0' + i), 0};
    center_text(x + 13, PART_Y + 4, b, i == part ? C_BG : muted ? C_DIM : C_TEXT, FONT_SMALL);
  }
}

static void draw_info(void) {
  rect(0, INFO_Y, SCREEN_W, 18, C_BG);
  char b[32], *d;
  pattern_t *p = &PAT[seq_cur];
  if (part == 0) {
    d = put(put(b, "R  KIT "), KIT_NAMES[clampi(p->kit, 0, KITS - 1)]);
    if (trrec) put(put(d, "  "), drum_name(p->kit, tr_drum));
  } else {
    int t = clampi(p->part[part].v[PP_TONE], 0, TONES - 1);
    d = put(put_int(b, part), "  ");
    put(d, TONE[t].name);
  }
  text(6, INFO_Y, b, C_TEXT, FONT_SMALL);
  if (ARP.on) text(SCREEN_W - 6 - text_width("ARP", FONT_SMALL), INFO_Y, "ARP", C_ORANGE, FONT_SMALL);
}

/* the 16 steps of the bar shown, for the current part (R in TR-REC: the drum chosen) */
static int bar_shown(void) {
  int bars = clampi(PAT[seq_cur].bars, 1, 4);
  if (trrec) return clampi(tr_bar, 0, bars - 1);
  return seq_playing ? (seq_step / 16) % bars : 0;
}
static void draw_steps(void) {
  rect(0, STEP_Y - 2, SCREEN_W, 26, C_BG);
  pattern_t *p = &PAT[seq_cur];
  int bar = bar_shown(), bars = clampi(p->bars, 1, 4);
  uint16_t hit = 0, other = 0;
  for (int i = 0; i < p->nev; i++) {
    const ev_t *e = &p->ev[i];
    if (e->part != part || e->step / 16 != bar) continue;
    if (part == 0 && trrec && e->note != tr_drum) other |= (uint16_t)(1u << (e->step % 16));
    else hit |= (uint16_t)(1u << (e->step % 16));
  }
  for (int s = 0; s < 16; s++) {
    int x = 6 + s * 14 + (s / 4) * 2;
    uint16_t c = (hit >> s & 1u) ? (part ? C_ORANGE : C_LED) : (other >> s & 1u) ? RGB(90, 40, 30) : (s % 4 == 0 ? C_FAINT : C_PANEL);
    rect(x, STEP_Y, 12, 16, c);
    if (seq_playing && seq_step == bar * 16 + s) frame(x - 1, STEP_Y - 1, 14, 18, C_TEXT);
  }
  if (bars > 1)
    for (int b = 0; b < bars; b++) rect(6 + b * 12, STEP_Y + 19, 10, 3, b == bar ? C_TEXT : C_FAINT);
}

static void draw_knobs(void) {
  rect(0, KNOB_Y - 2, SCREEN_W, FOOT_Y - KNOB_Y + 1, C_BG);
  for (int i = 0; i < 4; i++) {
    const knob_t *k = &KNOB[page][i];
    int cx = i * 60 + 30;
    if (!k->label[0]) continue;
    char b[16];
    center_text(cx, KNOB_Y, k->label, C_DIM, FONT_SMALL);
    knob_text(b, k);
    center_text(cx, KNOB_Y + 16, b, C_TEXT, FONT_SMALL);
    int32_t v = knob_get(k), w = 46;
    rect(cx - w / 2, KNOB_Y + 35, w, 3, C_FAINT);
    if (k->lo < 0) {
      int32_t len = (v * (w / 2)) / k->hi;
      if (len > 0) rect(cx, KNOB_Y + 35, len, 3, C_ORANGE);
      else if (len < 0) rect(cx + len, KNOB_Y + 35, -len, 3, C_ORANGE);
    } else {
      rect(cx - w / 2, KNOB_Y + 35, ((v - k->lo) * w) / (k->hi - k->lo ? k->hi - k->lo : 1), 3, C_ORANGE);
    }
  }
}

static void draw_foot(uint32_t now) {
  rect(0, FOOT_Y, SCREEN_W, SCREEN_H - FOOT_Y, C_BG);
  line(0, FOOT_Y, SCREEN_W - 1, FOOT_Y, C_FAINT);
  text(6, FOOT_Y + 4, PAGE_NAMES[page], C_ORANGE, FONT_SMALL);
  char b[24], *d = put(b, "OCT");
  if (oct >= 0) d = put(d, "+");
  put_int(d, oct);
  text(6, FOOT_Y + 22, b, C_DIM, FONT_SMALL);
  pattern_t *p = &PAT[seq_cur];
  d = put(put_int(b, p->bars), p->bars > 1 ? " BARS" : " BAR");
  text(70, FOOT_Y + 22, b, C_DIM, FONT_SMALL);
  d = put(b, "P");
  *d++ = (char)('1' + seq_cur), *d = 0;
  put(put(d, " "), p->name);
  text(SCREEN_W - 6 - text_width(b, FONT_SMALL), FOOT_Y + 4, b, C_TEXT, FONT_SMALL);
  if (now - saved_t < 1500 && saved_t) text(150, FOOT_Y + 22, "SAVED", C_GREEN, FONT_SMALL);
}

/* ---------------------------------------------------------------- setup and loop */

void setup(void) {
  if (settings_load(&store, sizeof store) || store.magic != STORE_MAGIC) {
    for (int i = 0; i < PATTERNS; i++) PAT[i] = FACTORY_PAT[i];
  } else {
    for (int i = 0; i < PATTERNS; i++) PAT[i] = store.pat[i];
  }
  voice_init();
  voice_pat = &PAT[0];
  led_backlight(1);
  clear(C_BG);
}

static void save_all(void) {
  store.magic = STORE_MAGIC;
  for (int i = 0; i < PATTERNS; i++) store.pat[i] = PAT[i];
  settings_save(&store, sizeof store);
}

void loop(void) {
  uint32_t now = millis();
  pattern_t *p = &PAT[seq_cur];
  int32_t e;

  /* ---- buttons */
  if (button_pressed(BTN_PLAY)) seq_play(!seq_playing);
  if (button_pressed(BTN_REC)) seq_rec(!seq_recording);
  if (button_pressed(BTN_ARP)) {
    ARP.on ^= 1;
    for (int k = 0; k < KEY_COUNT; k++) /* the keys held go out with the change */
      if (key_note[k]) seq_key(part, key_note[k] - 1, 0), key_note[k] = 0;
  }
  if (button_pressed(BTN_SEQ)) seq_down = now, seq_held_erased = 0;
  if (button_held(BTN_SEQ) && !seq_held_erased && now - seq_down > 1000) {
    seq_erase_part(part), seq_held_erased = 1, led_say("Er", part, now);
  }
  if (button_released(BTN_SEQ) && !seq_held_erased) {
    trrec ^= 1;
    if (trrec) part = 0, tr_bar = 0;
  }
  if (button_pressed(BTN_GLO)) seq_mute ^= (uint8_t)(1u << part);
  if (button_pressed(BTN_SAVE)) save_all(), saved_t = now ? now : 1, led_say("Sv", seq_cur + 1, now);
  if (button_pressed(BTN_SEL)) { /* TAP tempo: the last intervals' average */
    if (tap_n && now - tap_t[(tap_n - 1) & 3] > 2000) tap_n = 0;
    tap_t[tap_n & 3] = now, tap_n++;
    if (tap_n >= 2) {
      int n = tap_n > 4 ? 4 : tap_n;
      uint32_t span = tap_t[(tap_n - 1) & 3] - tap_t[(tap_n - n) & 3];
      if (span) p->bpm = (uint8_t)clampi((int32_t)(60000u * (uint32_t)(n - 1) / span), 40, 240), led_say("bP", p->bpm, now);
    }
  }
  static const uint8_t PAGE_BTN[] = {BTN_HOME, BTN_ENV, BTN_LFO, BTN_FX};
  for (int i = 0; i < 4; i++)
    if (button_pressed(PAGE_BTN[i])) page = (uint8_t)i;
  if (button_pressed(BTN_EDIT)) page = page == PG_ARP ? PG_PAT : PG_ARP;
  if (button_pressed(BTN_OCT_DOWN) || button_pressed(BTN_OCT_UP)) {
    int up = button_pressed(BTN_OCT_UP);
    if (trrec) tr_bar = (uint8_t)clampi(tr_bar + (up ? 1 : -1), 0, clampi(p->bars, 1, 4) - 1);
    else if (button_held(BTN_OCT_DOWN) && button_held(BTN_OCT_UP)) oct = 0;
    else oct = (int8_t)clampi(oct + (up ? 1 : -1), -3, 3);
  }

  /* ---- encoders */
  if ((e = encoder(ENC_SELECT))) {
    part = (uint8_t)((part + PARTS + (e > 0 ? 1 : -1)) % PARTS);
    if (trrec && part != 0) trrec = 0;
    led_say("Pt", part, now);
  }
  if ((e = encoder(ENC_PRESETS))) {
    int n = (seq_next + PATTERNS + (e > 0 ? 1 : -1)) % PATTERNS;
    seq_select(n), led_say("P", n + 1, now);
  }
  if ((e = encoder(ENC_ALGORITHM))) {
    int d = e > 0 ? 1 : -1;
    if (part == 0 && trrec) tr_drum = (uint8_t)((tr_drum + DRUMS + d) % DRUMS), led_say("dr", tr_drum + 1, now);
    else if (part == 0) p->kit = (uint8_t)((p->kit + KITS + d) % KITS), led_say("Ft", p->kit + 1, now);
    else {
      int t = (p->part[part].v[PP_TONE] + TONES + d) % TONES;
      p->part[part].v[PP_TONE] = (int8_t)t, led_say("t", t + 1, now);
    }
  }
  for (int i = 0; i < 4; i++)
    if ((e = encoder(ENC_KNOB1 + i))) {
      const knob_t *k = &KNOB[page][i];
      if (k->label[0]) {
        int32_t a = e < 0 ? -e : e, fast = (k->hi - k->lo > 20) && a > 1;
        knob_set(k, knob_get(k) + (e > 0 ? 1 : -1) * (fast ? 4 * a : a));
        led_say(k->led, knob_get(k), now);
      }
    }

  /* ---- keys: steps in TR-REC, mutes with GLO held, else the part */
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_pressed(k)) {
      if (button_held(BTN_GLO) && k < PARTS) {
        seq_mute ^= (uint8_t)(1u << k);
      } else if (trrec) {
        if (k < 16) seq_toggle_step(bar_shown() * 16 + k, tr_drum);
        if (!seq_recording) seq_key(0, tr_drum, 1); /* hear it */
      } else if (part == 0) {
        seq_key(0, k % DRUMS, 1);
      } else {
        int n = clampi(KEY_FIRST_NOTE - 12 + k + oct * 12, 0, 127);
        key_note[k] = (uint8_t)(n + 1);
        seq_key(part, n, 1);
      }
    }
    if (key_released(k) && key_note[k]) seq_key(part, key_note[k] - 1, 0), key_note[k] = 0;
  }

  /* ---- MIDI: channel 10 is R, 1..7 the parts */
  uint8_t st, d1, d2;
  while (midi_read(&st, &d1, &d2)) {
    int ch = st & 0x0F, t = st & 0xF0, pt = ch == 9 ? 0 : ch + 1;
    if (pt >= PARTS) continue;
    if (t == 0x90 && d2) seq_key(pt, pt == 0 ? (d1 + 120 - 36) % DRUMS : d1, 1);
    else if ((t == 0x80 || (t == 0x90 && !d2)) && pt) seq_key(pt, d1, 0);
  }

  /* ---- the parts' activity lights */
  for (int i = 0; i < PARTS; i++)
    if (voice_part_lit[i] != lit_seen[i]) lit_seen[i] = voice_part_lit[i], flash_t[i] = now;

  /* ---- LEDs */
  uint32_t lk = 0, lb = 0;
  if (trrec) {
    int bar = bar_shown();
    for (int i = 0; i < p->nev; i++)
      if (p->ev[i].part == 0 && p->ev[i].note == tr_drum && p->ev[i].step / 16 == bar) lk |= 1u << (p->ev[i].step % 16);
    if (seq_playing && seq_step / 16 == bar) lk ^= 1u << (seq_step % 16);
  } else {
    for (int k = 0; k < KEY_COUNT; k++)
      if (key_held(k)) lk |= 1u << k;
  }
  if (seq_playing && (seq_step % 4) < 2) lb |= 1u << BTN_PLAY;
  if (seq_recording) lb |= 1u << BTN_REC;
  if (trrec) lb |= 1u << BTN_SEQ;
  if (ARP.on) lb |= 1u << BTN_ARP;
  if (seq_mute >> part & 1u) lb |= 1u << BTN_GLO;
  lb |= 1u << (page == PG_ARP || page == PG_PAT ? BTN_EDIT : PAGE_BTN[page]);
  if (lk != led_k) {
    for (int i = 0; i < KEY_COUNT; i++)
      if ((lk ^ led_k) >> i & 1u) led_key(i, lk >> i & 1u);
    led_k = lk;
  }
  if (lb != led_b) {
    for (int i = 0; i < BTN_COUNT; i++)
      if ((lb ^ led_b) >> i & 1u) led_button(i, lb >> i & 1u);
    led_b = lb;
  }

  /* ---- the screen, only what changed */
  if (!drawn) clear(C_BG), draw_title(), drawn = 1;
  /* the LED display: a message for a while, else the pattern and the tempo */
  char led[8];
  uint8_t dots = 0;
  if (now < led_until) {
    for (int i = 0; i < 7; i++) led[i] = led_msg[i];
    dots = led_dots;
  } else {
    led[0] = 'P', led[1] = (char)('1' + seq_cur), led[2] = ' ';
    int bpm = p->bpm;
    led[3] = bpm >= 100 ? (char)('0' + bpm / 100) : ' ', led[4] = (char)('0' + (bpm / 10) % 10), led[5] = (char)('0' + bpm % 10);
    led[6] = 0;
    dots = 0x02 | (seq_recording && (seq_step % 4) < 2 ? 0x20 : 0);
  }
  uint32_t h = (uint32_t)dots;
  for (int i = 0; i < 6; i++) h = h * 131u + (uint8_t)led[i];
  if (h != sh_led) sh_led = h, draw_led(led, dots);
  h = (uint32_t)trrec | (uint32_t)seq_recording << 1 | (uint32_t)seq_playing << 2 | (uint32_t)seq_cur << 3 | (uint32_t)seq_next << 6;
  if (h != sh_state) sh_state = h, draw_state();
  h = (uint32_t)part | (uint32_t)seq_mute << 3;
  for (int i = 0; i < PARTS; i++) h = h * 3u + (now - flash_t[i] < 90);
  if (h != sh_parts) sh_parts = h, draw_parts(now);
  h = (uint32_t)part | (uint32_t)p->part[part].v[PP_TONE] << 3 | (uint32_t)p->kit << 9 | (uint32_t)trrec << 11 |
      (uint32_t)tr_drum << 12 | (uint32_t)ARP.on << 17 | (uint32_t)seq_cur << 18;
  if (h != sh_info) sh_info = h, draw_info();
  h = (uint32_t)seq_dirty | (uint32_t)part << 16 | (uint32_t)bar_shown() << 19 | (uint32_t)trrec << 21 | (uint32_t)tr_drum << 22 |
      (uint32_t)seq_cur << 27;
  h = h * 977u + (seq_playing ? seq_step + 1u : 0u) + (uint32_t)p->nev * 7u;
  if (h != sh_steps) sh_steps = h, draw_steps();
  h = (uint32_t)page | (uint32_t)part << 3 | (uint32_t)seq_cur << 6;
  for (int i = 0; i < 4; i++) h = h * 257u + (uint32_t)(knob_get(&KNOB[page][i]) + 128);
  if (h != sh_knobs) sh_knobs = h, draw_knobs();
  h = (uint32_t)page | (uint32_t)(oct + 4) << 3 | (uint32_t)p->bars << 6 | (uint32_t)seq_cur << 9 | (uint32_t)(now - saved_t < 1500) << 12;
  if (h != sh_foot) sh_foot = h, draw_foot(now);
}

/* ---------------------------------------------------------------- the sound */

void audio(int32_t *out, uint32_t frames) { seq_render(out, frames); }
