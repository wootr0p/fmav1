/* SPDX-License-Identifier: GPL-3.0-only */
/* FMav-1: a small semi-modular mono synth for the M-VAVE FM-1, control for control and jack for jack after its reference
 * instrument's user's manual: one
 * VCO (saw > pulse, pulse width, PWM), the four-pole ladder VCF, the LFO (0.1 .. 550 Hz, triangle > square), the
 * ADSR EG, the VCA with its MODE switch, the keyboard (low note, legato triggering, glide, KB SCALE), the wave folder,
 * the S+H, the ONE+TWO mixer, the attenuator, the mult, and the 24-point patchbay (13 inputs, 11 outputs) with its
 * normals. One monophonic track (the user's choice: one voice first), built for four as Felucca's (-DMFM_NTRK=4u: MIDI
 * channels N .. N+3, ALGORITHM picks the panel's). Played from outside (a Digitakt over the TRS MIDI IN, USB-MIDI) or
 * from the keys. No sequencer (the Digitakt sequences it).
 * Built on OberVave 3.3.2 (with the saves in silence, the fixed MIDI channels, the CPU report) on Dinghy's platform (Felucca by Leo Kuroshita / Hügelton Instruments, SLOOP by isod89); the ladder after
 * FMini-1 (Ruppert).
 *
 *   mfm_params.h    the parameter table: the panel's controls and jacks, MIDI CC / NRPN
 *   mfm_dsp.h       the VCO, the LFO, the folder, the ladder, tanh
 *   mfm_engine.h    the track: keyboard, EG, the patchbay a sample at a time, arpeggiator (the audio ISR)
 *   mfm_patches.h   the factory programs
 *   mfm_ui.h        the drawing; mfm_graph.h the graphs
 *   this file       the panel, MIDI, the storage, what to draw when
 *
 * Integers only (no FPU, no 64-bit division); tests/host.c runs this on a computer. FMav-1 is an independent work: it
 * is not made, endorsed or supported by any synthesizer maker, and contains none of their code, sounds or graphics. */
static void app_idle(void);
#define LCD_IDLE() app_idle()                   /* (lcd.c: while the DMA runs) */
#define APP_NAME "FMAV-1"
#define CV_MAX (240u * 192u)                    /* (0.2: the patchbay draws 240 x 192 at once) */
#include "platform.c"
#include "mfm_params.h"
#include "mfm_dsp.h"
#include "mfm_engine.h"
#include "mfm_patches.h"
#include "mfm_ui.h"
#include "cpu_meter.h"

static cpu_meter_t cpu_m;

static void app_audio(int32_t *out, uint32_t n)
{
#ifndef OBV_HOST
    uint32_t t0 = fm1_ticks();
#endif
    engine_audio(out, n);
#ifndef OBV_HOST
    ui_render_us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    cpu_meter_add(&cpu_m, ui_render_us, fm1_ms);
#endif
}

/* ---- what survives a power-off: the globals (the settings sector) and the user programs (their own sectors) */
#define SET_MAGIC 0x314D464Du                   /* "MFM1" */
#define SET_VERSION (1u + 16u * NTRK)           /* (the layout follows NTRK) */
typedef struct {
    uint32_t magic;
    uint8_t version, trk, midi_ch, tempo;       /* trk: the track on the panel; midi_ch 0 = omni (the panel's track),
                                                 * else 1..16: track 1's, track 2 the next one ... */
    int8_t tune, oct;                           /* tune: cents -50..50; oct: the keys' octave -3..3 */
    uint8_t kvel, lights;                       /* kvel: the keys' velocity; lights: the backlight + 1 (0: the default) */
    uint8_t tprog[NTRK], lvl[NTRK];             /* each track's program; the mixer's level (100 = unity) */
    int8_t pan[NTRK];                           /* the mixer's pan -64..64 */
    uint8_t fx_off, spare[3];                   /* 0.3: REC off: every effect off (the sounds keep theirs) */
} settings_t;
static settings_t set;
#define SET_SIZE_02 __builtin_offsetof(settings_t, fx_off)   /* (0.2's save: up to pan) */
static const uint8_t TPROG_DEF[4] = {0, 10, 20, 30};   /* (CLASSIC BASS, CLASSIC LEAD, S+H FILTER, KICK) */
static uint32_t set_dirty, set_t;
#define LFO_STEADY_LED 1947846u                 /* ~20 Hz: the LFO button's LED lit steady over it */
#define LFO_STEADY_SCR 779138u                  /* ~8 Hz: the screen's (30 frames a second) */

/* every button and key a little lit, always (the LED driver's backlight layer); GLOBAL page 2: LIGHTS */
#define LIGHTS_DEF 4u                           /* 2 us a frame, ~1/48 of a lit LED */
static uint32_t lights_level(void) { return set.lights && set.lights <= 11u ? set.lights - 1u : LIGHTS_DEF; }
static void lights_apply(void)
{
    uint32_t i, l = lights_level();
    for (i = 0; i < 14u; i++) fm1_led_key_bg(i, l > 0u);
    for (i = 0; i < 27u; i++) fm1_led_key_bg(KEY_LED(i), l > 0u);
    fm1_backlight_ns(l * 500u);
}

#define PS_N 40u                                /* programs a sector */
#define PS_TAG 0x334D464Du                      /* "MFM3": 0.3's layout (the clock appended) */
#define PS_TAG_02 0x324D464Du                   /* "MFM2": 0.2's (patch_from_02) */
#define PS_TAG_01 0x504D464Du                   /* "MFMP": 0.1's (patch_from_01) */
typedef struct { uint32_t tag, used[2]; patch_t p[PS_N]; } psect_t;
_Static_assert(sizeof(psect_t) <= SETTINGS_MAX, "a program sector is too big");
_Static_assert(NPSECT * PS_N >= NPROG, "not enough program sectors");
static psect_t psect[NPSECT] __attribute__((section(".pool")));
static uint32_t psect_dirty;                    /* bit i: sector i waits for a quiet moment */

/* a program saved by 0.1: its 45 parameters as they were (every one explicit), the effects off, its name moved from
 * 45 to NPARAM */
static void patch_from_01(patch_t *p)
{
    patch_t q;
    uint32_t i;
    patch_defaults(&q);
    for (i = 0; i < NPARAM_01; i++) q.v[i] = p->v[i];
    for (i = 0; i < PATCH_NAME; i++) q.v[NPARAM + i] = p->v[NPARAM_01 + i];
    *p = q;
}
/* a program saved by 0.2: its 66 parameters, the clock's at their defaults, the name moved from 66 to NPARAM */
static void patch_from_02(patch_t *p)
{
    patch_t q;
    uint32_t i;
    patch_defaults(&q);
    for (i = 0; i < NPARAM_02; i++) q.v[i] = p->v[i];
    for (i = 0; i < PATCH_NAME; i++) q.v[NPARAM + i] = p->v[NPARAM_02 + i];
    *p = q;
}
static int prog_used(uint32_t n) { return (int)(psect[n / PS_N].used[(n % PS_N) >> 5] >> (n % 32u) & 1u); }
static void prog_get(uint32_t n, patch_t *p)    /* program n: the user's, else the factory's, else INIT */
{
    n %= NPROG;
    if (prog_used(n)) *p = psect[n / PS_N].p[n % PS_N];
    else if (n < NFACT) patch_from_factory(p, &FACTORY[n]);
    else patch_defaults(p);
    patch_fix_jacks(p->v);                      /* (0.3: a cable from the mult or VCA OUT) */
}
static void prog_put(uint32_t n, const patch_t *p)
{
    n %= NPROG;
    psect[n / PS_N].p[n % PS_N] = *p;
    psect[n / PS_N].used[(n % PS_N) >> 5] |= 1u << (n % 32u);
    psect_dirty |= 1u << (n / PS_N);
}

/* ---- the panel's state */
#define ctrk (set.trk % NTRK)                   /* the track on the panel */
#define live (live_p[ctrk])
static uint8_t edited[NTRK];
static uint32_t save_armed, save_to, save_t;
static patch_t undo_p;
static uint32_t undo_ok, undo_trk;
static uint32_t dirty_hdr = 1, dirty_page = 1, dirty_map = 1, dirty_knob, dirty_graph = 1, t_graph, graph_played;
static const char *toast;
static uint32_t toast_t;
static char toast_buf[40];
static void say(const char *s) { toast = s; toast_t = fm1_ms; dirty_map = 1; }

/* the sections: a button each, a list of items (the button tapped again steps through them), each with up to five
 * pages of four knobs (SELECT turns the pages, across the items). 0.3 (the user): no switch on SELECT any more: what
 * it switched is a knob (VCA MODE, the effects' ON, the cables), and SELECT pressed opens UTL (the folder, the mixer,
 * the attenuator). A module's input jacks sit on its own pages too (their knob picks the cable's output); SEQ: the
 * whole patchbay, then all its inputs on knobs. */
enum { AC_PERF, AC_VCO, AC_VCF, AC_ENV, AC_LFO, AC_UTL, AC_JACK, AC_GLOB, AC_FX };
#define V_MIDICH 200                            /* virtual knobs: the globals, the mixer */
#define V_TUNE 201
#define V_TEMPO 202
#define V_KVEL 203
#define V_LIGHTS 204
#define V_LVL1 208                              /* the mixer: the levels, the pans (NTRK each) */
#define V_PAN1 212
#define NONE (-1)
/* an item: its title, which knobs on which page, its accent, the module tab it lights, each page's graph and subtitle,
 * a hint for the footer */
typedef struct { const char *title; uint8_t npg, acc; int8_t tab; uint8_t gr[5]; const char *sub[5]; int16_t p[5][4]; const char *hint; } item_t;

