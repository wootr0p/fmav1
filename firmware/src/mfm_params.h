/* SPDX-License-Identifier: GPL-3.0-only */
/* The parameter table: a patch is a byte per parameter plus a 12-character name. The UI, MIDI CC / NRPN, the
 * randomizer and the storage all walk this table. The layout is the reference instrument's front panel, control for control
 * (its user's manual, "Panel Controls & Functions"):
 *   VCO   PITCH, VCO WAVE (saw > pulse), PULSE WIDTH; VCO MOD MIX (EG > LFO), PITCH MOD AMT, PWM AMT
 *   VCF   CUTOFF, RESONANCE; VCF MOD MIX (EG > LFO), VCF MOD AMT (the only bipolar knob)
 *   LFO   LFO RATE (0.1 .. 550 Hz), LFO WAVE (triangle > square)
 *   EG    ATTACK, DECAY, SUSTAIN, RELEASE
 *   VCA   VOLUME, the VCA MODE switch (EG / ON)
 *   KB    KB SCALE (1 .. 5 octaves an octave), GLIDE
 *   UTL   FOLD, ONE LVL, ATTENUATOR
 * and its patchbay: one parameter per input jack (13), each holding the output (11) its cable comes from, 0 = no cable
 * (the input's normal: the VCO into S+H, the LFO into S+H GATE, -5 V into ONE, +5 V into ATTN ...).
 * Not on the reference panel (marked so in the manual): FINE (the PITCH knob is continuous; here PITCH steps in semitones for MIDI),
 * three MIDI sources a cable can take (VELO, MODWHL, PRESS: a MIDI-to-CV in the case), the keyboard's PRIORITY and
 * RETRIG (the classic: low note, legato), BEND, ANALOG (the drift), the arpeggiator.
 * The order is the MIDI CC order (CC 20 up, see cc_map_init): the most played first. Append new parameters at the end
 * (the CC / NRPN numbers and the saved programs keep their meaning). */

/* the patchbay: the outputs a cable can take (J_NONE: no cable), the inputs that take one */
enum { J_NONE, J_VCA, J_KBCV, J_VCO, J_LFO, J_EG, J_SH, J_ONE, J_ONETWO, J_ATTN, J_MULT1, J_MULT2, J_VELO, J_WHEEL,
       J_PRESS, J_CLOCK, J_DUMMY, NJSRC };      /* (0.3: J_VCA and the mult no longer offered; CLOCK, DUMMY appended) */
enum { I_1VOCT, I_PWM, I_CUTOFF, I_VCACV, I_LFORATE, I_GATE, I_FOLD, I_SHIN, I_SHGATE, I_ONE, I_TWO, I_ATTN, I_MULT,
       NJIN };
static const char *const JSRC_NAME[NJSRC] = {"---", "VCA", "KB CV", "VCO", "LFO", "EG", "S+H", "ONE", "ONE+TWO",
    "ATTN", "MULT 1", "MULT 2", "VELO", "MODWHL", "PRESS", "CLOCK", "DUMMY"};
/* the order a jack's knob runs through its choices: no cable, the panel's outputs, the clock, the MIDI sources, and
 * last a dummy (a plug on its own: the normal broken, 0 V; 0.3.1, the user: "in fondo all'elenco") */
static const uint8_t JORDER[] = {J_NONE, J_KBCV, J_VCO, J_LFO, J_EG, J_SH, J_ONE, J_ONETWO, J_ATTN, J_CLOCK, J_VELO,
    J_WHEEL, J_PRESS, J_DUMMY};
#define NJORDER (sizeof JORDER / sizeof JORDER[0])
#define JS_EXT(s) ((s) >= J_VELO && (s) <= J_CLOCK)   /* from outside the panel (MIDI): a cable from the edge */
static const char *const JIN_NAME[NJIN] = {"1V/OCT", "PWM", "CUTOFF", "VCA CV", "LFO RT", "GATE", "FOLD IN", "S+H IN",
    "S+H GT", "ONE", "TWO", "ATTN", "MULT"};
/* an input with no cable: its normal, as the knob shows it */
static const char *const JIN_NORMAL[NJIN] = {"---", "---", "---", "---", "---", "---", "---", "(VCO)", "(LFO)", "(-5V)",
    "---", "(+5V)", "---"};

