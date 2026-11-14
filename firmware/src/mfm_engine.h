/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine, all in the audio ISR. A TRACK is one whole semi-modular voice: its keyboard (low-note priority, legato triggering,
 * glide, KB SCALE), the EG, the LFO, the VCO, the patchbay's utilities (S+H, the ONE+TWO mixer, the attenuator, the
 * mult, the wave folder), the ladder VCF and the VCA, wired as the panel is wired with no cable, and each of its 13
 * input jacks taking the output its cable comes from (mfm_params.h). The patchbay runs a sample at a time in the
 * circuit's order (LFO, VCO, S+H, ATTN, mixer, MULT, GATE, folder, VCF, VCA): an input reads its output as it is this
 * sample, or the last sample's if that module comes later (a feedback patch: one sample round the loop).
 * Volts in Q12 (VOLT = 1 V): the VCO, the LFO and the S+H swing +-5 V, the EG 0..8 V, the mixer clips at +-8 V, as the
 * manual gives them.
 * NTRK tracks (MFM_NTRK: 1 by default, the user's choice to begin with; 4 as Felucca's: MIDI channels N .. N+3,
 * OMNI: the one on the panel), each a whole voice with its own patch, level and pan.
 * The main loop sends note events through a ring and writes the live patches and the controllers; the ISR owns the
 * voices, the held-note lists, the arpeggiators and the sustain. */
#ifndef RING_PUBLISH
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#endif
#ifndef RING_TAKE
#define RING_TAKE() __asm__ volatile("" ::: "memory")
#endif
#ifndef MFM_NTRK
#define MFM_NTRK 4u                             /* (0.2: four tracks as Felucca's; -DMFM_NTRK=1u: one) */
#endif
#if MFM_NTRK < 1 || MFM_NTRK > 4
#error "FMav-1 plays one to four tracks"
#endif
#define NTRK MFM_NTRK
#define NHELD 16u
#define ENV_ONE (1 << 24)                       /* the EG's top: 8 V */
#define LEVEL_FLOOR 2048                        /* -78 dB: a release below this is over */

enum { E_IDLE, E_ATT, E_DEC, E_REL };          /* (the decay runs on into the sustain, as an analog ADSR's does) */
typedef struct { uint32_t cnt, pos, sounding, running, gidx; } arp_t;   /* gidx: the clock's step it played on */

typedef struct {
    patch_t *p;
    /* the keyboard */
    struct { uint8_t note, vel, phys; } held[NHELD];   /* phys: a key holds it (else the sustain or HOLD) */
    uint32_t nheld;
    arp_t arp;
    uint8_t note, vel, kgate, started, trig;    /* trig: a new gate for the EG (a note after silence) */
    int32_t kb16, target16;                     /* the keyboard's note, gliding (Q16 semitones) */
    /* the EG */
    uint32_t eg_stage;
    int32_t eg, eg0;                            /* this block's end and start (Q24, ENV_ONE = 8 V) */
    uint8_t gate_hi, gate_rise, sh_hi, idle;    /* the GATE jack's and the S+H GATE's Schmitt triggers */
    /* the voice */
    mvco_t vco;
    mlfo_t lfo;
    fold_t fold;
    lad_t lad;
    ladc_t lc;
    int32_t lc_key;                             /* the ladder's coefficients are for this cutoff / resonance */
    uint32_t nrnd;                              /* (0.3.2) the white noise's generator (xorshift), one a track */
    int32_t sh;                                 /* the S+H's held voltage */
    int32_t o[NJSRC];                           /* the outputs, volts (Q12), as they are now */
    int32_t cut_sm, res_sm, pw_sm, wave_sm, lw_sm;   /* the knobs, slewed (a CC's steps glide), Q8 */
    int32_t drift[2];                           /* the analog wander: the VCO, the VCF */
    int32_t amp;                                /* the VCA's gain at the last sample, Q15 (the screen) */
} track_t;

static patch_t live_p[NTRK];                    /* the sounds as they play (the main loop writes, the ISR reads) */
static track_t trk[NTRK];

/* controllers (the main loop writes) */
static volatile int32_t ctl_bend[NTRK];         /* -8192..8191 */
static volatile uint8_t ctl_wheel[NTRK], ctl_after[NTRK];   /* 0..127 */
static volatile uint32_t ctl_sustain, ctl_hold; /* bit t: track t's pedal / HOLD */
static volatile uint32_t ctl_cc7[NTRK];         /* CC 7 (127 at the start: engine_init) */
static volatile int32_t ctl_tune8;              /* global tune, Q8 semitones */
static volatile uint32_t ctl_tempo = 120, ctl_ext_quarter;   /* MIDI clock: frames per quarter, 0 = none */
static volatile uint32_t ctl_arp_restart, ctl_panic;
/* 0.3: the MIDI clock (TRS or USB): the clocks since Start, 0..2303 (0xFFFFFFFF: Start, its first clock not yet in),
 * Start seen, a clock running (else the place runs on at the tempo); REC: every effect off (the sounds keep theirs) */
static volatile uint32_t ctl_clk_ticks = 0xFFFFFFFFu, ctl_clk_start, ctl_clk_run, ctl_fx_off;
static volatile uint32_t ui_lfo_inc[NTRK];       /* the LFO's step a sample (the screen: a fast LFO's light lit steady) */
static volatile uint8_t ctl_lvl[NTRK];          /* the mixer: level 0..127 (100 = unity; set by the app) */
static volatile int8_t ctl_pan[NTRK];           /* -64..64 */