static const item_t IT_PERF[] = {
    {"PERFORM", 3, AC_PERF, TAB_NONE, {G_VCF, G_VCO, G_SCOPE}, {0, 0, 0},
        {{P_CUTOFF, P_RES, P_VCF_AMT, P_DECAY}, {P_WAVE, P_PW, P_LFO_RATE, P_GLIDE}, {P_PITCH_AMT, P_PWM_AMT, P_FOLD, P_VOLUME}},
        "SELECT: THE|NEXT PAGES"},
#if NTRK > 1
    {"MIXER", 2, AC_PERF, TAB_NONE, {G_TRACKS, G_TRACKS}, {"LEVELS", "PANS"},
        {{V_LVL1, V_LVL1 + 1, V_LVL1 + 2, V_LVL1 + 3}, {V_PAN1, V_PAN1 + 1, V_PAN1 + 2, V_PAN1 + 3}}, "ALGORITHM:|THE TRACK"},
#else
    {"MIXER", 1, AC_PERF, TAB_NONE, {G_TRACKS}, {0}, {{V_LVL1, V_PAN1, NONE, NONE}}, "THE TRACK'S|LEVEL AND PAN"},
#endif
};
static const item_t IT_EDIT[] = {
    {"VCO", 3, AC_VCO, TAB_VCO, {G_VCO, G_VMOD, G_PATCH}, {0, "MOD", "INPUTS"},
        {{P_PITCH, P_WAVE, P_PW, P_FINE}, {P_VCO_MIX, P_PITCH_AMT, P_PWM_AMT, NONE}, {PJ(I_1VOCT), PJ(I_PWM), NONE, NONE}},
        "WAVE: SAW >|MIX > PULSE"},
    {"VCF", 2, AC_VCF, TAB_VCF, {G_VCF, G_PATCH}, {"LADDER", "INPUTS"},
        {{P_CUTOFF, P_RES, P_VCF_MIX, P_VCF_AMT}, {PJ(I_CUTOFF), PJ(I_FOLD), P_FOLD, NONE}},
        "KB CV > CUTOFF:|KEY TRACKING"},
};
static const item_t IT_ENV[] = {
    {"EG", 2, AC_ENV, TAB_EG, {G_EG, G_PATCH}, {"ADSR", "INPUTS"},
        {{P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE}, {PJ(I_GATE), NONE, NONE, NONE}}, "THE EG OPENS|THE VCA"},
    {"VCA", 1, AC_ENV, TAB_VCA, {G_VCA}, {0}, {{P_VOLUME, P_VCA_MODE, PJ(I_VCACV), NONE}},
        "MODE ON:|A DRONE"},
};
static const item_t IT_LFO[] = {
    {"LFO", 1, AC_LFO, TAB_LFO, {G_LFO}, {0}, {{P_LFO_RATE, P_LFO_WAVE, PJ(I_LFORATE), P_LFO_SYNC}},
        "SYNC: LOCKED|TO THE CLOCK"},
    {"S+H", 1, AC_LFO, TAB_UTL, {G_SH}, {"SAMPLE + HOLD"}, {{PJ(I_SHIN), PJ(I_SHGATE), NONE, NONE}},
        "NO CABLES: THE|VCO BY THE LFO"},
    {"CLOCK", 1, AC_LFO, TAB_LFO, {G_CLK}, {"MIDI / TEMPO"}, {{P_CLK_DIV, P_CLK_WIDTH, V_TEMPO, NONE}},
        "PATCH CLOCK|INTO ANY INPUT"},
};
static const item_t IT_PATCH[] = {             /* SEQ: the whole patchbay, then its 12 inputs in the panel's order */
    {"PATCHBAY", 4, AC_JACK, TAB_JACK, {G_PATCH, G_PATCH, G_PATCH, G_PATCH}, {"ALL CABLES", "INPUTS 1", "INPUTS 2", "INPUTS 3"},
        {{NONE, NONE, NONE, NONE}, {PJ(I_FOLD), PJ(I_1VOCT), PJ(I_PWM), PJ(I_ONE)}, {PJ(I_LFORATE), PJ(I_CUTOFF), PJ(I_GATE), PJ(I_VCACV)},
         {PJ(I_TWO), PJ(I_SHIN), PJ(I_SHGATE), PJ(I_ATTN)}},
        "TURN SELECT:|THE CABLES"},
};
static const item_t IT_UTL[] = {               /* SELECT pressed (0.3): the utilities the manual calls the patchbay's */
    {"WAVE FOLDER", 1, AC_UTL, TAB_UTL, {G_FOLD}, {0}, {{P_FOLD, PJ(I_FOLD), NONE, NONE}},
        "VCO > FOLD IN:|IT FOLDS"},
    {"MIXER", 1, AC_UTL, TAB_UTL, {G_UMIX}, {"ONE + TWO"}, {{P_ONE_LVL, PJ(I_ONE), PJ(I_TWO), NONE}},
        "NO CABLE IN|ONE: -5 V"},
    {"ATTENUATOR", 1, AC_UTL, TAB_UTL, {G_ATTN}, {0}, {{P_ATTN, PJ(I_ATTN), NONE, NONE}},
        "NO CABLE IN|ATTN: +5 V"},
};
static const item_t IT_FX[] = {                 /* FX: the chorus, the delay and the reverb (shared, a send each) */
    {"CHORUS", 2, AC_FX, TAB_FX, {G_CHO, G_CHO}, {0, 0}, {{P_CH_ON, P_CH_TYPE, P_CH_MIX, NONE}, {P_CH_RATE, P_CH_DEPTH, NONE, NONE}},
        "REC: EVERY|EFFECT OFF / ON"},
    {"DELAY", 3, AC_FX, TAB_FX, {G_DLY, G_DLY, G_DLY}, {0, 0, 0}, {{P_DL_ON, P_DL_SEND, P_DL_TIMEL, P_DL_TIMER},
        {P_DL_FB, P_DL_PING, P_DL_WIDTH, NONE}, {P_DL_LOW, P_DL_HIGH, NONE, NONE}}, "REC: EVERY|EFFECT OFF / ON"},
    {"REVERB", 2, AC_FX, TAB_FX, {G_REV, G_REV}, {0, 0}, {{P_RV_ON, P_RV_SEND, P_RV_DECAY, P_RV_DAMP},
        {P_RV_PRE, P_RV_DIFF, P_RV_WIDTH, NONE}}, "REC: EVERY|EFFECT OFF / ON"},
};
static const item_t IT_ARP[] = { {"ARPEGGIATOR", 1, AC_GLOB, TAB_NONE, {G_ARP}, {0},
    {{P_ARP_MODE, P_ARP_OCT, P_ARP_RATE, P_ARP_GATE}}, "ARP: ON / OFF|MIDI CLOCK: SYNC"} };
static const item_t IT_GLO[] = {
    {"KEYBOARD", 1, AC_GLOB, TAB_NONE, {G_KB}, {0}, {{P_KB_SCALE, P_GLIDE, P_PRIO, P_RETRIG}},
        "LOW NOTE,|LEGATO: CLASSIC"},
    {"VOICE", 1, AC_GLOB, TAB_NONE, {G_SCOPE}, {0}, {{P_BEND, P_DRIFT, NONE, NONE}}, "ANALOG: THE|DRIFT"},
    {"GLOBAL", 2, AC_GLOB, TAB_NONE, {G_SCOPE, G_SCOPE}, {0, 0}, {{V_MIDICH, V_TUNE, V_TEMPO, V_KVEL}, {V_LIGHTS, NONE, NONE, NONE}},
        NTRK > 1 ? "CH N: TRACK 1|N+1: TRACK 2 .." : "MIDI CH: THE|TRACK'S CHANNEL"},
};
enum { S_PERF, S_EDIT, S_ENV, S_LFO, S_FX, S_PATCH, S_ARP, S_GLO, S_UTL, NSEC };
typedef struct { const item_t *it; uint8_t n, btn; } section_t;
static const section_t SEC[NSEC] = {
    {IT_PERF, 2, BTN_HOME}, {IT_EDIT, 2, BTN_EDIT}, {IT_ENV, 2, BTN_ENV}, {IT_LFO, 3, BTN_LFO}, {IT_FX, 3, BTN_FX},
    {IT_PATCH, 1, BTN_SEQ}, {IT_ARP, 1, BTN_ARP}, {IT_GLO, 3, BTN_GLO}, {IT_UTL, 3, BTN_SEL},
};
static uint32_t sec = S_PERF, sec_item[NSEC], item_pg[NSEC][8];

static const item_t *item_of(uint32_t s, uint32_t i) { return &SEC[s].it[i % SEC[s].n]; }
static const item_t *cur_item(void) { return item_of(sec, sec_item[sec]); }
static uint32_t cur_pg(void) { return item_pg[sec][sec_item[sec] % 8u] % cur_item()->npg; }
static int32_t page_param(uint32_t k) { return cur_item()->p[cur_pg()][k & 3u]; }   /* the parameter on knob k, -1: none */

static int is_jack(int32_t id) { return id >= P_JACK && id < P_JACK + (int32_t)NJIN; }
static int32_t jack_step(int32_t id, int32_t v, int32_t e)   /* e detents on a jack's knob: its next choices (JORDER) */
{
    uint32_t in = (uint32_t)(id - P_JACK);
    int32_t i = 0, w;
    while (i < (int32_t)NJORDER && JORDER[i] != v) i++;
    if (i >= (int32_t)NJORDER) i = 0;
    for (; e > 0; e--) { w = i; do w++; while (w < (int32_t)NJORDER && !jack_ok(in, JORDER[w])); if (w < (int32_t)NJORDER) i = w; }
    for (; e < 0; e++) { w = i; do w--; while (w > 0 && !jack_ok(in, JORDER[w])); if (w >= 0) i = w; }
    return JORDER[i];
}
static uint32_t ncables(const uint8_t *v)
{
    uint32_t i, n = 0;
    for (i = 0; i < NJIN; i++) n += v[PJ(i)] != J_NONE;
    return n;
}