enum {
    P_CUTOFF = 0, P_RES, P_VCF_AMT, P_VCF_MIX,
    P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE,
    P_WAVE, P_PW, P_PITCH, P_VCO_MIX, P_PITCH_AMT, P_PWM_AMT,
    P_LFO_RATE, P_LFO_WAVE,
    P_VOLUME, P_VCA_MODE, P_GLIDE, P_KB_SCALE,
    P_FOLD, P_ONE_LVL, P_ATTN,
    P_FINE,
    P_JACK,                                     /* the 13 inputs' cables: P_JACK + I_* */
    P_PRIO = P_JACK + NJIN, P_RETRIG, P_BEND, P_DRIFT,
    P_ARP_MODE, P_ARP_OCT, P_ARP_RATE, P_ARP_GATE,
    /* 0.2: the effects (not on the reference panel): a CHORUS on each track, the DELAY and the REVERB shared, each sound sending */
    P_CH_ON, P_CH_TYPE, P_CH_RATE, P_CH_DEPTH, P_CH_MIX,
    P_DL_ON, P_DL_SEND, P_DL_TIMEL, P_DL_TIMER, P_DL_FB, P_DL_LOW, P_DL_HIGH, P_DL_WIDTH, P_DL_PING,
    P_RV_ON, P_RV_SEND, P_RV_DECAY, P_RV_DAMP, P_RV_PRE, P_RV_DIFF, P_RV_WIDTH,
    /* 0.3: the clock (MIDI clock, else TEMPO): the LFO locked to it, a CLOCK output a cable can take */
    P_LFO_SYNC, P_CLK_DIV, P_CLK_WIDTH,
    NPARAM
};
#define NPARAM_01 45u                           /* 0.1's parameters (its programs: the name at 45; fmav1.c converts them) */
#define NPARAM_02 66u                           /* 0.2's (its programs: the name at 66; fmav1.c converts them) */
#define PJ(i) (P_JACK + (i))

#define PATCH_NAME 12u
#define PATCH_BYTES 96u                         /* NPARAM values, the name at NPARAM, the rest spare (room to append) */
#if NPARAM + PATCH_NAME > PATCH_BYTES
#error "the patch outgrew PATCH_BYTES"
#endif
#define NPROG 99u                               /* (two digits) the factory sounds, then INIT; the user's saves over any */

enum { VCA_EG, VCA_ON };
enum { PR_LOW, PR_LAST, PR_HIGH, NPR };
enum { RT_LEGATO, RT_ALWAYS };
enum { AR_OFF, AR_UP, AR_DOWN, AR_UPDN, AR_UPDN2, AR_DNUP, AR_ORDER, AR_RANDOM, NAR };
enum { CT_I, CT_II, CT_I_II, CT_FREE, NCT };

typedef struct {
    const char *name;                           /* 7 characters at most (the knob label) */
    uint8_t max, def, kind, cc;                 /* cc: a fixed MIDI CC, 0 = given from 20 up in table order */
} pdef_t;

/* how a value shows (param_value) */
enum { K_NUM, K_TEN, K_BTEN, K_CUT, K_ATT, K_DR, K_PITCH, K_FINE, K_WAVE, K_PW, K_MMIX, K_LWAVE, K_LRATE, K_VCAMODE,
       K_GLIDE, K_KBSCL, K_JACK, K_PRIO, K_RETRIG, K_BEND, K_ARP, K_AOCT, K_ADIV, K_PCT, K_ONOFF, K_CTYPE, K_CHZ, K_CHMS,
       K_DDIV, K_LHZ, K_HHZ, K_RMS, K_SYNC, K_CDIV };