/* the UI's view (the ISR writes) */
static volatile uint8_t ui_note[NTRK];          /* the sounding note, 255: none (engine_init) */
static volatile int32_t ui_env[NTRK];           /* the EG, Q15 */
static volatile int32_t ui_cut8[NTRK];          /* the cutoff as it plays (Q8 semitones) */
static volatile uint8_t ui_estage[NTRK];        /* the EG's stage (E_IDLE ..) */
static volatile int32_t ui_amp[NTRK];           /* the VCA, Q15 */
static volatile int16_t ui_sh_hist[NTRK][16];   /* the S+H's last steps (volts / 4) */
static volatile uint8_t ui_sh_n[NTRK];
static volatile uint32_t ui_render_us;
#define SCOPE_N 240u
static int16_t scope[SCOPE_N];
static volatile uint32_t scope_state, scope_stride = 4;   /* 0 idle, 1 armed, 2 running, 3 done */
static uint32_t rnd_state = 0x9E3779B9u;
static volatile uint32_t eng_t;                 /* frames rendered (the MIDI clock is measured in them) */

/* ---- main loop -> ISR: note events */
typedef struct { uint8_t type, note, vel, trk; } nev_t;   /* type 1 on, 0 off */
#define NQ 64u
static nev_t nq[NQ];
static volatile uint32_t nq_w, nq_r;
static void note_event_t(uint32_t t, uint32_t on, uint32_t note, uint32_t vel)
{
    nev_t e;
    if (nq_w - nq_r >= NQ) return;             /* full: dropped (64 in one loop pass is not playing) */
    e.type = (uint8_t)on; e.note = (uint8_t)(note & 127u); e.vel = (uint8_t)(vel & 127u); e.trk = (uint8_t)t;
    nq[nq_w % NQ] = e;
    RING_PUBLISH();
    nq_w++;
}

/* a quarter in frames: the MIDI clock's when one runs, else the tempo's */
static uint32_t quarter_len(void)
{
    uint32_t q = ctl_ext_quarter;
    if (q) return q;
    return FS * 60u / (ctl_tempo < 30u ? 30u : ctl_tempo);
}
static uint32_t div_frames(uint32_t quarter, uint32_t div)   /* a note value in frames */
{
    return quarter * DIV_NUM[div % NDIVS] / DIV_DEN[div % NDIVS];
}

#include "mfm_fx.h"

/* ---- the clock's place (0.3): MIDI clocks, Q20, wrapping at 2304 (every note value's whole clocks divide it, so a
 * synced LFO, the CLOCK output and the arp never jump at the wrap). It runs at the tempo (the MIDI clock's measured, else
 * TEMPO); while a MIDI clock comes in it stays between the last clock and the next (never ahead, never behind), and
 * Start puts it at 0 (the first clock after Start is the first beat). Once a control block */
#define CLK_WRAP (2304u << 20)
static uint32_t clk_pos, clk_adv;               /* the place, its step a block */
static void clock_block(void)
{
    uint32_t q = quarter_len(), tk = ctl_clk_ticks;
    clk_adv = (CTL * 24u << 20) / (q ? q : 1u);
    if (ctl_clk_start) { ctl_clk_start = 0; clk_pos = 0; }
    if (ctl_clk_run) {
        uint32_t lo, d;
        if (tk == 0xFFFFFFFFu) { clk_pos = 0; return; }   /* (Start: at 0 until its first clock) */
        lo = (tk % 2304u) << 20;
        d = clk_pos >= lo ? clk_pos - lo : clk_pos + (CLK_WRAP - lo);   /* how far past the last clock */
        if (d >= CLK_WRAP / 2u) d = 0;          /* (behind it: up to it) */
        d += clk_adv;
        if (d > (1u << 20) - 1u) d = (1u << 20) - 1u;   /* (never past the next clock before it comes) */
        clk_pos = lo + d;
        if (clk_pos >= CLK_WRAP) clk_pos -= CLK_WRAP;
    } else {
        clk_pos += clk_adv;
        if (clk_pos >= CLK_WRAP) clk_pos -= CLK_WRAP;
    }
}
/* (0.4) one white-noise value, +-5 V (the S+H's normal when the noise is not running) */
static inline int32_t noise_one(uint32_t *st)
{
    uint32_t r = *st;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    *st = r;
    return (((int32_t)(r >> 16) - 32768) * 5) >> 3;
}
/* the Q8 pitch whose step is inc (pitch_inc's inverse, by halving: a synced LFO with a cable on LFO RATE) */
static int32_t p8_of_inc(uint32_t inc)
{
    int32_t lo = -20000, hi = 136 * 256;
    while (hi - lo > 1) { int32_t m = (lo + hi) / 2; if (pitch_inc(m) <= inc) lo = m; else hi = m; }
    return lo;
}

/* ---- the held notes */
static void held_remove(track_t *t, uint32_t note)
{
    uint32_t i, j;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i].note == note) {
            for (j = i; j + 1u < t->nheld; j++) t->held[j] = t->held[j + 1u];
            t->nheld--;
            return;
        }
}
static void held_add(track_t *t, uint32_t note, uint32_t vel)
{
    held_remove(t, note);
    if (t->nheld >= NHELD) { uint32_t j; for (j = 0; j + 1u < NHELD; j++) t->held[j] = t->held[j + 1u]; t->nheld--; }
    t->held[t->nheld].note = (uint8_t)note;
    t->held[t->nheld].vel = (uint8_t)vel;
    t->held[t->nheld].phys = 1;
    t->nheld++;
}
/* the held note the keyboard plays: the classic's is the lowest (PRIO LOW); LAST and HIGH for a sequencer's slides */
static uint32_t held_pick(const track_t *t)
{
    uint32_t i, k = t->nheld - 1u, prio = t->p->v[P_PRIO];
    if (prio == PR_LOW || prio == PR_HIGH)
        for (i = 0; i < t->nheld; i++)
            if (prio == PR_LOW ? t->held[i].note < t->held[k].note : t->held[i].note > t->held[k].note) k = i;
    return k;
}

/* the keyboard goes to a note. A gate after silence starts the EG; with keys still held the classic does not (legato
 * triggering), RETRIG ALWAYS does. The glide always glides (an RC lag on the keyboard's CV) */