/* ---- parameters, real and virtual */
static uint32_t pmax(int32_t id)
{
    if (id >= 0 && id < NPARAM) return PDEF[id].max;
    switch (id) {
    case V_MIDICH: return 16;
    case V_TUNE: return 100;
    case V_TEMPO: return 200;
    case V_LIGHTS: return 10;
    default:
        if (id >= V_PAN1 && id < V_PAN1 + 4) return 128;
        return 127;
    }
}
static int32_t pget(int32_t id)
{
    if (id >= 0 && id < NPARAM) return live.v[id];
    switch (id) {
    case V_MIDICH: return set.midi_ch;
    case V_TUNE: return set.tune + 50;
    case V_TEMPO: return set.tempo - 40;
    case V_KVEL: return set.kvel;
    case V_LIGHTS: return (int32_t)lights_level();
    default:
        if (id >= V_LVL1 && id < V_LVL1 + (int32_t)NTRK) return set.lvl[id - V_LVL1];
        if (id >= V_PAN1 && id < V_PAN1 + (int32_t)NTRK) return set.pan[id - V_PAN1] + 64;
        return 0;
    }
}
static void mark_edited_t(uint32_t t) { t %= NTRK; if (!edited[t]) { edited[t] = 1; if (t == ctrk) dirty_hdr = 1; } }
static void mark_edited(void) { mark_edited_t(ctrk); }
static void pset_t(uint32_t t, int32_t id, int32_t v)   /* track t's sound, or a global */
{
    patch_t *pp = &live_p[t % NTRK];
    v = clampi(v, 0, (int32_t)pmax(id));
    if (id >= 0 && id < NPARAM) {
        if (pp->v[id] == (uint8_t)v) return;
        pp->v[id] = (uint8_t)v;
        mark_edited_t(t);
        if (id == P_CH_TYPE && v != CT_FREE) {  /* (the chorus TYPE writes its RATE and DEPTH; turned by hand: FREE) */
            pp->v[P_CH_RATE] = CT_RATE[v]; pp->v[P_CH_DEPTH] = CT_DEPTH[v];
            if (t % NTRK == ctrk) dirty_page = 1;
        }
        if ((id == P_CH_RATE || id == P_CH_DEPTH) && pp->v[P_CH_TYPE] != CT_FREE
            && (pp->v[P_CH_RATE] != CT_RATE[pp->v[P_CH_TYPE] % 3u] || pp->v[P_CH_DEPTH] != CT_DEPTH[pp->v[P_CH_TYPE] % 3u])) {
            pp->v[P_CH_TYPE] = CT_FREE;
            if (t % NTRK == ctrk) dirty_page = 1;
        }
        if (t % NTRK == ctrk) {
            if (PDEF[id].max < 24u) dirty_page = 1;   /* (a switch, a jack: the page's values may follow it) */
            if (id == P_VCA_MODE || id == P_ARP_MODE || is_jack(id) || id == P_FOLD) dirty_hdr = 1;   /* (the tags) */
            dirty_map = dirty_graph = 1;
        }
        return;
    }
    switch (id) {
    case V_MIDICH: set.midi_ch = (uint8_t)v; break;
    case V_TUNE: set.tune = (int8_t)(v - 50); ctl_tune8 = set.tune * 256 / 100; break;
    case V_TEMPO: set.tempo = (uint8_t)(v + 40); ctl_tempo = set.tempo; break;
    case V_KVEL: set.kvel = (uint8_t)(v ? v : 1); break;
    case V_LIGHTS: set.lights = (uint8_t)(v + 1); lights_apply(); break;
    default:
        if (id >= V_LVL1 && id < V_LVL1 + (int32_t)NTRK) { set.lvl[id - V_LVL1] = (uint8_t)v; ctl_lvl[id - V_LVL1] = (uint8_t)v; break; }
        if (id >= V_PAN1 && id < V_PAN1 + (int32_t)NTRK) { set.pan[id - V_PAN1] = (int8_t)(v - 64); ctl_pan[id - V_PAN1] = (int8_t)(v - 64); break; }
        return;
    }
    set_dirty = 1; set_t = fm1_ms;
}
static void pset(int32_t id, int32_t v) { pset_t(ctrk, id, v); }
static const char *plabel(int32_t id)
{
    static const char *const LVL[4] = {"LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4"}, *const PAN[4] = {"PAN 1", "PAN 2", "PAN 3", "PAN 4"};
    if (id >= 0 && id < NPARAM) return PDEF[id].name;
    switch (id) {
    case V_MIDICH: return "MIDI CH";
    case V_TUNE: return "TUNE";
    case V_TEMPO: return "TEMPO";
    case V_KVEL: return "KEY VEL";
    case V_LIGHTS: return "LIGHTS";
    default:
        if (id >= V_LVL1 && id < V_LVL1 + 4) return NTRK > 1u ? LVL[id - V_LVL1] : "LEVEL";
        return NTRK > 1u ? PAN[(id - V_PAN1) & 3] : "PAN";
    }
}

/* ---- numbers to text (no printf on the chip) */
static uint32_t slen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static void scat(char *d, const char *s) { d += slen(d); while ((*d++ = *s++)) ; }
static void fmt_u(char *s, uint32_t v)
{
    char t[12];
    int i = 0;
    do { t[i++] = (char)('0' + v % 10u); v /= 10u; } while (v && i < 11);
    while (i) *s++ = t[--i];
    *s = 0;
}
static void fmt_s(char *s, int32_t v) { if (v > 0) *s++ = '+'; else if (v < 0) { *s++ = '-'; v = -v; } fmt_u(s, (uint32_t)v); }
static void fmt_frac(char *s, int32_t t, uint32_t dec, int sign)   /* t / 10^dec: 43, 1 -> "4.3"; sign: "+4.3" */
{
    uint32_t a = (uint32_t)(t < 0 ? -t : t), p = dec == 2u ? 100u : 10u;
    if (t < 0) *s++ = '-'; else if (sign && t > 0) *s++ = '+';
    fmt_u(s, a / p);
    s += slen(s);
    *s++ = '.';
    if (dec == 2u) *s++ = (char)('0' + a / 10u % 10u);
    *s++ = (char)('0' + a % 10u); *s = 0;
}
static void fmt_hz(char *s, uint32_t hz)        /* 450, 1.2k, 12k */
{
    if (hz < 1000u) { fmt_u(s, hz); return; }
    if (hz < 10000u) { fmt_frac(s, (int32_t)(hz / 100u), 1, 0); scat(s, "k"); return; }
    fmt_u(s, hz / 1000u); scat(s, "k");
}
static void fmt_ms(char *s, uint32_t ms_x10)    /* 0.8ms, 12ms, 1.2s */
{
    if (ms_x10 < 100u) { fmt_frac(s, (int32_t)ms_x10, 1, 0); scat(s, "ms"); }
    else if (ms_x10 < 10000u) { fmt_u(s, ms_x10 / 10u); scat(s, "ms"); }
    else { fmt_frac(s, (int32_t)(ms_x10 / 1000u), 1, 0); scat(s, "s"); }
}
static void fmt_lhz(char *s, uint32_t h)        /* Hz x 100: 0.10Hz, 4.5Hz, 120Hz */
{
    if (h < 1000u) fmt_frac(s, (int32_t)h, 2, 0);
    else if (h < 10000u) fmt_frac(s, (int32_t)(h / 10u), 1, 0);
    else fmt_u(s, h / 100u);
    scat(s, "Hz");
}
static uint32_t cut_hz(int32_t v)               /* the CUTOFF knob's frequency */
{
    int32_t c8 = CUT_LO8 + v * 16 * (CUT_SPAN8 >> 3) / 254, i = clampi(c8 >> 8, 0, 135);
    return CUT_HZ[i] + (uint32_t)(((int32_t)(CUT_HZ[i + 1] - CUT_HZ[i]) * (c8 & 255)) >> 8);
}
static int is_bip(int32_t id)
{
    if (id == V_TUNE || (id >= V_PAN1 && id < V_PAN1 + 4)) return 1;
    if (id < 0 || id >= NPARAM) return 0;
    return PDEF[id].kind == K_BTEN || PDEF[id].kind == K_PITCH || PDEF[id].kind == K_FINE;
}
static void mix_name(char *s, int32_t v, const char *a, const char *b)   /* a crossfade knob: "EG", "LFO", "LFO 60%" */
{
    int32_t pc = v * 100 / 127;
    if (pc <= 2) { scat(s, a); return; }
    if (pc >= 98) { scat(s, b); return; }
    if (pc == 50) { scat(s, "50/50"); return; }
    scat(s, pc < 50 ? a : b); scat(s, " "); fmt_u(s + slen(s), (uint32_t)(pc < 50 ? 100 - pc : pc)); scat(s, "%");
}
static void param_value(char *s, int32_t id, int32_t v)   /* <= 8 characters */
{
    s[0] = 0;
    if (id >= NPARAM || id < 0) {
        switch (id) {
        case V_MIDICH: if (v) fmt_u(s, (uint32_t)v); else scat(s, "OMNI"); return;
        case V_LIGHTS: if (v) fmt_u(s, (uint32_t)v); else scat(s, "OFF"); return;
        case V_TUNE: fmt_s(s, v - 50); scat(s, "c"); return;
        case V_TEMPO: if (ctl_ext_quarter) { scat(s, "EXT"); return; } fmt_u(s, (uint32_t)v + 40u); return;
        default:
            if (id >= V_PAN1 && id < V_PAN1 + 4) {
                if (v == 64) scat(s, "C"); else { scat(s, v < 64 ? "L" : "R"); fmt_u(s + 1, (uint32_t)(v < 64 ? 64 - v : v - 64)); }
                return;
            }
            fmt_u(s, (uint32_t)v);
            return;
        }
    }
    switch (PDEF[id].kind) {
    case K_TEN: fmt_frac(s, v * 100 / 127, 1, 0); break;   /* a classic dial, 0 .. 10 */
    case K_BTEN: fmt_frac(s, clampi((v - 64) * 50 / 63, -50, 50), 1, 1); break;   /* -5 .. +5 */
    case K_PCT: fmt_u(s, (uint32_t)v * 100u / 127u); scat(s, "%"); break;
    case K_ONOFF: scat(s, v ? "ON" : "OFF"); break;
    case K_CTYPE: scat(s, CTYPE_NAME[v % NCT]); break;
    case K_CHZ: fmt_lhz(s, CH_HZ_X100[v & 127]); break;
    case K_CHMS: fmt_frac(s, v * 250 / 127, 2, 0); scat(s, "ms"); break;
    case K_DDIV: scat(s, DIV_NAME[18u - (uint32_t)v % NDLYDIV]); break;
    case K_LHZ: fmt_hz(s, DLC_HZ[v & 127]); break;
    case K_HHZ: fmt_hz(s, DHC_HZ[v & 127]); break;
    case K_RMS: fmt_ms(s, (uint32_t)v * 930u / 127u); break;
    case K_CUT: fmt_hz(s, cut_hz(v)); break;
    case K_ATT: fmt_ms(s, ATT_MS_X10[v & 127]); break;
    case K_DR: fmt_ms(s, DR_MS_X10[v & 127]); break;
    case K_PITCH: if (v == 60) scat(s, "0"); else { fmt_s(s, v - 60); scat(s, " st"); } break;
    case K_FINE: fmt_s(s, v - 50); scat(s, "c"); break;
    case K_WAVE: mix_name(s, v, "SAW", "PULSE"); break;
    case K_PW: fmt_u(s, (uint32_t)(5 + v * 45 / 127)); scat(s, "%"); break;
    case K_MMIX: mix_name(s, v, "EG", "LFO"); break;
    case K_LWAVE: mix_name(s, v, "TRI", "SQUARE"); break;
    case K_LRATE: fmt_lhz(s, LFO_HZ_X100[v & 127]); break;
    case K_VCAMODE: scat(s, v ? "ON" : "EG"); break;
    case K_GLIDE: if (v) fmt_ms(s, (uint32_t)GLIDE_MS[v & 127] * 10u); else scat(s, "OFF"); break;
    case K_KBSCL: fmt_frac(s, 10 + v * 40 / 127, 1, 0); scat(s, " OCT"); break;
    case K_JACK: if (v) scat(s, JSRC_NAME[v % NJSRC]); else scat(s, JIN_NORMAL[(id - P_JACK) % NJIN]); break;
    case K_PRIO: scat(s, PRIO_NAME[v % NPR]); break;
    case K_RETRIG: scat(s, v ? "ALWAYS" : "LEGATO"); break;
    case K_BEND: fmt_u(s, (uint32_t)v); scat(s, " st"); break;
    case K_ARP: scat(s, ARP_NAME[v % NAR]); break;
    case K_AOCT: fmt_u(s, (uint32_t)v + 1u); break;
    case K_ADIV: scat(s, DIV_NAME[ARP_DIV0 + (uint32_t)v % NARPDIV]); break;
    case K_SYNC: scat(s, v ? DIV_NAME[((uint32_t)v - 1u) % NCLKDIV] : "FREE"); break;
    case K_CDIV: scat(s, DIV_NAME[(uint32_t)v % NCLKDIV]); break;
    default: fmt_u(s, (uint32_t)v); break;
    }
}