#define NDIVS 19u                               /* the note values, slow to fast (the arp takes a part) */
static const char *const DIV_NAME[NDIVS] = {"8/1", "4/1", "2/1", "1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T",
    "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
static const uint8_t DIV_NUM[NDIVS] = {32, 16, 8, 4, 3, 2, 4, 3, 1, 2, 3, 1, 1, 3, 1, 1, 1, 1, 1};   /* of a quarter */
static const uint8_t DIV_DEN[NDIVS] = {1, 1, 1, 1, 1, 1, 3, 2, 1, 3, 4, 2, 3, 8, 4, 6, 8, 12, 16};
#define ARP_DIV0 3u                             /* the arp's RATE: 1/1 .. 1/32T */
#define NARPDIV 15u
#define NDLYDIV 14u                             /* the delay's TIME: 1/64 (0) .. 1/2 (13), DIV index = 18 - v */
#define NCLKDIV 18u                             /* the clock's note values: 8/1 (0) .. 1/32T (17), whole MIDI clocks */
#define DIV_TICKS(d) (24u * DIV_NUM[(d) % NDIVS] / DIV_DEN[(d) % NDIVS])   /* a note value in MIDI clocks (24 a quarter) */

#define JACK_DEF(i, nm) [PJ(i)] = {nm, NJSRC - 1, J_NONE, K_JACK, 0}
static const pdef_t PDEF[NPARAM] = {
    [P_CUTOFF] = {"CUTOFF", 127, 64, K_CUT, 74},
    [P_RES] = {"RESONANCE", 127, 64, K_TEN, 71},
    [P_VCF_AMT] = {"VCF MOD AMT", 127, 64, K_BTEN, 0},
    [P_VCF_MIX] = {"VCF MOD MIX", 127, 64, K_MMIX, 0},
    [P_ATTACK] = {"ATTACK", 127, 0, K_ATT, 73},
    [P_DECAY] = {"DECAY", 127, 64, K_DR, 75},
    [P_SUSTAIN] = {"SUSTAIN", 127, 85, K_TEN, 0},
    [P_RELEASE] = {"RELEASE", 127, 64, K_DR, 72},
    [P_WAVE] = {"VCO WAVE", 127, 64, K_WAVE, 0},
    [P_PW] = {"PULSE WIDTH", 127, 64, K_PW, 0},
    [P_PITCH] = {"PITCH", 120, 60, K_PITCH, 0},
    [P_VCO_MIX] = {"VCO MOD MIX", 127, 64, K_MMIX, 0},
    [P_PITCH_AMT] = {"PITCH MOD AMT", 127, 0, K_TEN, 0},
    [P_PWM_AMT] = {"PWM AMT", 127, 0, K_TEN, 0},
    [P_LFO_RATE] = {"LFO RATE", 127, 64, K_LRATE, 0},
    [P_LFO_WAVE] = {"LFO WAVE", 127, 64, K_LWAVE, 0},
    [P_VOLUME] = {"VOLUME", 127, 100, K_TEN, 0},
    [P_VCA_MODE] = {"VCA MODE", 1, VCA_EG, K_VCAMODE, 0},
    [P_GLIDE] = {"GLIDE", 127, 0, K_GLIDE, 5},
    [P_KB_SCALE] = {"KB SCALE", 127, 0, K_KBSCL, 0},
    [P_FOLD] = {"FOLD", 127, 0, K_TEN, 0},
    [P_ONE_LVL] = {"ONE LVL", 127, 0, K_TEN, 0},
    [P_ATTN] = {"ATTENUATOR", 127, 0, K_TEN, 0},
    [P_FINE] = {"FINE", 100, 50, K_FINE, 0},
    JACK_DEF(I_1VOCT, "1V/OCT"), JACK_DEF(I_PWM, "PWM"), JACK_DEF(I_CUTOFF, "CUTOFF"), JACK_DEF(I_VCACV, "VCA CV"),
    JACK_DEF(I_LFORATE, "LFO RATE"), JACK_DEF(I_GATE, "GATE"), JACK_DEF(I_FOLD, "FOLD IN"), JACK_DEF(I_SHIN, "S+H (VCO)"),
    JACK_DEF(I_SHGATE, "S+H GATE"), JACK_DEF(I_ONE, "ONE (-5)"), JACK_DEF(I_TWO, "TWO"), JACK_DEF(I_ATTN, "ATTN (+5)"),
    JACK_DEF(I_MULT, "MULT"),
    [P_PRIO] = {"PRIO", NPR - 1, PR_LOW, K_PRIO, 0},
    [P_RETRIG] = {"RETRIG", 1, RT_LEGATO, K_RETRIG, 0},
    [P_BEND] = {"BEND", 24, 2, K_BEND, 0},
    [P_DRIFT] = {"ANALOG", 127, 24, K_TEN, 0},
    [P_ARP_MODE] = {"MODE", NAR - 1, AR_OFF, K_ARP, 0},
    [P_ARP_OCT] = {"OCTAVES", 3, 0, K_AOCT, 0},
    [P_ARP_RATE] = {"RATE", NARPDIV - 1, 11, K_ADIV, 0},
    [P_ARP_GATE] = {"GATE", 127, 64, K_PCT, 0},
    [P_CH_ON] = {"CHORUS", 1, 0, K_ONOFF, 0},
    [P_CH_TYPE] = {"TYPE", NCT - 1, CT_I, K_CTYPE, 0},
    [P_CH_RATE] = {"RATE", 127, 52, K_CHZ, 0},
    [P_CH_DEPTH] = {"DEPTH", 127, 94, K_CHMS, 0},
    [P_CH_MIX] = {"SEND", 127, 64, K_PCT, 93},
    [P_DL_ON] = {"DELAY", 1, 0, K_ONOFF, 0},
    [P_DL_SEND] = {"SEND", 127, 30, K_PCT, 92},
    [P_DL_TIMEL] = {"TIME L", NDLYDIV - 1, 8, K_DDIV, 0},
    [P_DL_TIMER] = {"TIME R", NDLYDIV - 1, 10, K_DDIV, 0},
    [P_DL_FB] = {"FEEDBACK", 127, 50, K_TEN, 0},
    [P_DL_LOW] = {"LOW CUT", 127, 30, K_LHZ, 0},
    [P_DL_HIGH] = {"HIGH CUT", 127, 90, K_HHZ, 0},
    [P_DL_WIDTH] = {"WIDTH", 127, 127, K_TEN, 0},
    [P_DL_PING] = {"PING-PONG", 1, 1, K_ONOFF, 0},
    [P_RV_ON] = {"REVERB", 1, 0, K_ONOFF, 0},
    [P_RV_SEND] = {"SEND", 127, 30, K_PCT, 91},
    [P_RV_DECAY] = {"DECAY", 127, 80, K_TEN, 0},
    [P_RV_DAMP] = {"DAMP", 127, 60, K_TEN, 0},
    [P_RV_PRE] = {"PRE-DELAY", 127, 20, K_RMS, 0},
    [P_RV_DIFF] = {"DIFFUSION", 127, 100, K_TEN, 0},
    [P_RV_WIDTH] = {"WIDTH", 127, 127, K_TEN, 0},
    [P_LFO_SYNC] = {"SYNC", NCLKDIV, 0, K_SYNC, 0},   /* 0 FREE (the RATE knob), 1.. a note value of the clock */
    [P_CLK_DIV] = {"CLOCK", NCLKDIV - 1, 11, K_CDIV, 0},   /* 1/8 */
    [P_CLK_WIDTH] = {"GATE", 127, 64, K_PCT, 0},
};
/* the INITIAL PATCH above is the manual's (its page 14: VCO WAVE, PULSE WIDTH, both MOD MIXes, CUTOFF, RESONANCE, LFO
 * RATE and WAVE, DECAY, RELEASE at 12 o'clock, SUSTAIN at 2, the rest at 0). 0.1's defaults were others: its factory
 * programs are lists over those, so they start from them (bit-identical: tests/mfm_programs.txt against 0.1's render) */
static const uint8_t FACT_BASE_01[] = {P_CUTOFF, 104, P_RES, 0, P_VCF_MIX, 0, P_DECAY, 70, P_SUSTAIN, 127, P_RELEASE, 40,
    P_WAVE, 0, P_VCO_MIX, 127, P_LFO_RATE, 60, P_LFO_WAVE, 0, P_ATTN, 64, 255};

/* the values' names (7 characters at most: a knob's LCD) */
static const char *const PRIO_NAME[NPR] = {"LOW", "LAST", "HIGH"};
static const char *const ARP_NAME[NAR] = {"OFF", "UP", "DOWN", "UP/DN", "UP/DN 2", "DN/UP", "ORDER", "RANDOM"};
static const char *const CTYPE_NAME[NCT] = {"I", "II", "I+II", "FREE"};
/* the chorus TYPEs set its RATE and DEPTH as the classic BBD chorus's two buttons did (I, II, both) */
static const uint8_t CT_RATE[3] = {52, 61, 100}, CT_DEPTH[3] = {94, 94, 13};   /* (0.5 Hz, 0.8 Hz, 9.6 Hz; +-1.85 / 0.25 ms) */

/* the patchbay's choices (0.2, the user: "togli dalle selezioni ... l'input-output stesso"): an input never takes its
 * own module's output (the VCO's inputs the VCO, the S+H's the S+H, the mixer's its outputs but ONE into TWO, the
 * manual's amplifier, ...). A bit a source */
/* 0.3 (the user): VCA OUT is the audio out (the headphones are in it): no cable from it; the mult is gone (its input
 * takes nothing, its outputs are offered to no one) */
#define JB(s) (1u << (s))
#define JD_ALL (JB(J_VCA) | JB(J_MULT1) | JB(J_MULT2))
static const uint32_t JIN_DENY[NJIN] = {
    [I_1VOCT] = JD_ALL | JB(J_VCO), [I_PWM] = JD_ALL | JB(J_VCO), [I_CUTOFF] = JD_ALL, [I_VCACV] = JD_ALL,
    [I_LFORATE] = JD_ALL | JB(J_LFO), [I_GATE] = JD_ALL | JB(J_EG), [I_FOLD] = JD_ALL, [I_SHIN] = JD_ALL | JB(J_SH),
    [I_SHGATE] = JD_ALL | JB(J_SH), [I_ONE] = JD_ALL | JB(J_ONE) | JB(J_ONETWO), [I_TWO] = JD_ALL | JB(J_ONETWO),
    [I_ATTN] = JD_ALL | JB(J_ATTN), [I_MULT] = 0xFFFFFFFFu};
#undef JB
static int jack_ok(uint32_t in, uint32_t src) { return src < NJSRC && in < NJIN && !(JIN_DENY[in] >> src & 1u); }
/* a cable an older program has that 0.3 offers no more: from a mult -> from what the mult took; from VCA OUT -> none */
static void patch_fix_jacks(uint8_t *v)
{
    uint32_t i, m = v[PJ(I_MULT)];
    for (i = 0; i < NJIN; i++) {
        uint32_t s = v[PJ(i)];
        if (s == J_MULT1 || s == J_MULT2) s = m;
        if (s >= NJSRC || !jack_ok(i, s)) s = J_NONE;
        v[PJ(i)] = (uint8_t)s;
    }
}

typedef struct { uint8_t v[PATCH_BYTES]; } patch_t;   /* v[0..NPARAM) the parameters, v[NPARAM..] the name */

static void patch_defaults(patch_t *p)
{
    uint32_t i;
    for (i = 0; i < NPARAM; i++) p->v[i] = PDEF[i].def;
    for (; i < PATCH_BYTES; i++) p->v[i] = ' ';
    p->v[NPARAM + 0] = 'I'; p->v[NPARAM + 1] = 'N'; p->v[NPARAM + 2] = 'I'; p->v[NPARAM + 3] = 'T';
}

/* a factory patch: a name and a list of (parameter, value) pairs over the defaults, ended by 255 */
typedef struct { const char *name; const uint8_t *set; } factory_t;
static void patch_from_factory(patch_t *p, const factory_t *f)
{
    const uint8_t *s = f->set;
    uint32_t i;
    patch_defaults(p);
    for (i = 0; FACT_BASE_01[i] != 255u; i += 2) p->v[FACT_BASE_01[i]] = FACT_BASE_01[i + 1];
    while (s[0] != 255u) { p->v[s[0]] = s[1]; s += 2; }
    for (i = 0; i < PATCH_NAME; i++) p->v[NPARAM + i] = ' ';
    for (i = 0; i < PATCH_NAME && f->name[i]; i++) p->v[NPARAM + i] = (uint8_t)f->name[i];
}