static void voice_note(track_t *t, uint32_t note, uint32_t vel, int legato)
{
    t->target16 = (int32_t)note << 16;
    if (!t->started || !t->p->v[P_GLIDE]) t->kb16 = t->target16;
    t->started = 1;
    t->note = (uint8_t)note;
    t->vel = (uint8_t)vel;
    if (!legato || t->p->v[P_RETRIG] == RT_ALWAYS) t->trig = 1;
    t->kgate = 1;
}

static inline uint32_t trk_sus(uint32_t ti) { return (ctl_sustain | ctl_hold) >> ti & 1u; }

static void ev_note_on(uint32_t ti, uint32_t note, uint32_t vel)
{
    track_t *t = &trk[ti];
    uint32_t i, phys = 0, k;
    int legato = t->kgate;
    for (i = 0; i < t->nheld; i++) phys |= t->held[i].phys;
    if ((ctl_hold >> ti & 1u) && !phys) { t->nheld = 0; legato = 0; }   /* HOLD: a new chord replaces the held one */
    held_add(t, note, vel ? vel : 100u);
    if (t->p->v[P_ARP_MODE]) return;            /* the arp plays it */
    k = held_pick(t);
    if (legato && t->held[k].note == t->note) {   /* (a key above the low one: it plays on; ALWAYS retriggers) */
        if (t->p->v[P_RETRIG] == RT_ALWAYS) t->trig = 1;
        return;
    }
    voice_note(t, t->held[k].note, t->held[k].vel, legato);
}
static void voice_follow(track_t *t)            /* a key let go: on to the note the held ones now give, or silence */
{
    uint32_t k;
    if (t->p->v[P_ARP_MODE]) return;
    if (!t->nheld) { t->kgate = 0; return; }
    k = held_pick(t);
    if (t->held[k].note != t->note || !t->kgate) voice_note(t, t->held[k].note, t->held[k].vel, t->kgate);
}
static void ev_note_off(uint32_t ti, uint32_t note)
{
    track_t *t = &trk[ti];
    uint32_t i;
    if (trk_sus(ti)) {                          /* the pedal or HOLD keeps it */
        for (i = 0; i < t->nheld; i++) if (t->held[i].note == note) t->held[i].phys = 0;
        return;
    }
    held_remove(t, note);
    voice_follow(t);
}
static void ev_sustain_up(uint32_t ti)          /* the pedal / HOLD let go: what no key holds goes */
{
    track_t *t = &trk[ti];
    uint32_t i = 0;
    while (i < t->nheld) {
        if (!t->held[i].phys) held_remove(t, t->held[i].note);
        else i++;
    }
    voice_follow(t);
}
static void track_off(track_t *t)               /* a panic: its keys, its arp, its note */
{
    t->nheld = 0;
    t->arp.running = t->arp.sounding = 0;
    t->kgate = 0;
}

static uint32_t sus_was;
static void engine_events(void)
{
    uint32_t sus = 0, ti;
    for (ti = 0; ti < NTRK; ti++) sus |= trk_sus(ti) << ti;
    if (ctl_panic) { ctl_panic = 0; for (ti = 0; ti < NTRK; ti++) track_off(&trk[ti]); }
    for (ti = 0; ti < NTRK; ti++)
        if ((sus_was & ~sus) >> ti & 1u) ev_sustain_up(ti);
    sus_was = sus;
    while (nq_r != nq_w) {
        nev_t e;
        RING_TAKE();
        e = nq[nq_r % NQ];
        nq_r++;
        ti = e.trk % NTRK;
        if (e.type) ev_note_on(ti, e.note, e.vel);
        else ev_note_off(ti, e.note);
    }
}

/* ---- the arpeggiator (not on the reference panel: the FM-1's ARP button): steps on the tempo (or the MIDI clock's) */
static void arp_step(track_t *t)
{
    const uint8_t *pv = t->p->v;
    uint8_t nn[NHELD], vv[NHELD];
    uint32_t n = t->nheld, i, j, oct = pv[P_ARP_OCT] % 4u + 1u, total, k, L, q, mode = pv[P_ARP_MODE];
    for (i = 0; i < n; i++) { nn[i] = t->held[i].note; vv[i] = t->held[i].vel; }
    if (mode != AR_ORDER)                       /* by pitch (ORDER: as played) */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && nn[j - 1u] > nn[j]; j--) {
                uint8_t a = nn[j], b = vv[j];
                nn[j] = nn[j - 1u]; vv[j] = vv[j - 1u]; nn[j - 1u] = a; vv[j - 1u] = b;
            }
    total = n * oct;
    switch (mode) {
    case AR_DOWN: k = total - 1u - t->arp.pos % total; break;
    case AR_UPDN: case AR_DNUP:
        L = total > 1u ? 2u * total - 2u : 1u; q = t->arp.pos % L; k = q < total ? q : L - q;
        if (mode == AR_DNUP) k = total - 1u - k;
        break;
    case AR_UPDN2: L = 2u * total; q = t->arp.pos % L; k = q < total ? q : L - 1u - q; break;
    case AR_RANDOM: k = xorshift(&rnd_state) % total; break;
    default: k = t->arp.pos % total; break;     /* UP, ORDER */
    }
    t->arp.pos++;
    voice_note(t, (uint32_t)clampi((int32_t)nn[k % n] + 12 * (int32_t)(k / n), 0, 127), vv[k % n], 0);
    t->arp.sounding = 1;
}
static void arp_tick(track_t *t)
{
    const uint8_t *pv = t->p->v;
    arp_t *a = &t->arp;
    uint32_t period, gate_len;
    if (!pv[P_ARP_MODE]) {
        if (a->running) { a->running = 0; if (a->sounding) { a->sounding = 0; if (!t->nheld) t->kgate = 0; else voice_follow(t); } }
        return;
    }
    if (!t->nheld) {
        if (a->sounding) { t->kgate = 0; a->sounding = 0; }
        a->running = 0;
        return;
    }
    if (!a->running) { a->running = 1; a->cnt = 0xFFFFFFFFu; a->pos = 0; }
    period = div_frames(quarter_len(), ARP_DIV0 + pv[P_ARP_RATE] % NARPDIV);
    if (period < CTL) period = CTL;
    gate_len = (uint32_t)(((uint64_t)period * (pv[P_ARP_GATE] + 1u)) >> 7);
    if (ctl_clk_run) {                          /* (0.3) a MIDI clock: the steps on its grid (the first at once) */
        uint32_t idx = clk_pos / (DIV_TICKS(ARP_DIV0 + pv[P_ARP_RATE] % NARPDIV) << 20);
        if (a->cnt == 0xFFFFFFFFu || idx != a->gidx) { a->gidx = idx; a->cnt = 0; arp_step(t); }
        else a->cnt += CTL;
    } else if (a->cnt == 0xFFFFFFFFu || a->cnt + CTL >= period) {
        a->cnt = a->cnt == 0xFFFFFFFFu ? 0u : a->cnt + CTL - period;
        a->gidx = clk_pos / (DIV_TICKS(ARP_DIV0 + pv[P_ARP_RATE] % NARPDIV) << 20);
        arp_step(t);
    } else
        a->cnt += CTL;
    if (a->sounding && pv[P_ARP_GATE] < 127u && a->cnt >= gate_len) { t->kgate = 0; a->sounding = 0; }
}