/* ---- MIDI CC: the fixed ones, the rest from CC 20 up in the table's order (and every parameter by NRPN 0 / index) */
static uint8_t cc_map[128];                     /* CC -> parameter + 1 */
static int cc_reserved(uint32_t cc) { return cc == 64u || (cc >= 71u && cc <= 75u) || (cc >= 91u && cc <= 93u) || cc == 98u || cc == 99u; }
static void cc_map_init(void)
{
    uint32_t p, cc = 20;
    for (p = 0; p < NPARAM; p++) if (PDEF[p].cc) cc_map[PDEF[p].cc] = (uint8_t)(p + 1u);
    for (p = 0; p < NPARAM && cc < 120u; p++) {
        if (PDEF[p].cc) continue;
        while (cc_reserved(cc)) cc++;
        if (cc >= 120u) break;
        cc_map[cc++] = (uint8_t)(p + 1u);
    }
}
static int32_t cc_scale(int32_t id, uint32_t v) { uint32_t m = pmax(id); return m == 127u ? (int32_t)v : (int32_t)((v * m + 63u) / 127u); }

/* ---- the sound on the track */
static void trk_load(uint32_t t, uint32_t n)    /* track t: program n */
{
    patch_t p;
    t %= NTRK;
    prog_get(n, &p);
    live_p[t] = p;
    set.tprog[t] = (uint8_t)(n % NPROG);
    edited[t] = 0;
    set_dirty = 1; set_t = fm1_ms;
    if (t == ctrk) dirty_hdr = dirty_page = dirty_map = dirty_graph = 1;
}
static void undo_keep(void) { undo_p = live; undo_trk = ctrk; undo_ok = 1; }

static uint32_t rnd_ui = 0xC0FFEE42u;
static uint32_t rn(uint32_t lo, uint32_t hi) { return lo + xorshift(&rnd_ui) % (hi - lo + 1u); }

static void name_as(const char *nm)
{
    uint32_t i;
    for (i = 0; i < PATCH_NAME; i++) live.v[NPARAM + i] = ' ';
    for (i = 0; i < PATCH_NAME && nm[i]; i++) live.v[NPARAM + i] = (uint8_t)nm[i];
}
/* a new sound, within reason: always something to hear; sometimes a cable or two of the classic patches */
static void randomize_patch(void)
{
    static const uint8_t CABLE[][2] = {{I_CUTOFF, J_KBCV}, {I_CUTOFF, J_SH}, {I_FOLD, J_VCO}, {I_PWM, J_LFO},
        {I_VCACV, J_LFO}, {I_PWM, J_EG}, {I_LFORATE, J_KBCV}, {I_FOLD, J_ONETWO}, {I_CUTOFF, J_LFO}, {I_1VOCT, J_ATTN}};
    uint8_t *v;
    uint32_t n, k;
    undo_keep();
    patch_defaults(&live);
    v = live.v;
    v[P_WAVE] = (uint8_t)(rn(0, 2) ? rn(0, 1) * 127u : rn(20, 110));
    v[P_PW] = (uint8_t)rn(20, 127);
    v[P_CUTOFF] = (uint8_t)rn(25, 95); v[P_RES] = (uint8_t)rn(0, 105);
    v[P_VCF_AMT] = (uint8_t)rn(70, 118); v[P_VCF_MIX] = (uint8_t)(rn(0, 3) ? 0 : rn(20, 127));
    v[P_ATTACK] = (uint8_t)(rn(0, 3) ? rn(0, 25) : rn(40, 85)); v[P_DECAY] = (uint8_t)rn(35, 95);
    v[P_SUSTAIN] = (uint8_t)rn(0, 127); v[P_RELEASE] = (uint8_t)rn(20, 85);
    v[P_LFO_RATE] = (uint8_t)rn(20, 90); v[P_LFO_WAVE] = (uint8_t)(rn(0, 1) * 127u);
    v[P_VCO_MIX] = (uint8_t)(rn(0, 1) ? 127 : rn(0, 127));
    v[P_PITCH_AMT] = (uint8_t)(rn(0, 4) == 0 ? rn(8, 30) : 0);
    v[P_PWM_AMT] = (uint8_t)(rn(0, 2) == 0 ? rn(20, 90) : 0);
    v[P_GLIDE] = (uint8_t)(rn(0, 3) == 0 ? rn(20, 60) : 0);
    v[P_FOLD] = (uint8_t)rn(0, 100);
    v[P_PITCH] = (uint8_t)(rn(0, 2) ? 60 : 48);
    n = rn(0, 3) ? rn(0, 2) : 0;
    for (k = 0; k < n; k++) { uint32_t c = rn(0, sizeof CABLE / 2u - 1u); v[PJ(CABLE[c][0])] = CABLE[c][1]; }
    if (v[PJ(I_FOLD)] == J_ONETWO) { v[PJ(I_ONE)] = J_LFO; v[PJ(I_TWO)] = J_VCO; v[P_ONE_LVL] = (uint8_t)rn(40, 110); v[P_LFO_RATE] = (uint8_t)rn(105, 125); }
    if (v[PJ(I_1VOCT)] == J_ATTN) { v[PJ(I_ATTN)] = J_SH; v[P_ATTN] = (uint8_t)rn(10, 40); }
    if (v[PJ(I_VCACV)] == J_LFO) v[P_LFO_RATE] = (uint8_t)rn(50, 85);
    v[P_VOLUME] = 110;
    name_as("RANDOM");
    mark_edited();
    dirty_hdr = dirty_page = dirty_map = dirty_graph = 1;
}
static void mutate_patch(void)                  /* a variation: the continuous knobs nudged a little, the cables kept */
{
    uint32_t i;
    undo_keep();
    for (i = 0; i < NPARAM; i++) {
        uint32_t k = PDEF[i].kind;
        if (k == K_TEN || k == K_BTEN || k == K_ATT || k == K_DR || k == K_CUT || k == K_WAVE || k == K_PW || k == K_MMIX
            || k == K_LWAVE || k == K_LRATE) {
            int32_t v = live.v[i], d = (int32_t)rn(0, 12) - 6;
            if (k == K_BTEN && v == 64) continue;   /* (a depth at 0 stays off) */
            if ((i == P_PITCH_AMT || i == P_PWM_AMT || i == P_FOLD || i == P_ONE_LVL || i == P_DRIFT) && v == 0) continue;
            if (i == P_VOLUME) continue;
            live.v[i] = (uint8_t)clampi(v + d, 0, (int32_t)PDEF[i].max);
        }
    }
    mark_edited();
    dirty_page = dirty_map = dirty_graph = 1;
}

/* ---- the keys: notes, or (SAVE held) functions on the white keys */
static const int8_t KEY_WHITE[27] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1, 13, 14, -1, 15};
static uint32_t layer_btn, layer_t, layer_used; /* a held button whose keys are functions: BTN_SAVE (else 0xFF) */
static uint32_t edit_t, edit_used;              /* EDIT: pressed when, used as a modifier (undo / redo) */
static uint32_t keys_prev;
static uint8_t key_note[27], key_fn[27], key_trk[27];

static int note_base(void) { return 53 + 12 * set.oct; }   /* F3 at octave 0 */
static void play_note(uint32_t t, uint32_t on, uint32_t note, uint32_t vel)   /* the keys: track t, out on its channel */
{
    note_event_t(t, on, note, vel);
    midi_send((on ? 0x90u : 0x80u) | (set.midi_ch ? (set.midi_ch - 1u + t) & 15u : 0u), note, on ? vel : 0u);
}
static void key_function(uint32_t k)
{
    int32_t w = KEY_WHITE[k];
    layer_used = 1;
    if (w < 0) return;
    if (layer_btn == BTN_SAVE) {
        switch (w) {
        case 0: undo_keep(); patch_defaults(&live); mark_edited(); dirty_hdr = dirty_page = dirty_map = dirty_graph = 1; say("INIT"); break;
        case 1: randomize_patch(); say("RANDOM SOUND|EDIT + OCT-: UNDO"); break;
        case 2: mutate_patch(); say("MUTATED: A VARIATION|EDIT + OCT-: UNDO"); break;
        default: break;
        }
    }
}
static void keys_in(uint32_t notes_only)
{
    uint32_t keys = fm1_in.notes, down = keys & ~keys_prev, up = keys_prev & ~keys, k;
    int base = note_base();
    for (k = 0; k < 27u; k++) {
        if (down >> k & 1u) {
            if (layer_btn != 0xFFu) {           /* a function, not a note */
                if (notes_only) continue;       /* (the main loop's: its bit stays new) */
                keys_prev |= 1u << k;
                key_fn[k] = 1;
                key_function(k);
                continue;
            }
            keys_prev |= 1u << k;
            key_fn[k] = 0;
            key_note[k] = (uint8_t)clampi(base + (int32_t)k, 0, 127);
            key_trk[k] = (uint8_t)ctrk;
            play_note(key_trk[k], 1, key_note[k], set.kvel);
        }
        if (up >> k & 1u) {
            if (key_fn[k] && notes_only) continue;
            keys_prev &= ~(1u << k);
            if (!key_fn[k]) play_note(key_trk[k], 0, key_note[k], 0);
            key_fn[k] = 0;
        }
    }
}