/* ---- the EG, a block at a time (Q24): the attack aims past the top (an RC charging toward 1.3) and turns at it, the
 * decay falls toward the SUSTAIN and stays there (the knob moves it, at the decay's pace), the release falls to 0.
 * A new gate starts the attack from where the level is (no click) */
static void eg_tick(track_t *t, const uint8_t *pv, int gate, int trig)
{
    int32_t sus = pv[P_SUSTAIN] * (ENV_ONE / 127);
    if (trig || (gate && (t->eg_stage == E_IDLE || t->eg_stage == E_REL))) t->eg_stage = E_ATT;
    if (!gate && (t->eg_stage == E_ATT || t->eg_stage == E_DEC)) t->eg_stage = E_REL;
    switch (t->eg_stage) {
    case E_ATT:
        t->eg += (int32_t)(((int64_t)(ENV_ONE + (ENV_ONE * 3 >> 3) - t->eg) * (int32_t)ENV_K_ATT[pv[P_ATTACK] & 127u]) >> 24);
        if (t->eg >= ENV_ONE) { t->eg = ENV_ONE; t->eg_stage = E_DEC; }
        break;
    case E_DEC:
        t->eg += (int32_t)(((int64_t)(sus - t->eg) * (int32_t)ENV_K_DR[pv[P_DECAY] & 127u]) >> 24);
        break;
    case E_REL:
        t->eg += (int32_t)(((int64_t)(0 - t->eg) * (int32_t)ENV_K_DR[pv[P_RELEASE] & 127u]) >> 24) - 32;
        if (t->eg < LEVEL_FLOOR) { t->eg = 0; t->eg_stage = E_IDLE; }
        break;
    default:
        t->eg = 0;
    }
}

/* the knobs' laws */
static inline int32_t sq_q15(uint32_t v) { return (int32_t)(v * v * 2u); }   /* 0..127 -> 0..~1 square law, Q15 (a log pot) */
static inline int32_t lin_q15(uint32_t v) { return (int32_t)(v * 258u); }    /* 0..127 -> 0..~1, Q15 */
static inline int32_t bip_sq_q15(uint32_t v)    /* VCF MOD AMT: 64 = 0, +-1 at the ends, finer near 0 */
{
    int32_t d = (int32_t)v - 64;
    d = d > 0 ? d * 520 : d * 512;              /* +-32767 */
    return (d * (d < 0 ? -d : d)) >> 15;
}
/* the duty of PULSE WIDTH (Q16 of the turn): 5 % (0) .. 50 % (127, a square); the PWM input moves it 4.5 % a volt
 * (the manual: 10 Vpp from the middle sweeps ~5 .. ~50 %), here within 1 .. 99 % */
#define DUTY_MIN16 3277                         /* 5 % */
#define DUTY_SPAN16 29491                       /* to 50 % */
#define DUTY_PER_V16 2949                       /* 4.5 % a volt */