/* ---- MIDI in: USB and the TRS jack (one ring). A channel N set: N .. N + NTRK - 1 are the tracks (fixed, whatever track
 * the panel is on: OberVave's fixed channels); others are ignored. OMNI: everything plays the panel's track. */
static uint8_t midi_back[4];                    /* a program change left for the main loop (app_idle met it) */
static uint32_t clk_n, clk_last, clk_t0;
static uint8_t nrpn_msb[NTRK], nrpn_lsb[NTRK];  /* (127, 127 at the start: no parameter chosen) */
static uint8_t omni_trk[128];                   /* OMNI: the track each note began on (the panel may move meanwhile) */
static uint32_t diag_req;
/* CC 3 (any value) asks for the CPU, answered on channel 16: CC 20 / 21 the slowest half of the last 2 s in us (high
 * / low 7 bits), CC 22 / 23 the last half's, CC 24 the late halves since power-on (up to 127), CC 25 the tracks,
 * CC 26 the MIDI channel (0 OMNI). (tools/fm1_cpu.py reads it: the render time measured on the chip itself) */
static void diag_send(void)
{
    uint32_t pk = cpu_m.peak, now = ui_render_us, ov = cpu_m.over;
    midi_send(0xBFu, 20, pk >> 7 & 127u); midi_send(0xBFu, 21, pk & 127u);
    midi_send(0xBFu, 22, now >> 7 & 127u); midi_send(0xBFu, 23, now & 127u);
    midi_send(0xBFu, 24, ov > 127u ? 127u : ov); midi_send(0xBFu, 25, NTRK); midi_send(0xBFu, 26, set.midi_ch);
}
static int32_t knob_of(int32_t id)              /* the knob on the page showing parameter id, -1: none */
{
    uint32_t k;
    for (k = 0; k < 4u; k++) if (page_param(k) == id) return (int32_t)k;
    return -1;
}
static void cc_param(uint32_t t, int32_t id, uint32_t v)
{
    int32_t k, x = cc_scale(id, v);
    if (is_jack(id) && !jack_ok((uint32_t)(id - P_JACK), (uint32_t)x)) x = J_NONE;   /* (its own output: no cable) */
    pset_t(t, id, x);
    if (t % NTRK != ctrk) return;
    if ((k = knob_of(id)) >= 0) dirty_knob |= 1u << k;
}
static void midi_in(uint32_t notes_only)
{
    uint8_t st, d1, d2;
    for (;;) {
        uint32_t ch, kind, t;
        if (midi_back[3]) { st = midi_back[0]; d1 = midi_back[1]; d2 = midi_back[2]; midi_back[3] = 0; }
        else if (!midi_get(&st, &d1, &d2)) break;
        if (notes_only && (st & 0xF0u) == 0xC0u) {   /* a program change loads a sound: not in the middle of a draw */
            midi_back[0] = st; midi_back[1] = d1; midi_back[2] = d2; midi_back[3] = 1;
            break;
        }
        if (st == 0xF8u) {                      /* clock: 24 a quarter; the tempo from 4 quarters (OberVave's) */
            uint32_t tk = ctl_clk_ticks + 1u;   /* (0.3) the engine's place follows the clocks (0xFFFFFFFF + 1: Start's first) */
            ctl_clk_ticks = tk >= 2304u ? 0u : tk;
            if (!ctl_clk_run) { ctl_clk_run = 1; if (sec == S_LFO) dirty_page = 1; }
            if (!clk_n || fm1_ms - clk_last > 250u) { clk_n = 0; clk_t0 = eng_t; }
            clk_last = fm1_ms;
            if (++clk_n == 97u) {
                uint32_t q = ((eng_t - clk_t0) / 4u + 24u) / 48u * 48u, d;
                if (q > FS / 5u && q < FS * 8u / 5u) {
                    d = q > ctl_ext_quarter ? q - ctl_ext_quarter : ctl_ext_quarter - q;
                    if (!ctl_ext_quarter || d * 1000u > 3u * q) { ctl_ext_quarter = q; if (sec == S_GLO) dirty_page = 1; }
                }
                clk_n = 1; clk_t0 = eng_t;
            }
            continue;
        }
        if (st == 0xFAu) { ctl_clk_ticks = 0xFFFFFFFFu; ctl_clk_start = 1; ctl_clk_run = 1; clk_last = fm1_ms; ctl_arp_restart = 1; clk_n = 0; continue; }
        if (st == 0xFBu) { ctl_clk_run = 1; clk_last = fm1_ms; ctl_arp_restart = 1; clk_n = 0; continue; }   /* continue */
        if (st == 0xFCu) { ctl_clk_run = 0; if (sec == S_LFO) dirty_page = 1; continue; }   /* stop: the place runs on */
        if (st >= 0xF0u) continue;
        ch = (st & 15u) + 1u; kind = st & 0xF0u;
        if (set.midi_ch) {
            if (ch < set.midi_ch || ch >= set.midi_ch + NTRK) continue;
            t = ch - set.midi_ch;
        } else t = ctrk;
        switch (kind) {
        case 0x90u:
            if (d2) { if (!set.midi_ch) omni_trk[d1 & 127u] = (uint8_t)t; note_event_t(t, 1, d1, d2); break; }
            /* fall through */                  /* (velocity 0: a note off) */
        case 0x80u: note_event_t(set.midi_ch ? t : omni_trk[d1 & 127u], 0, d1, 0); break;
        case 0xE0u: ctl_bend[t] = (int32_t)(d1 | d2 << 7) - 8192; break;
        case 0xD0u: ctl_after[t] = d1; break;
        case 0xA0u: ctl_after[t] = d2; break;   /* (poly pressure: the voice is one) */
        case 0xC0u: if (d1 < NPROG) trk_load(t, d1); break;
        case 0xB0u:
            if (d1 == 1u) { ctl_wheel[t] = d2; }
            else if (d1 == 7u) ctl_cc7[t] = d2;
            else if (d1 == 64u) { if (d2 >= 64u) ctl_sustain |= 1u << t; else ctl_sustain &= ~(1u << t); }
            else if (d1 == 120u || d1 == 123u) ctl_panic = 1;
            else if (d1 == 3u) {                /* (a diagnosis: the CPU, answered on channel 16; 1-16 / 17 also set
                                                 * the MIDI channel / OMNI, for tools/fm1_cpu.py) */
                if (d2 >= 1u && d2 <= 17u) { set.midi_ch = (uint8_t)(d2 == 17u ? 0u : d2); set_dirty = 1; set_t = fm1_ms; if (sec == S_GLO) dirty_page = 1; }
                diag_req = 1;
            }
            else if (d1 == 99u) nrpn_msb[t] = d2;
            else if (d1 == 98u) nrpn_lsb[t] = d2;
            else if (d1 == 6u) { uint32_t p = nrpn_msb[t] * 128u + nrpn_lsb[t]; if (p < NPARAM) cc_param(t, (int32_t)p, d2); }
            else if (cc_map[d1]) cc_param(t, cc_map[d1] - 1, d2);
            break;
        default: break;
        }
    }
}

static uint32_t idle_ok;
static void app_idle(void)                      /* lcd.c while a strip goes out, ui_page between the knobs: the notes */
{
    static uint32_t busy;
    if (!idle_ok || busy) return;
    busy = 1;
    keys_in(1);
    midi_in(1);
    busy = 0;
}

/* ---- drawing */
#include "mfm_graph.h"
static uint16_t acc_colour(const item_t *it)
{
    static const uint16_t AC[] = {C_PERF, C_VCO, C_VCF, C_ENV, C_LFO, C_UTL, C_JACK, C_GLOB, C_FXC};
    return AC[it->acc % 9u];
}
static uint32_t cur_graph(void) { const item_t *it = cur_item(); return it->gr[cur_pg() % 5u]; }
static int on_patchbay(void) { return sec == S_PATCH && cur_pg() == 0u; }   /* (SEQ's first page: the whole patchbay) */
static const char *tab_text(const item_t *it)   /* the box's tab: the module, as the panel prints it */
{
    static const char *const TT[NTAB] = {"VCO", "VCF", "EG", "VCA", "LFO", "UTL", "PATCH", "FX"};
    if (it->tab >= 0) return TT[it->tab % NTAB];
    switch (sec) {
    case S_PERF: return sec_item[S_PERF] ? "MIX" : "PERF";
    case S_ARP: return "ARP";
    default: return sec_item[S_GLO] == 0u ? "KB" : sec_item[S_GLO] == 1u ? "VOICE" : "GLO";
    }
}

/* ---- the animations (0.2): the LFO LED, the tracks' lamps, the keyboard's buttons, a knob's light, the cables' swing.
 * A step every 33 ms, only what changed is drawn again, and nothing while the page or a knob waits to be drawn */
static uint16_t an_led, an_lamp[4], an_key[13], an_knob[4];
static uint16_t an_led_was = 0xFFFF, an_lamp_was[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, an_key_was[13];
static int16_t an_swing[NJIN], an_swv[NJIN];    /* the cables' sag offset (px) and its speed (Q4) */
static uint32_t an_t, an_keys_dirty = 1, an_patch_live, an_focus_t;
static int32_t an_focus = -1;                   /* the patchbay: the knob turned last (for a moment) */
static void cable_swing(uint32_t in) { if (in < NJIN) { an_swing[in] = -14; an_swv[in] = 0; an_patch_live = 1; } }

static void draw_top(void)
{
    an_led_was = an_led;
    memcpy(an_lamp_was, an_lamp, sizeof an_lamp);
    ui_top(an_led, an_lamp, ctrk);
}
static void draw_header(void)                   /* the top strip and the program */
{
    char tags[64], msg[32];
    uint32_t t = ctrk, n = ncables(live.v);
    tags[0] = 0;
    scat(tags, live.v[P_VCA_MODE] ? "VCA ON" : "VCA EG");
    scat(tags, "  ");
    if (n) { fmt_u(tags + slen(tags), n); scat(tags, n > 1u ? " CABLES" : " CABLE"); }
    else scat(tags, "NO CABLES");
    if (set.fx_off) scat(tags, "  FX OFF");
    else if (live.v[P_CH_ON] || live.v[P_DL_ON] || live.v[P_RV_ON]) {
        scat(tags, " ");
        if (live.v[P_CH_ON]) scat(tags, " CHO");
        if (live.v[P_DL_ON]) scat(tags, " DLY");
        if (live.v[P_RV_ON]) scat(tags, " REV");
    }
    if (live.v[P_ARP_MODE]) scat(tags, "  ARP");
    if (ctl_hold >> t & 1u) scat(tags, "  HOLD");
    if (set.oct && slen(tags) < 34u) { scat(tags, "  OCT"); fmt_s(tags + slen(tags), set.oct); }
    draw_top();
    if (save_armed) {
        msg[0] = 0; scat(msg, "SAVE TO ");
        fmt_u(msg + slen(msg), save_to % NPROG + 1u); scat(msg, "?");
        ui_prog(save_to % NPROG, live.v + NPARAM, edited[t], tags, msg, t);
    } else
        ui_prog(set.tprog[t], live.v + NPARAM, edited[t], tags, 0, t);
}
static void knob_view(knob_view_t *kv, uint32_t k)
{
    int32_t id = page_param(k);
    kv->present = id >= 0;
    if (id < 0) return;
    kv->label = plabel(id);
    kv->v = pget(id);
    kv->max = (int32_t)pmax(id);
    kv->bip = is_bip(id);
    kv->style = KS_KNOB; kv->icl = kv->icr = IC_NONE; kv->cable = 0; kv->top = kv->bot = 0;
    param_value(kv->value, id, kv->v);
    if (is_jack(id)) { kv->style = KS_JACK; kv->cable = kv->v ? CABLE_C[kv->v % NJSRC] : 0; return; }
    if (id == P_LFO_RATE && live.v[P_LFO_SYNC]) { kv->value[0] = 0; scat(kv->value, "SYNC "); scat(kv->value, DIV_NAME[(live.v[P_LFO_SYNC] - 1u) % NCLKDIV]); }
    switch (id) {                               /* the panel's big knobs, its toggles, the signs at the scales' ends */
    case P_PITCH: case P_LFO_RATE: case P_ATTACK: case P_DECAY: case P_SUSTAIN: case P_RELEASE: case P_VOLUME:
        kv->style = KS_BIG;
        if (id == P_ATTACK) { kv->icl = IC_ATT; } else if (id == P_DECAY) { kv->icl = IC_DEC; }
        else if (id == P_SUSTAIN) { kv->icl = IC_SUS; } else if (id == P_RELEASE) { kv->icl = IC_REL; }
        break;
    case P_WAVE: kv->icl = IC_SAW; kv->icr = IC_PULSE; break;
    case P_PW: kv->icl = IC_NARROW; kv->icr = IC_SQUARE; break;
    case P_LFO_WAVE: kv->icl = IC_TRI; kv->icr = IC_SQUARE; break;
    case P_FOLD: kv->icl = IC_FOLD0; kv->icr = IC_FOLD3; break;
    case P_VCO_MIX: case P_VCF_MIX: kv->icl = IC_EG; kv->icr = IC_LFO; break;
    case P_VCF_AMT: kv->icl = IC_MINUS; kv->icr = IC_PLUS; break;
    case P_VCA_MODE: kv->style = KS_TOGGLE; kv->top = "ON"; kv->bot = "EG"; break;
    default:
        if (id >= 0 && id < NPARAM && PDEF[id].kind == K_ONOFF) { kv->style = KS_TOGGLE; kv->top = "ON"; kv->bot = "OFF"; }
        break;
    }
}
static void page_map(pmap_t *pm)
{
    uint32_t k;
    pm->n = (uint8_t)(SEC[sec].n < 8u ? SEC[sec].n : 8u);   /* the section's map: every item's pages (SELECT runs through them) */
    for (k = 0; k < pm->n; k++) pm->npg[k] = item_of(sec, k)->npg;
    pm->item = (uint8_t)(sec_item[sec] % SEC[sec].n); pm->pg = (uint8_t)cur_pg();
}
static void draw_patchbay(void)
{
    const item_t *it = cur_item();
    pmap_t pm;
    page_map(&pm);
    ui_patchbay(live.v, an_swing, it->sub[cur_pg() % 5u], &pm);
}
static void draw_page(void)
{
    const item_t *it = cur_item();
    knob_view_t kv[4];
    pmap_t pm;
    char note[16];
    const char *sub = it->sub[cur_pg() % 5u];
    uint32_t k;
    if (on_patchbay()) { draw_patchbay(); return; }
    note[0] = 0;
    if (sec == S_GLO && sec_item[S_GLO] == 2u) {   /* GLOBAL: the CPU (the slowest half of the last 2 s, of 5805 us) */
        if (cpu_m.over) { scat(note, "LATE "); fmt_u(note + 5, cpu_m.over); }
        else { fmt_u(note, cpu_m.peak); scat(note, " us"); }
    }
    if (sec == S_ENV && sec_item[S_ENV] % 2u == 1u) sub = live.v[P_VCA_MODE] ? "MODE ON" : "MODE EG";
    if (sec == S_FX) {                          /* the effect's ON, and REC's */
        static const uint8_t ON[3] = {P_CH_ON, P_DL_ON, P_RV_ON};
        sub = set.fx_off ? "OFF (REC)" : live.v[ON[sec_item[S_FX] % 3u]] ? "ON" : "OFF";
    }
    if (sec == S_LFO && sec_item[S_LFO] % 3u == 2u) sub = ctl_ext_quarter ? (ctl_clk_run ? "MIDI CLOCK" : "MIDI (STOPPED)") : "TEMPO";
    if (sec == S_ARP) sub = ARP_NAME[live.v[P_ARP_MODE] % NAR];
    for (k = 0; k < 4u; k++) knob_view(&kv[k], k);
    page_map(&pm);
    ui_page(tab_text(it), it->title, sub, &pm, note[0] ? note : 0, kv, acc_colour(it));
    memset(an_knob, 0, sizeof an_knob);
    an_keys_dirty = 1;
}
static void draw_knobs(uint32_t mask)           /* only the knobs that moved (lit while turned, the light fading) */
{
    knob_view_t kv;
    uint32_t k;
    if (on_patchbay()) { draw_patchbay(); return; }
    for (k = 0; k < 4u; k++)
        if (mask >> k & 1u) { knob_view(&kv, k); ui_knob(k, &kv, acc_colour(cur_item()), an_knob[k]); }
}
static void draw_graph(void)                    /* the page's picture, from the sound as it is */
{
    const item_t *it = cur_item();
    uint16_t c = acc_colour(it);
    uint32_t g = cur_graph();
    if (on_patchbay()) return;                  /* (the patchbay takes the screen too) */
    switch (g) {
    case G_VCO: g_vco(live.v, c); break;
    case G_VMOD: g_vmod(live.v, ctrk, c); break;
    case G_VCF: g_filter(live.v, ctrk, c); break;
    case G_EG: g_env(live.v, ctrk, c); break;
    case G_VCA: g_vca(live.v, ctrk, c); break;
    case G_LFO: g_lfo(live.v, ctrk, c); break;
    case G_SH: g_sh(live.v, ctrk, c); break;
    case G_FOLD: g_fold(live.v, c); break;
    case G_UMIX: g_umix(live.v, ctrk, c); break;
    case G_ATTN: g_attn(live.v, ctrk, c); break;
    case G_KB: g_kb(live.v, c); break;
    case G_ARP: g_arp(live.v, c); break;
    case G_CHO: g_cho(live.v, c); break;
    case G_DLY: g_dly(live.v, c); break;
    case G_REV: g_rev(live.v, c); break;
    case G_TRACKS: g_tracks(ctrk); break;
    case G_CLK: g_clk(live.v, c); break;
    case G_PATCH: {                             /* an INPUTS page: its four inputs and their cables */
        int16_t jp[4];
        uint32_t k;
        for (k = 0; k < 4u; k++) { int32_t id = page_param(k); jp[k] = (int16_t)(is_jack(id) ? id - P_JACK : -1); }
        g_jacks(live.v, jp, fm1_ms - an_focus_t < 1500u ? an_focus : -1);
        break;
    }
    default: break;                             /* (the scope draws itself as it fills) */
    }
}
static void note_name(char *s, uint32_t n)      /* MIDI 60 -> "C4" */
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    s[0] = 0; scat(s, NN[n % 12u]); fmt_s(s + slen(s), (int32_t)(n / 12u) - 1);
    if (s[slen(s) - 2u] == '+') { s[slen(s) - 2u] = s[slen(s) - 1u]; s[slen(s) - 1u] = 0; }
}
static void draw_map(void)                      /* the foot: the keyboard and the page's hint, or a message */
{
    char nt[8];
    if (toast && fm1_ms - toast_t < 1800u) { ui_toast(toast); return; }
    if (toast) { toast = 0; if (on_patchbay()) { dirty_page = 1; return; } }
    if (on_patchbay()) return;
    memcpy(an_key_was, an_key, sizeof an_key);
    an_keys_dirty = 0;
    nt[0] = 0;
    if (ui_note[ctrk] != 255u) note_name(nt, ui_note[ctrk]);
    ui_keys(an_key, cur_item()->hint, nt[0] ? nt : 0);
}
/* one step of the animations: the levels move, what changed is drawn again (cheap strips only) */
static void animate(void)
{
    uint32_t t, k, lfo_hi = trk[ctrk].lfo.ph < 0x80000000u, n = ui_note[ctrk], changed = 0;
    an_t = fm1_ms;
    an_led = (uint16_t)(lfo_hi ? 256u : an_led > 70u ? an_led - 70u : 0u);   /* (lit with the LFO, a short afterglow) */
    if (ui_lfo_inc[ctrk] > LFO_STEADY_SCR) an_led = 200u;   /* (0.3: faster than the screen shows: lit steady) */
    for (t = 0; t < NTRK; t++) {
        uint32_t p = ui_trk_peak[t] >> 6, l = ui_note[t] != 255u ? 140u + (p > 116u ? 116u : p) : 0u;
        an_lamp[t] = (uint16_t)(l > an_lamp[t] ? l : an_lamp[t] > 24u ? an_lamp[t] - 24u : 0u);
    }
    for (k = 0; k < 13u; k++) {                 /* the keyboard: the note's button lit, the others fading */
        uint32_t on = n != 255u && (k == n % 12u || (k == 12u && n % 12u == 0u && set.oct > 0));
        an_key[k] = (uint16_t)(on ? 256u : an_key[k] > 40u ? an_key[k] - 40u : 0u);
        changed |= an_key[k] != an_key_was[k];
    }
    if (an_led != an_led_was || memcmp(an_lamp, an_lamp_was, sizeof an_lamp)) draw_top();
    if ((changed || an_keys_dirty) && !toast && !on_patchbay()) draw_map();
    for (k = 0; k < 4u; k++)                    /* a knob's light fades after the turn */
        if (an_knob[k]) { an_knob[k] = (uint16_t)(an_knob[k] > 64u ? an_knob[k] - 64u : 0u); if (!on_patchbay()) dirty_knob |= 1u << k; }
    if (an_patch_live) {                        /* the cables: a damped swing toward their rest */
        uint32_t live_n = 0;
        for (k = 0; k < NJIN; k++) {
            an_swv[k] = (int16_t)(an_swv[k] - an_swing[k] * 5 - an_swv[k] / 4);
            an_swing[k] = (int16_t)(an_swing[k] + an_swv[k] / 16);
            if (an_swing[k] > -1 && an_swing[k] < 1 && an_swv[k] > -16 && an_swv[k] < 16) an_swing[k] = an_swv[k] = 0;
            else live_n++;
        }
        an_patch_live = live_n != 0;
        if (on_patchbay()) dirty_knob |= 1u;
    } else if (cur_graph() == G_PATCH && !on_patchbay() && fm1_ms - an_focus_t < 1600u)
        dirty_graph = 1;                        /* (the input being turned lit, a moment) */
}