/* ---- one block (CTL frames) of a track, added into out (stereo). The patchbay a sample at a time */
static int track_block(track_t *t, uint32_t ti, int32_t *out)
{
    const uint8_t *pv = t->p->v;
    int32_t *o = t->o;
    uint32_t i, gate, lfo_inc = 0, nz_on;
    int32_t kb8, kbcv, scl, p_base, c_base, deg, vco_mx, vcf_mx, pamt, wamt, famt, vol, attn, one, fg, gl, gr, pos, nlev;
    int32_t lfo_p8, lw, ww, duty0;
    uint8_t jk[NJIN];
    for (i = 0; i < NJIN; i++) jk[i] = (uint8_t)(jack_ok(i, pv[PJ(i)]) ? pv[PJ(i)] : 0u);   /* (never its own output) */
    /* the keyboard: the glide (an RC lag), KB SCALE (1 .. 5 octaves an octave: the low C of the panel's keys is 0 V,
     * MIDI C4 here), the KB CV out */
    if (t->kb16 != t->target16) {
        int32_t diff = t->target16 - t->kb16;
        if (!pv[P_GLIDE] || (diff > -64 && diff < 64)) t->kb16 = t->target16;
        else t->kb16 += (int32_t)(((int64_t)diff * GLIDE_K[pv[P_GLIDE] & 127u]) >> 16);
    }
    kb8 = (t->kb16 >> 8) - 60 * 256;
    scl = 4096 + (int32_t)pv[P_KB_SCALE] * 4 * 4096 / 127;   /* Q12, 1 .. 5 */
    kbcv = (((kb8 * 4) / 3) * scl) >> 12;       /* Q8 semitones -> volts (12 * 256 = 1 V = 4096) x KB SCALE */
    o[J_KBCV] = kbcv;
    /* the MIDI sources (not on the reference panel): 0 .. +5 V */
    o[J_VELO] = t->vel * 5 * VOLT / 127;
    o[J_WHEEL] = ctl_wheel[ti] * 5 * VOLT / 127;
    o[J_PRESS] = ctl_after[ti] * 5 * VOLT / 127;
    /* the EG: the keyboard's gate or the GATE jack's (its Schmitt trigger runs each sample below) */
    gate = t->kgate || (jk[I_GATE] && t->gate_hi);
    t->eg0 = t->eg;
    eg_tick(t, pv, (int)gate, t->trig || t->gate_rise);
    t->trig = 0; t->gate_rise = 0;
    for (i = 0; i < 2u; i++)                    /* the analog wander: a slow random walk each */
        t->drift[i] += (((int32_t)(xorshift(&rnd_state) >> 20) - 2048) >> 6) - (t->drift[i] >> 11);
    ui_env[ti] = t->eg >> 9;
    ui_estage[ti] = (uint8_t)t->eg_stage;
    /* the knobs, slewed */
    t->cut_sm += (((int32_t)pv[P_CUTOFF] << 8) - t->cut_sm) >> 3;
    t->res_sm += (((int32_t)pv[P_RES] << 8) - t->res_sm) >> 3;
    t->pw_sm += (((int32_t)pv[P_PW] << 8) - t->pw_sm) >> 3;
    t->wave_sm += (((int32_t)pv[P_WAVE] << 8) - t->wave_sm) >> 3;
    t->lw_sm += (((int32_t)pv[P_LFO_WAVE] << 8) - t->lw_sm) >> 3;
    /* the LFO: its rate a pitch (LFO_P8), the LFO RATE input 1 V an octave */
    lfo_p8 = LFO_P8[pv[P_LFO_RATE] & 127u];
    if (!jk[I_LFORATE]) lfo_inc = pitch_inc(lfo_p8);
    if (pv[P_LFO_SYNC]) {                       /* (0.3) SYNC: a note value of the clock, its phase the clock's place */
        uint32_t tk = DIV_TICKS((pv[P_LFO_SYNC] - 1u) % NCLKDIV), inc = (clk_adv << 12) / (tk * CTL);
        if (!jk[I_LFORATE]) { t->lfo.ph = ((clk_pos % (tk << 20)) / tk) << 12; lfo_inc = inc; }
        else lfo_p8 = p8_of_inc(inc);           /* (a cable on LFO RATE: the CV around the synced rate, free) */
    }
    ui_lfo_inc[ti] = jk[I_LFORATE] ? pitch_inc(lfo_p8) : lfo_inc;
    {                                           /* (0.3) CLOCK: high for GATE of each note value of the clock; DUMMY 0 V */
        uint32_t tk = DIV_TICKS(pv[P_CLK_DIV] % NCLKDIV);
        o[J_CLOCK] = ((clk_pos % (tk << 20)) >> 8) < (((tk << 12) * (pv[P_CLK_WIDTH] + 1u)) >> 7) ? 5 * VOLT : 0;
        o[J_DUMMY] = 0;
    }
    lw = (t->lw_sm * 258) >> 8;                 /* (Q15) */
    /* nothing to hear: the EG at rest, the VCA closed (its MODE on EG, no cable on VCA CV or GATE). The LFO and the VCO
     * run on (free-running), the S+H keeps what it holds */
    if (t->eg_stage == E_IDLE && !gate && pv[P_VCA_MODE] == VCA_EG && (jk[I_VCACV] == J_NONE || jk[I_VCACV] == J_DUMMY)
        && (jk[I_GATE] == J_NONE || jk[I_GATE] == J_DUMMY)) {
        uint32_t li = jk[I_LFORATE] ? pitch_inc(lfo_p8 + ((o[jk[I_LFORATE]] * 3) >> 2)) : lfo_inc;
        t->lfo.ph += li * CTL;
        t->vco.ph += pitch_inc(60 * 256) * CTL;
        o[J_EG] = 0; o[J_VCA] = 0;
        t->amp = 0; ui_amp[ti] = 0; ui_note[ti] = 255u; t->idle = 1;
        return 0;
    }
    t->idle = 0;
    ui_note[ti] = t->note;
    /* the pitch: PITCH (+-5 octaves), FINE, the keyboard's CV, the bend, the tune, the wander; per sample: 1V/OCT and
     * the VCO MOD */
    deg = (t->drift[0] * pv[P_DRIFT]) >> 13;
    p_base = 60 * 256 + ((int32_t)pv[P_PITCH] - 60) * 256 + ((int32_t)pv[P_FINE] - 50) * 256 / 100 + ((kbcv * 3) >> 2)
           + (ctl_bend[ti] * (int32_t)pv[P_BEND] * 256 >> 13) + ctl_tune8 + deg;
    vco_mx = lin_q15(pv[P_VCO_MIX]);            /* EG (0) .. LFO (32767) */
    vcf_mx = lin_q15(pv[P_VCF_MIX]);
    pamt = sq_q15(pv[P_PITCH_AMT]);             /* full: 1 V an octave of the mix */
    wamt = lin_q15(pv[P_PWM_AMT]);              /* full: 4.5 % a volt of the mix (as the PWM jack) */
    famt = bip_sq_q15(pv[P_VCF_AMT]);           /* full: 1 V an octave of the mix (EG at its top: +8 octaves) */
    ww = (t->wave_sm * 258) >> 8;
    duty0 = DUTY_MIN16 + t->pw_sm * DUTY_SPAN16 / (127 * 256);
    /* the cutoff: 30 Hz .. 20 kHz over the knob, the wander */
    c_base = CUT_LO8 + (t->cut_sm >> 4) * (CUT_SPAN8 >> 3) / 254 + ((t->drift[1] * pv[P_DRIFT]) >> 11);
    vol = sq_q15(pv[P_VOLUME]);                 /* (a log pot) */
    attn = lin_q15(pv[P_ATTN]);
    one = lin_q15(pv[P_ONE_LVL]);
    fg = 4096 + (int32_t)pv[P_FOLD] * 161;      /* the folder's gain, Q12: 1 .. 6 (three folds of a 5 V wave) */
    nlev = lin_q15(pv[P_NOISE]);                /* (0.3.2) the noise into the VCF: +-5 V at the top */
    for (i = 0, nz_on = nlev != 0; i < NJIN; i++) nz_on |= jk[i] == J_NOISE;   /* (it runs only when heard) */
    pos = clampi(ctl_pan[ti], -64, 64) + 64;    /* (the track's pan, from the mixer; x 2: the middle is +3 dB, 5 V out of
                                                 * the VCA ~-1 dBFS) */
    gl = PAN_Q12[128 - pos] * 2; gr = PAN_Q12[pos] * 2;
    for (i = 0; i < CTL; i++) {
        int32_t eg = t->eg0 + (int32_t)(((t->eg - t->eg0) >> 4) * (int32_t)i), egv = eg >> 9, lfo, mv, p8, x, y, c8, g, s, d16;
        uint32_t li;
        o[J_EG] = egv;
        if (nz_on) o[J_NOISE] = noise_one(&t->nrnd);   /* (0.4) white noise, +-5 V, the track's own, while it is heard */
        /* LFO */
        li = jk[I_LFORATE] ? pitch_inc(lfo_p8 + ((o[jk[I_LFORATE]] * 3) >> 2)) : lfo_inc;
        lfo = mlfo_tick(&t->lfo, li, lw);
        o[J_LFO] = (lfo * 5) >> 3;              /* +-5 V */
        /* VCO: its modulation the EG / LFO mix */
        mv = egv + (((o[J_LFO] - egv) * (vco_mx >> 1)) >> 14);
        p8 = p_base + ((mv * 3 * (pamt >> 1)) >> 16);
        if (jk[I_1VOCT]) p8 += (o[jk[I_1VOCT]] * 3) >> 2;
        p8 = clampi(p8, -1 * 256, 135 * 256);   /* (8 Hz .. 16 kHz and a little) */
        /* the width: the knob, the PWM input, the PWM AMT of the mix (4.5 % a volt both), 1 .. 99 % */
        d16 = ((mv * (wamt >> 1)) >> 14) + (jk[I_PWM] ? o[jk[I_PWM]] : 0);
        d16 = clampi(duty0 + ((clampi(d16, -20 * VOLT, 20 * VOLT) * DUTY_PER_V16) >> 12), 655, 64881);
        x = mvco_tick(&t->vco, pitch_inc(p8), (uint32_t)d16 << 16, ww);
        o[J_VCO] = (x * 5) >> 3;                /* +-5 V */
        /* S+H: its gate the LFO (normal) or the jack, its input the VCO (normal) or the jack; it takes on the rise */
        {
            int32_t gv = o[jk[I_SHGATE] ? jk[I_SHGATE] : J_LFO];
            if (!t->sh_hi && gv > 5 * VOLT / 2) {
                t->sh_hi = 1;                   /* (0.4: no cable in S+H IN: the white noise, drawn now if it is not running) */
                t->sh = jk[I_SHIN] ? o[jk[I_SHIN]] : nz_on ? o[J_NOISE] : noise_one(&t->nrnd);
                ui_sh_hist[ti][ui_sh_n[ti] & 15u] = (int16_t)clampi(t->sh >> 2, -32767, 32767); ui_sh_n[ti]++;
            }
            else if (t->sh_hi && gv < VOLT) t->sh_hi = 0;
            o[J_SH] = t->sh;
        }
        /* ATTN: the input (normal: +5 V) x ATTENUATOR; the mixer: ONE (normal: -5 V) x ONE LVL, + TWO (unity),
         * clipping at +-8 V (0.3: the mult is gone) */
        o[J_ATTN] = ((jk[I_ATTN] ? o[jk[I_ATTN]] : 5 * VOLT) * (attn >> 3)) >> 12;
        o[J_ONE] = clampi(((jk[I_ONE] ? o[jk[I_ONE]] : -5 * VOLT) * (one >> 3)) >> 12, -8 * VOLT, 8 * VOLT);
        o[J_ONETWO] = clampi(o[J_ONE] + (jk[I_TWO] ? o[jk[I_TWO]] : 0), -8 * VOLT, 8 * VOLT);
        /* the GATE jack: high over 3.5 V, low under 1.5 V; a rise starts the EG (next block) */
        if (jk[I_GATE]) {
            int32_t gv = o[jk[I_GATE]];
            if (!t->gate_hi && gv > 7 * VOLT / 2) { t->gate_hi = 1; t->gate_rise = 1; }
            else if (t->gate_hi && gv < 3 * VOLT / 2) t->gate_hi = 0;
        }
        /* the VCF's input: FOLD IN patched -> the folder (the VCO bypassed), else the VCO */
        x = jk[I_FOLD] ? fold_tick(&t->fold, o[jk[I_FOLD]], fg) : o[J_VCO];
        if (nlev) x += (o[J_NOISE] * nlev) >> 15;   /* (0.3.2) the noise, mixed in with its level */
        /* the cutoff: the knob, the CUTOFF input (1 V an octave), the VCF MOD (the EG / LFO mix x the bipolar AMT) */
        mv = egv + (((o[J_LFO] - egv) * (vcf_mx >> 1)) >> 14);
        c8 = c_base + ((((mv * 3) >> 2) * (famt >> 1)) >> 14);
        if (jk[I_CUTOFF]) c8 += (o[jk[I_CUTOFF]] * 3) >> 2;
        c8 = clampi(c8, 16 * 256, 136 * 256);
        {
            int32_t key = (c8 >> 4) | (t->res_sm >> 4) << 16;   /* (new coefficients past 1/16 semitone) */
            if (key != t->lc_key) { t->lc_key = key; ladder_coef(&t->lc, c8, t->res_sm); }
        }
        y = ladder_tick(&t->lad, &t->lc, (x * 7) >> 3);   /* (5 V = 17920 of the ladder's unit: the VCO drives it a
                                                 * little, the mixer's 8 V more) */
        y = (y * 73) >> 6;                      /* (back to volts) */
        /* the VCA: the EG (MODE EG) or open (ON), plus the VCA CV input (0 .. 8 V), x VOLUME */
        g = (pv[P_VCA_MODE] == VCA_ON ? 8 * VOLT : egv) + (jk[I_VCACV] ? o[jk[I_VCACV]] : 0);
        g = clampi(g, 0, 8 * VOLT);             /* (8 V = 32768: Q15) */
        g = (g * (vol >> 1)) >> 14;
        s = (clampi(y, -12 * VOLT, 12 * VOLT) * (g >> 1)) >> 14;
        o[J_VCA] = s;
        out[2u * i] += (s * gl) >> 12;
        out[2u * i + 1u] += (s * gr) >> 12;
        t->amp = g;
        if (i == CTL - 1u) ui_cut8[ti] = c8;
    }
    ui_amp[ti] = t->amp;
    return 1;
}