/* ---- buttons */
/* The panel (the user's rule): a section's button opens it, SELECT turned runs through ALL its pages
 * (every item's, one after the other), the four knobs are the page's; SELECT pressed is the UTL section's button (0.3).
 * Nothing else is needed to edit (the button pressed again still jumps to the next item, a shortcut no one has to
 * know). */
static void go_section(uint32_t s)              /* a section's button: there, or (already there) its next item */
{
    if (sec == s) sec_item[s] = (sec_item[s] + 1u) % SEC[s].n;
    sec = s;
    dirty_page = dirty_map = dirty_hdr = 1;
}
static void select_turn(int32_t e)              /* SELECT turned: the next / previous page of the section, across its items */
{
    uint32_t n = SEC[sec].n, i = sec_item[sec] % n, pg = cur_pg(), was = i;
    for (; e > 0; e--) if (++pg >= item_of(sec, i)->npg) { pg = 0; i = (i + 1u) % n; }
    for (; e < 0; e++) {
        if (pg) pg--;
        else { i = (i + n - 1u) % n; pg = item_of(sec, i)->npg - 1u; }
    }
    sec_item[sec] = i; item_pg[sec][i % 8u] = pg;
    dirty_page = dirty_map = 1;
    if (i != was) dirty_hdr = 1;                /* (the module's tab and its colour) */
}
static uint8_t arp_last = AR_UP;
static void arp_toggle(void)                    /* the arp on / off (it keeps its mode) */
{
    if (live.v[P_ARP_MODE]) { arp_last = live.v[P_ARP_MODE]; pset(P_ARP_MODE, AR_OFF); say("ARP OFF"); }
    else { pset(P_ARP_MODE, arp_last); say("ARP ON"); }
    dirty_page = dirty_hdr = dirty_map = 1;
}
static void buttons(void)
{
    uint32_t rel, b = fm1_input_edges(&rel);
    if (!b && !rel) return;
    if (save_armed && (b & ~(1u << BTN_SAVE))) { save_armed = 0; dirty_hdr = 1; say("SAVE CANCELLED"); }
    /* tap / hold: EDIT (held: OCT- / OCT+ undo / redo), SAVE (held: white keys = INIT, RANDOM, MUTATE) */
    if (b >> BTN_EDIT & 1u) { edit_t = fm1_ms; edit_used = 0; }   /* (EDIT held keeps the keys as notes) */
    if (rel >> BTN_EDIT & 1u && !edit_used && fm1_ms - edit_t < 600u) go_section(S_EDIT);
    if (b >> BTN_SAVE & 1u) { layer_btn = BTN_SAVE; layer_t = fm1_ms; layer_used = 0; say("F3 INIT  G3 RANDOM|A3 MUTATE (A VARIATION)"); }
    if (rel >> BTN_SAVE & 1u && layer_btn == BTN_SAVE) {
        int tap = !layer_used && fm1_ms - layer_t < 600u;
        layer_btn = 0xFFu;
        toast = 0;
        if (tap && save_armed) {                /* SAVE twice: saved */
            uint32_t u;
            prog_put(save_to % NPROG, &live);
            set.tprog[ctrk] = (uint8_t)(save_to % NPROG); edited[ctrk] = 0; save_armed = 0;
            for (u = 0; u < NTRK; u++)          /* (another track on that program, unchanged: it hears the new one) */
                if (u != ctrk && set.tprog[u] == save_to % NPROG && !edited[u]) live_p[u] = live;
            set_dirty = 1; set_t = fm1_ms;
            say("SAVED|(WRITTEN WHEN SILENT)");
        } else if (tap) { save_armed = 1; save_t = fm1_ms; save_to = set.tprog[ctrk]; say("PRESETS: THE SLOT|SAVE AGAIN: SAVE"); }
        dirty_hdr = dirty_map = 1;
    }
    if (b & (3u << BTN_OCT_DOWN)) {
        if (fm1_in.buttons >> BTN_EDIT & 1u) {  /* EDIT + OCT- / OCT+: undo / redo (one level, a swap) */
            edit_used = 1;
            if (undo_ok) {                      /* (on the track it was done on) */
                patch_t t = live_p[undo_trk % NTRK];
                live_p[undo_trk % NTRK] = undo_p; undo_p = t; mark_edited_t(undo_trk);
                dirty_hdr = dirty_page = dirty_map = dirty_graph = 1; say(b & (1u << BTN_OCT_DOWN) ? "UNDO" : "REDO");
            }
            else say("NOTHING TO UNDO");
        } else if ((fm1_in.buttons & (3u << BTN_OCT_DOWN)) == (3u << BTN_OCT_DOWN)) {
            set.oct = 0; set_dirty = 1; set_t = fm1_ms; say("OCTAVE 0"); dirty_hdr = 1;
        } else {
            if (b & (1u << BTN_OCT_DOWN) && set.oct > -3) set.oct--;
            if (b & (1u << BTN_OCT_UP) && set.oct < 3) set.oct++;
            set_dirty = 1; set_t = fm1_ms; dirty_hdr = 1;
        }
    }
    if (b & (1u << BTN_HOME)) go_section(S_PERF);
    if (b & (1u << BTN_ENV)) go_section(S_ENV);
    if (b & (1u << BTN_LFO)) go_section(S_LFO);
    if (b & (1u << BTN_FX)) go_section(S_FX);
    if (b & (1u << BTN_SEQ)) go_section(S_PATCH);
    if (b & (1u << BTN_GLO)) go_section(S_GLO);
    if (b & (1u << BTN_ARP)) {                  /* ARP: its page (on / off: SELECT pressed; ARP again too, a shortcut) */
        if (sec == S_ARP) arp_toggle();
        sec = S_ARP;
        dirty_page = dirty_hdr = dirty_map = 1;
    }
    if (b & (1u << BTN_SEL)) go_section(S_UTL);  /* (0.3: SELECT pressed: UTL; turned: the pages) */
    if (b & (1u << BTN_PLAY)) {                 /* PLAY: HOLD (the notes stay until the next chord), the panel's track */
        ctl_hold ^= 1u << ctrk;
        say(ctl_hold >> ctrk & 1u ? "HOLD ON" : "HOLD OFF");
        dirty_hdr = 1;
    }
    if (b & (1u << BTN_REC)) {                  /* REC (0.3, the user): every effect off / on again, as they were */
        set.fx_off ^= 1u; ctl_fx_off = set.fx_off; set_dirty = 1; set_t = fm1_ms;
        say(set.fx_off ? "EFFECTS OFF|REC: ON AGAIN" : "EFFECTS ON|AS THEY WERE");
        dirty_hdr = 1;
        if (sec == S_FX) dirty_page = dirty_graph = 1;
    }
}

/* ---- the encoders */
/* the knobs' acceleration: from the turning speed (detents a second, smoothed): slow = one step a detent (fine work),
 * fast = up to 8, so 0..127 is about a turn and a half of a quick spin. Small ranges (switches, jacks) stay one a
 * detent. */
static uint32_t enc_t[7], knob_t;              /* knob_t: a knob turned last then (the screen waits for it) */
static int32_t enc_speed[7];                    /* detents a second x 16 */
static int8_t enc_dir[7];
static int32_t enc_step(uint32_t e, int32_t d, uint32_t max)
{
    uint32_t dt = fm1_ms - enc_t[e], ad = (uint32_t)(d < 0 ? -d : d), mult;
    int8_t dir = d < 0 ? -1 : 1;
    int32_t sp;
    enc_t[e] = fm1_ms;
    if (dt > 200u || dir != enc_dir[e]) enc_speed[e] = 0;   /* (a pause or a turn back: fine again) */
    enc_dir[e] = dir;
    if (dt < 4u) dt = 4u;
    enc_speed[e] = (enc_speed[e] + (int32_t)(ad * 16000u / dt)) / 2;
    if (max < 24u) return d;
    sp = enc_speed[e] >> 4;
    mult = sp < 7 ? 1u : sp < 13 ? 2u : sp < 22 ? 4u : sp < 35 ? 6u : 8u;
    if (max < 64u && mult > 2u) mult = 2u;
    return d * (int32_t)mult;
}
static void trk_select(uint32_t t)              /* ALGORITHM (several tracks): the track the panel plays and edits */
{
    uint32_t i;
    t %= NTRK;
    if (t == ctrk) return;
    set.trk = (uint8_t)t; set_dirty = 1; set_t = fm1_ms;
    save_armed = 0;
    toast_buf[0] = 0; scat(toast_buf, "TRACK "); fmt_u(toast_buf + slen(toast_buf), t + 1u); scat(toast_buf, ": ");
    for (i = 0; i < PATCH_NAME; i++) { toast_buf[slen(toast_buf) + 1u] = 0; toast_buf[slen(toast_buf)] = (char)live.v[NPARAM + i]; }
    while (slen(toast_buf) && toast_buf[slen(toast_buf) - 1u] == ' ') toast_buf[slen(toast_buf) - 1u] = 0;
    say(toast_buf);
    dirty_hdr = dirty_page = dirty_map = dirty_graph = 1;
}
static void encoders(void)
{
    int32_t e;
    uint32_t k;
    if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {
        if (save_armed) { save_to = (save_to + NPROG * 4u + (uint32_t)e) % NPROG; save_t = fm1_ms; dirty_hdr = 1; }
        else trk_load(ctrk, (set.tprog[ctrk] + NPROG * 4u + (uint32_t)(enc_step(6, e, NPROG) % (int32_t)NPROG + (int32_t)NPROG)) % NPROG);
    }
    if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
        if (NTRK > 1u)                          /* ALGORITHM: the track, one a detent */
            trk_select((uint32_t)clampi((int32_t)ctrk + (e > 0 ? 1 : -1), 0, (int32_t)NTRK - 1));
        else {                                  /* (one track: the filter's CUTOFF, always at hand) */
            int32_t kk;
            pset(P_CUTOFF, pget(P_CUTOFF) + enc_step(1, e, 127));
            if ((kk = knob_of(P_CUTOFF)) >= 0) dirty_knob |= 1u << kk;
            if (!toast || fm1_ms - toast_t > 300u) {
                toast_buf[0] = 0; scat(toast_buf, "CUTOFF ");
                param_value(toast_buf + slen(toast_buf), P_CUTOFF, pget(P_CUTOFF));
                scat(toast_buf, "Hz");
                say(toast_buf);
            }
        }
    }
    if ((e = fm1_enc_take(ENC_SELECT)) != 0) select_turn(e);   /* SELECT: every page of the section */
    for (k = 0; k < 4u; k++)
        if ((e = fm1_enc_take(ENC_KNOB1 + k)) != 0) {
            int32_t id = page_param(k);
            if (id < 0) continue;
            if (is_jack(id)) {                  /* (never its own module's output); the cable swings in */
                int32_t was = pget(id);
                pset(id, jack_step(id, was, e));
                if (pget(id) != was) cable_swing((uint32_t)(id - P_JACK));
            } else pset(id, pget(id) + enc_step(2u + k, e, pmax(id)));
            dirty_knob |= 1u << k;
            an_knob[k] = 256; an_focus = (int32_t)k; an_focus_t = knob_t = fm1_ms;   /* (lit while turned) */
        }
}