/* ---- the audio half: events, arps and voices per control block; each track through its level and pan into the
 * mix; the output stage */
static int32_t mbus[2u * HALF_FRAMES], dsend[2u * HALF_FRAMES], rsend[HALF_FRAMES], csend[HALF_FRAMES];
static int32_t lvl_g[NTRK];                     /* each track's level x CC 7 as it moves, Q12 */
static volatile uint32_t ui_trk_peak[NTRK];
/* the audio interrupt starts with the platform, before app_main has set anything up (on the chip): until engine_init
 * has run, the halves stay silent (a patch read through a null pointer at boot crashed here) */
static volatile uint32_t engine_ready;
#define QUIET_PEAK 32u                          /* ~-60 dBFS: below it the output counts as silent (engine_quiet) */
#define SOFT_PEAK 512u                          /* ~-36 dBFS: quiet enough once a save has waited long */
static volatile uint32_t eng_loud_ms, eng_soft_ms, ui_out_peak;
static int32_t tbus[2u * CTL];
#ifdef OBV_HOST
static uint32_t host_blocks[NTRK];              /* (the host's report: control blocks each track rendered) */
static void host_blocks_report(void)
{
    uint32_t ti;
    printf("blocks rendered per track:");
    for (ti = 0; ti < NTRK; ti++) printf(" %u", (unsigned)host_blocks[ti]);
    printf("\n");
}
#endif