/* ---- the LEDs: the section's button, HOLD, ARP; the LFO button blinks with the LFO, as the reference panel's LFO RATE LED does
 * (lit steady while its page is open); the sounding note on the keys */
static void leds(void)
{
    static uint32_t was = 0xFFFFFFFFu, kwas = 0xFFFFFFFFu, lfo_t, lfo_led;
    uint32_t on = 0, keys = 0, i;
    int32_t n = ui_note[ctrk] == 255u ? -1 : (int32_t)ui_note[ctrk] - note_base();
    on |= 1u << SEC[sec].btn;
    if (fm1_ms - lfo_t >= 12u) { lfo_t = fm1_ms; lfo_led = trk[ctrk].lfo.ph < 0x80000000u; }   /* (looked at ~80 times a
                                                 * second: an audio-rate LFO flickers, the LED driver is not flooded) */
    if (ui_lfo_inc[ctrk] > LFO_STEADY_LED) lfo_led = 1;   /* (0.3: over ~20 Hz lit steady: no false slow blink) */
    if (sec != S_LFO && lfo_led) on |= 1u << BTN_LFO;
    if (!set.fx_off) on |= 1u << BTN_REC;       /* (0.3) REC lit: the effects on */
    if (set.oct) {                              /* (0.3) the octave: +-1 lit, +-2 blinking slow, +-3 fast */
        uint32_t a = (uint32_t)(set.oct < 0 ? -set.oct : set.oct);
        if (a == 1u || (a == 2u && fm1_ms / 400u % 2u == 0u) || (a >= 3u && fm1_ms / 120u % 2u == 0u))
            on |= 1u << (set.oct < 0 ? BTN_OCT_DOWN : BTN_OCT_UP);
    }
    if (ctl_hold >> ctrk & 1u) on |= 1u << BTN_PLAY;
    if (live.v[P_ARP_MODE]) on |= 1u << BTN_ARP;
    if (save_armed && (fm1_ms / 250u & 1u)) on |= 1u << BTN_SAVE;
    if (n >= 0 && n < 27) keys = 1u << n;
    if (on != was) {
        for (i = 0; i < 14u; i++) if ((on ^ was) >> i & 1u) fm1_led_key(i, (int)(on >> i & 1u));
        was = on;
    }
    if (keys != kwas) {
        for (i = 0; i < 27u; i++) if ((keys ^ kwas) >> i & 1u) fm1_led_key(KEY_LED(i), (int)(keys >> i & 1u));
        kwas = keys;
    }
}

/* ---- saves: the settings 2 s after the last change, the programs at once, both only in silence (an erase stops the
 * audio ~50 ms) */
static void saves(void)
{
    uint32_t i;
    if (!(set_dirty && fm1_ms - set_t > 2000u) && !psect_dirty) return;
    if (!engine_quiet()) return;
    for (i = 0; i < NPSECT; i++)
        if (psect_dirty >> i & 1u) {
            psect[i].tag = PS_TAG;
            psect_save(i, &psect[i], sizeof psect[i]);
            psect_dirty &= ~(1u << i);
            return;                             /* (one sector a pass) */
        }
    if (set_dirty && fm1_ms - set_t > 2000u) { settings_save(&set, sizeof set); set_dirty = 0; }
}

static void app_main(void)
{
    uint32_t t_scope = 0, t_page = 0, i;
#ifdef OBV_HOST
    atexit(host_blocks_report);
#endif
    engine_init();
    cc_map_init();
    for (i = 0; i < NPSECT; i++)
        if (psect_load(i, &psect[i], sizeof psect[i])
            || (psect[i].tag != PS_TAG && psect[i].tag != PS_TAG_02 && psect[i].tag != PS_TAG_01)) {
            memset(&psect[i], 0, sizeof psect[i]); psect[i].tag = PS_TAG;
        } else if (psect[i].tag != PS_TAG) {    /* (0.1's, 0.2's: converted here, written so with the sector's next save) */
            uint32_t j;
            for (j = 0; j < PS_N; j++)
                if (psect[i].used[j >> 5] >> (j & 31u) & 1u) {
                    if (psect[i].tag == PS_TAG_01) patch_from_01(&psect[i].p[j]);
                    else patch_from_02(&psect[i].p[j]);
                }
            psect[i].tag = PS_TAG;
        }
    if ((settings_load(&set, sizeof set) && settings_load(&set, SET_SIZE_02)) || set.magic != SET_MAGIC
        || set.version != SET_VERSION) {
        memset(&set, 0, sizeof set);
        set.magic = SET_MAGIC; set.version = SET_VERSION; set.tempo = 120; set.kvel = 100;
        for (i = 0; i < NTRK; i++) { set.tprog[i] = TPROG_DEF[i & 3u]; set.lvl[i] = NTRK > 1u ? 80 : 100; }
    }
    if (set.trk >= NTRK) set.trk = 0;
    for (i = 0; i < NTRK; i++) nrpn_msb[i] = nrpn_lsb[i] = 127;
    for (i = 0; i < NTRK; i++) {
        if (set.tprog[i] >= NPROG) set.tprog[i] = 0;
        if (set.lvl[i] > 127u) set.lvl[i] = 100;
        if (set.pan[i] < -64 || set.pan[i] > 64) set.pan[i] = 0;
        ctl_lvl[i] = set.lvl[i]; ctl_pan[i] = set.pan[i];
    }
    if (set.tempo < 40u || set.tempo > 240u) set.tempo = 120;
    if (set.oct < -3 || set.oct > 3) set.oct = 0;
    if (set.midi_ch > 16u) set.midi_ch = 0;
    if (set.tune < -50 || set.tune > 50) set.tune = 0;
    if (!set.kvel || set.kvel > 127u) set.kvel = 100;
    if (set.lights > 11u) set.lights = 0;
    set.fx_off = set.fx_off ? 1u : 0u;
    ctl_fx_off = set.fx_off;
    lights_apply();                             /* (every key a little lit from the start) */
    ctl_tempo = set.tempo;
    ctl_tune8 = set.tune * 256 / 100;
    for (i = 0; i < NTRK; i++) trk_load(i, set.tprog[i]);
    set_dirty = 0;
    layer_btn = 0xFFu;
    ui_splash();
    scope_state = 1;
    idle_ok = 1;
    for (;;) {
        platform_poll();
        keys_in(0);
        midi_in(0);
        buttons();
        encoders();
        if (save_armed && fm1_ms - save_t > 8000u) { save_armed = 0; dirty_hdr = 1; say("SAVE CANCELLED"); }
        if (diag_req) { diag_req = 0; diag_send(); }
        if (ctl_clk_run && fm1_ms - clk_last > 300u) { ctl_clk_run = 0; if (sec == S_LFO) dirty_page = 1; }   /* (no clock) */
        saves();
        leds();
        if (dirty_hdr) { dirty_hdr = 0; draw_header(); }
        if (dirty_page && fm1_ms - t_page >= 30u) {
            dirty_page = 0; dirty_knob = 0; t_page = fm1_ms; draw_page(); dirty_graph = 1;
        }
        else if (dirty_knob && !dirty_page && fm1_ms - t_page >= 12u) { uint32_t m = dirty_knob; dirty_knob = 0; t_page = fm1_ms; draw_knobs(m); }
        if (dirty_map || (toast && fm1_ms - toast_t >= 1800u)) { dirty_map = 0; draw_map(); }
        {                                       /* the graph: after a change, and 20 times a second while it moves */
            uint32_t g = cur_graph();
            int playing = ui_note[ctrk] != 255u, moving = g == G_LFO || g == G_SH || g == G_TRACKS || g == G_VMOD || g == G_CLK
                || (g_live(g) && g != G_SCOPE && (playing || graph_played));
            uint32_t gap = fm1_ms - knob_t < 150u ? 70u : dirty_graph ? 30u : 50u;   /* (a knob turning comes first) */
            if (g != G_SCOPE && (dirty_graph || moving) && fm1_ms - t_graph >= gap) {
                dirty_graph = 0; t_graph = fm1_ms; graph_played = playing;
                draw_graph();
            }
            if (g == G_SCOPE && dirty_graph) { dirty_graph = 0; g_scope(scope, 0, acc_colour(cur_item())); }
        }
        if (!dirty_page && !dirty_knob && fm1_ms - an_t >= 33u && fm1_ms - knob_t >= 60u) animate();
        if (scope_state == 3u && fm1_ms - t_scope >= 40u) {   /* ~25 frames a second (on the scope's pages) */
            uint32_t note = ui_note[ctrk] == 255u ? 60u : ui_note[ctrk], hz = CUT_HZ[note < 136u ? note : 135u];
            if (cur_graph() == G_SCOPE) g_scope(scope, SCOPE_N, acc_colour(cur_item()));
            scope_stride = (uint32_t)clampi((int32_t)(FS * 3u / ((hz ? hz : 1u) * SCOPE_N)), 1, 64);
            t_scope = fm1_ms;
            scope_state = 1;
        }
    }
}