static void engine_audio(int32_t *out, uint32_t n)
{
    uint32_t b, i, ti, pk = 0, dl_fed = 0, rv_fed = 0;
    int32_t cm[NTRK];
    const patch_t *pc = 0;
    static int32_t sc_prev;
    static uint32_t sc_i, sc_sub;
    int32_t tg[NTRK], dg[NTRK], dm[NTRK], rm[NTRK];
    const patch_t *pd = 0, *pr = 0;
    if (!engine_ready) return;
    for (i = 0; i < 2u * n; i++) mbus[i] = dsend[i] = 0;
    for (i = 0; i < n; i++) rsend[i] = csend[i] = 0;
    for (ti = 0; ti < NTRK; ti++) {             /* the choruses; the delay / reverb of the first track that has them on;
                                                 * each sound's sends (Q15) */
        const uint8_t *v = trk[ti].p->v;
        uint32_t on = !ctl_fx_off;              /* (0.3: REC off: every effect off, the sounds' switches as they are) */
        if (!pc && on && v[P_CH_ON]) pc = trk[ti].p;
        if (!pd && on && v[P_DL_ON]) pd = trk[ti].p;
        if (!pr && on && v[P_RV_ON]) pr = trk[ti].p;
        dm[ti] = on && v[P_DL_ON] ? v[P_DL_SEND] * 258 : 0;
        rm[ti] = on && v[P_RV_ON] ? v[P_RV_SEND] * 258 : 0;
        cm[ti] = on && v[P_CH_ON] ? v[P_CH_MIX] * 258 : 0;
    }
    for (ti = 0; ti < NTRK; ti++) {             /* the mixer's level x CC 7, gliding over the half (no zipper) */
        int32_t cc = (int32_t)(ctl_cc7[ti] * ctl_cc7[ti] * 4096u / 16129u);
        tg[ti] = ((int32_t)(ctl_lvl[ti] * ctl_lvl[ti] * 4096u / 10000u) * cc) >> 12;
        dg[ti] = (tg[ti] - lvl_g[ti]) * 16 / (int32_t)n;
    }
    for (b = 0; b < n; b += CTL) {
        engine_events();
        eng_t += CTL;
        clock_block();
        if (ctl_arp_restart) {                  /* MIDI start: the arps from the top */
            ctl_arp_restart = 0;
            for (ti = 0; ti < NTRK; ti++) { trk[ti].arp.pos = 0; trk[ti].arp.cnt = 0xFFFFFFFFu; }
        }
        for (ti = 0; ti < NTRK; ti++) {
            uint32_t tp = 0;
            arp_tick(&trk[ti]);
            for (i = 0; i < 2u * CTL; i++) tbus[i] = 0;
            if (track_block(&trk[ti], ti, tbus)) {
                int32_t g = lvl_g[ti] * 16 + dg[ti] * (int32_t)b;
#ifdef OBV_HOST
                host_blocks[ti]++;
#endif
                for (i = 0; i < CTL; i++) {     /* a frame: the level, into the mix and the sends */
                    int32_t gg = (g + dg[ti] * (int32_t)i) >> 4;
                    int32_t l = (clampi(tbus[2u * i], -262143, 262143) * gg) >> 12, r = (clampi(tbus[2u * i + 1u], -262143, 262143) * gg) >> 12;
                    int32_t al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
                    mbus[2u * (b + i)] += l; mbus[2u * (b + i) + 1u] += r;
                    if (dm[ti]) { dsend[2u * (b + i)] += ((l >> 2) * (dm[ti] >> 3)) >> 10; dsend[2u * (b + i) + 1u] += ((r >> 2) * (dm[ti] >> 3)) >> 10; dl_fed = 1; }
                    if (rm[ti]) { rsend[b + i] += ((((l + r) >> 1) >> 2) * (rm[ti] >> 3)) >> 10; rv_fed = 1; }
                    if (cm[ti]) csend[b + i] += ((((l + r) >> 1) >> 2) * (cm[ti] >> 3)) >> 10;
                    if ((uint32_t)al > tp) tp = (uint32_t)al;
                    if ((uint32_t)ar > tp) tp = (uint32_t)ar;
                }
            }
            if (tp > ui_trk_peak[ti]) ui_trk_peak[ti] = tp;
        }
    }
    for (ti = 0; ti < NTRK; ti++) lvl_g[ti] = tg[ti];
    /* the sends: off (no sound has them on) cuts the tail (dirty: cleared before it runs again); idle (nothing sent,
     * the tail silent) runs nothing */
    if (!pd) { if (fxs.dl_on) dl_dirty = 1; dl_live = 0; }
    else if (!dl_fed && !dl_live) { if (fxs.dl_on && dl_clr < DLY_MAX) dl_dirty = 1; pd = 0; }
    if (!pr) { if (fxs.rv_on) rv_dirty = 1; rv_live = 0; }
    else if (!rv_fed && !rv_live) pr = 0;
    chs_block(pc);
    fxs_block(pd, pr, quarter_len());
    fxs_process(mbus, csend, dsend, rsend, n);
    for (i = 0; i < 2u * n; i++) {              /* a DC blocker (~7 Hz): the pulse's offset, a driven filter. Its state in
                                                 * Q8, so the leak never stalls on a rounding (a dead band) */
        static int32_t dcx[2], dcy[2];
        int32_t x = clampi(mbus[i], -262144, 262143), *y = &dcy[i & 1u];
        *y = clampi(*y + (x - dcx[i & 1u]) * 256 - (((*y >> 4) * 34) >> 11), -(262144 << 8), (262143 << 8));
        dcx[i & 1u] = x;
        mbus[i] = *y >> 8;
    }
    for (i = 0; i < n; i++) {                   /* the output stage: a gentle tanh, never a hard clip */
        int32_t l = softclip(clampi(mbus[2u * i], -262144, 262143));
        int32_t r = softclip(clampi(mbus[2u * i + 1u], -262144, 262143));
        int32_t m = (l + r) >> 1;
        uint32_t al = (uint32_t)(l < 0 ? -l : l), ar = (uint32_t)(r < 0 ? -r : r);
        if (al > pk) pk = al;
        if (ar > pk) pk = ar;
        out[2u * i] += l;
        out[2u * i + 1u] += r;
        if (scope_state == 1u) {                /* armed: wait for a rising zero crossing */
            if (sc_prev < 0 && m >= 0) { scope_state = 2; sc_i = 0; sc_sub = 0; }
        } else if (scope_state == 2u) {
            if (!sc_sub++) scope[sc_i++] = (int16_t)m;
            if (sc_sub >= scope_stride) sc_sub = 0;
            if (sc_i >= SCOPE_N) scope_state = 3;
        }
        sc_prev = m;
    }
    if (pk > ui_out_peak) ui_out_peak = pk;
    if (pk > QUIET_PEAK) eng_loud_ms = fm1_ms;
    if (pk > SOFT_PEAK) eng_soft_ms = fm1_ms;
}

/* nothing sounds: a flash erase is safe (it stops the audio ~50 ms a sector). The voices idle and the output under
 * ~-60 dBFS for QUIET_MS; a sound that never gets there (a drone: VCA MODE ON) is taken under ~-36 dBFS once the
 * voices have been idle QUIET_WAIT_MS, so a save is put off, never lost (OberVave's rule) */
#define QUIET_MS 300u
#define QUIET_WAIT_MS 20000u
static uint32_t quiet_idle_t, quiet_idle;
static int engine_quiet(void)
{
    uint32_t ti;
    for (ti = 0; ti < NTRK; ti++)
        if (trk[ti].kgate || trk[ti].eg_stage != E_IDLE) { quiet_idle = 0; return 0; }
    if (nq_r != nq_w) { quiet_idle = 0; return 0; }
    if (!quiet_idle) { quiet_idle = 1; quiet_idle_t = fm1_ms; }
    if (fm1_ms - eng_loud_ms >= QUIET_MS) return 1;
    return fm1_ms - quiet_idle_t >= QUIET_WAIT_MS && fm1_ms - eng_soft_ms >= QUIET_MS;
}

static void engine_init(void)
{
    uint32_t ti;
    for (ti = 0; ti < NTRK; ti++) {
        track_t *t = &trk[ti];
        uint32_t seed = 0x12345678u * (ti + 1u) + 0x9E3779B9u;
        t->p = &live_p[ti];
        t->vco.ph = xorshift(&seed);            /* free-running: anywhere */
        t->lfo.ph = xorshift(&seed);
        t->cut_sm = 104 << 8;
        t->pw_sm = 64 << 8;
        t->note = 60;
        t->kb16 = t->target16 = 60 << 16;
        t->lc_key = -1;
        t->nrnd = 0x2545F491u * (ti + 1u);      /* (never 0: xorshift stays at 0) */
        ctl_cc7[ti] = 127; ui_note[ti] = 255;
    }
    fx_init();
    RING_PUBLISH();
    engine_ready = 1;
}
