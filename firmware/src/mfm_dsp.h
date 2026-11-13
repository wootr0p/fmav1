/* SPDX-License-Identifier: GPL-3.0-only */
/* FMav-1's DSP building blocks, all fixed point (no FPU, no 64-bit division, no 64-bit products in the sample loops).
 * Everything runs a sample at a time, as the analog circuit does: any output may go to any input through the patchbay,
 * the LFO reaches 550 Hz, so the pitch, the pulse width and the cutoff all move at audio rate.
 *
 *  - the VCO: one saw core, a comparator pulse off it (its width from PULSE WIDTH, the PWM input and the modulation),
 *    the WAVE knob crossfading saw > pulse; the saw's reset and the pulse's two edges band-limited (polyBLEP).
 *  - the LFO: a triangle and a square off one core, WAVE crossfading them; the square's edges band-limited (it is an
 *    audio oscillator up to 550 Hz).
 *  - the wave folder: a triple folder (folds at 5, 15, 25 V) with rounded peaks (the LED folder's soft knees), run at
 *    twice the rate.
 *  - the VCF: the four-pole transistor ladder (after FMini-1's ladder, Ruppert, GPL-3.0: four
 *    differential pairs, the emphasis network's lag pole, the loop predicted from the states), its coefficients each
 *    sample when the cutoff moves.
 * From OberVave: the polyBLEP event, the tanh table, the pitch.
 *
 * Signals: the patchbay's in volts, Q12 (1 V = 4096: the VCO swings +-5 V = +-20480); the ladder's own unit is
 * +-32768 = 1.0 (tanh(1) at 32768), its states in Q8. */
#include "mfm_tables.h"

#define VOLT 4096                               /* 1 V in the patchbay's Q12 */

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* tanh(x / 32768) * 32767, interpolated (unity slope at 0), up to |x| = 4.0 (131072), 0.9993 beyond */
static inline __attribute__((always_inline)) int32_t softclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i = a >> 8, y;
    if (i >= 512)
        y = TANH_Q15[512];
    else
        y = TANH_Q15[i] + (((TANH_Q15[i + 1] - TANH_Q15[i]) * (a & 255)) >> 8);
    return x < 0 ? -y : y;
}

/* a * k / 65536 rounded to the nearest, a within +-2^25, k 0..65536 (FMini-1's: a floor in each of four stages leaves
 * a bias a quiet low ladder cannot sing against) */
static inline __attribute__((always_inline)) int32_t mulr(int32_t a, int32_t k)
{
    return ((a >> 16) * k) + (int32_t)(((uint32_t)(a & 0xFFFF) * (uint32_t)k + 32768u) >> 16);
}

static inline uint32_t xorshift(uint32_t *st)
{
    uint32_t s = *st;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *st = s;
    return s;
}

/* Q8 semitones (0 = MIDI 0, 8.18 Hz) -> phase step per frame; 12 octave values x 256 fine steps. Below MIDI 0 the step
 * halves an octave at a time (the LFO goes down to 0.1 Hz, 76 semitones under). 32-bit products only: it runs each
 * sample (a*f >> 16 split: the high half times f, the low half times f / 2) */
static inline __attribute__((always_inline)) uint32_t pitch_inc(int32_t p)
{
    uint32_t n, o, sh = 0, a, f, r;
    while (p < 0 && sh < 12u) { p += 12 * 256; sh++; }
    p = clampi(p, 0, 131 * 256 + 255);
    n = (uint32_t)p >> 8;
    o = n / 12u;
    a = INC_C9[n - o * 12u];
    f = FINE_Q16[p & 255];
    r = (a >> 16) * f + (((a & 0xFFFFu) * (f >> 1)) >> 15);   /* a * f >> 16 */
    return (r >> (10u - o)) >> sh;
}

/* a sine of the phase, Q15, interpolated in the 1024-step table */
static inline __attribute__((always_inline)) int32_t sine_q15(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINE_Q15[i], f = (int32_t)((ph >> 12) & 1023u);
    return a + (((SINE_Q15[i + 1u] - a) * f) >> 10);
}

/* the polyBLEP event: a jump of H at t (0..32767 of a sample) before the current sample (the oscillator outputs one
 * sample late, its previous sample still open to the correction) */
static inline __attribute__((always_inline)) void osc_event(int32_t *prev, int32_t *cur, int32_t h, int32_t t)
{
    int32_t u = 32767 - t, h16 = h >> 4;
    t = clampi(t, 0, 32767);
    u = 32767 - t;
    *prev += (h16 * ((t * t) >> 15)) >> 12;
    *cur -= (h16 * ((u * u) >> 15)) >> 12;
}

/* ================================================================== the VCO */
typedef struct { uint32_t ph; int32_t prev; } mvco_t;
/* one sample: the phase steps by inc; duty: the pulse's high part of the turn (a phase); w: the pulse's share (WAVE,
 * Q15: 0 saw, 32768 pulse). Out: the previous sample, band-limited, +-32768 */
static inline __attribute__((always_inline)) int32_t mvco_tick(mvco_t *v, uint32_t inc, uint32_t duty, int32_t w)
{
    uint32_t old = v->ph, ph = old + inc, d = inc >> 15;
    int32_t ws = 32768 - w, saw = (int32_t)(ph >> 16) - 32768, pls = ph < duty ? 32767 : -32768;
    int32_t cur = ((saw * ws) >> 15) + ((pls * w) >> 15), out = v->prev;
    if (d) {
        int wrapped = ph < old;
        if (wrapped)                            /* the saw falls 2, the pulse rises 2 (it goes high at the reset) */
            osc_event(&out, &cur, 2 * (w - ws), (int32_t)(ph / d));
        if (w && ph >= duty && (wrapped || old < duty))   /* the pulse falls at its width */
            osc_event(&out, &cur, -2 * w, (int32_t)((ph - duty) / d));
    }
    v->ph = ph; v->prev = cur;
    return out;
}

/* ================================================================== the LFO */
typedef struct { uint32_t ph; int32_t prev; } mlfo_t;
/* one sample: w the square's share (Q15); out +-32768, the previous sample (the square band-limited) */
static inline __attribute__((always_inline)) int32_t mlfo_tick(mlfo_t *l, uint32_t inc, int32_t w)
{
    uint32_t old = l->ph, ph = old + inc, d = inc >> 15;
    int32_t p = (int32_t)((ph + 0x40000000u) >> 15), tri = p < 65536 ? p - 32768 : 98303 - p;
    int32_t sq = ph < 0x80000000u ? 32767 : -32768, cur = ((tri * (32768 - w)) >> 15) + ((sq * w) >> 15), out = l->prev;
    if (w && d) {
        if (ph < old) osc_event(&out, &cur, 2 * w, (int32_t)(ph / d));
        if (ph >= 0x80000000u && old < 0x80000000u) osc_event(&out, &cur, -2 * w, (int32_t)((ph - 0x80000000u) / d));
    }
    l->ph = ph; l->prev = cur;
    return out;
}

/* ================================================================== the wave folder */
/* u in volts (Q12): folded at +-5 V (and 15, 25 ...: a triangle of period 20 V), the peaks rounded over the last
 * 1.25 V. Out within +-5 V */
#define FOLD_T (5 * VOLT)
#define FOLD_W (FOLD_T / 4)
static inline __attribute__((always_inline)) int32_t fold1(int32_t u)
{
    int32_t m = (u + FOLD_T) % (4 * FOLD_T), tri, a, d;
    if (m < 0) m += 4 * FOLD_T;
    tri = m < 2 * FOLD_T ? m - FOLD_T : 3 * FOLD_T - m;
    a = tri < 0 ? -tri : tri;
    d = FOLD_T - a;                             /* how far under the fold */
    if (d < FOLD_W) a = FOLD_T - d - (((FOLD_W - d) * (FOLD_W - d)) / (2 * FOLD_W));
    return tri < 0 ? -a : a;
}
typedef struct { int32_t xp, yp; } fold_t;
/* x volts (Q12), g the gain (Q12: 1 .. 6, the FOLD knob): twice the rate (the midpoint, then a 1-2-1 down) */
static inline __attribute__((always_inline)) int32_t fold_tick(fold_t *f, int32_t x, int32_t g)
{
    int32_t u = (clampi(x, -12 * VOLT, 12 * VOLT) * g) >> 12, um = (f->xp + u) >> 1, y1 = fold1(um), y2 = fold1(u);
    int32_t y = (f->yp + 2 * y1 + y2) >> 2;
    f->xp = u; f->yp = y2;
    return y;
}

/* ================================================================== the VCF: the transistor ladder */
typedef struct { int32_t s0, s1, s2, s3, sx, yp; } lad_t;
typedef struct { int32_t G, gx, c0, c1, c2, c3, inv, kinv, tr; } ladc_t;
static inline uint32_t g4_of(int32_t G16)
{
    uint32_t g = (uint32_t)G16, g2 = (g * g) >> 16;
    return (g2 * g2) >> 16;
}
/* the singing pitch lc8 (Q8 semitones), the RESONANCE knob x 256: the loop's coefficients (y4 = G^4 u + c0 s0 + c1 s1
 * + c2 s2 + c3 s3, each stage a trapezoidal one-pole), the loop gain and its solution's 1 / (1 + k Gx G^4) */
static void ladder_coef(ladc_t *c, int32_t lc8, int32_t res8)
{
    int32_t lc = clampi(lc8 >> 4, LAD_GM0 * 16, 136 * 16), ix = (lc >> 4) - LAD_GM0, fr = lc & 15, ri = res8 >> 8, rf = res8 & 255;
    uint32_t gq = LAD_G[ix], gxq = LAD_GX[ix], den, g, om, g2, g3;
    if (ix + 1 < (int32_t)(sizeof LAD_G / sizeof LAD_G[0])) {
        gq = (uint32_t)((int32_t)gq + ((((int32_t)LAD_G[ix + 1] - (int32_t)gq) * fr) >> 4));
        gxq = (uint32_t)((int32_t)gxq + ((((int32_t)LAD_GX[ix + 1] - (int32_t)gxq) * fr) >> 4));
    }
    g = (gq + 128u) >> 8; om = 65536u - g; g2 = (g * g) >> 16; g3 = (g2 * g) >> 16;
    c->G = (int32_t)g;
    c->c3 = (int32_t)(om >> 2);
    c->c2 = (int32_t)((g * (om >> 2)) >> 16);
    c->c1 = (int32_t)((g2 * (om >> 2)) >> 16);
    c->c0 = (int32_t)((g3 * (om >> 2)) >> 16);
    c->gx = (int32_t)((gxq + 128u) >> 8);
    ri = clampi(ri, 0, 127);
    {
        int32_t k12 = LAD_K[ri], tr = LAD_TRIM[ri];
        if (ri < 127) { k12 += ((LAD_K[ri + 1] - k12) * rf) >> 8; tr += ((LAD_TRIM[ri + 1] - tr) * rf) >> 8; }
        den = 65536u + (uint32_t)((k12 * (int32_t)((((gxq >> 8) * g4_of(c->G)) >> 16))) >> 12);   /* 1 + k Gx G^4 */
        c->inv = (int32_t)((1u << 30) / den);
        c->kinv = (k12 * c->inv) >> 14;
        c->tr = tr;
    }
}
/* one sample through the ladder: in the ladder's unit (+-32768 = 1), out the same */
static inline __attribute__((always_inline)) int32_t ladder_tick(lad_t *f, const ladc_t *c, int32_t in)
{
    int32_t s0 = f->s0, s1 = f->s1, s2 = f->s2, s3 = f->s3, sx = f->sx, G = c->G, S, fbc, u, xin, v, y1, y2, y3, y4;
    in = clampi(in, -65535, 65535);
    S = (c->c0 * ((s0 + 128) >> 8) + c->c1 * ((s1 + 128) >> 8) + c->c2 * ((s2 + 128) >> 8) + c->c3 * ((s3 + 128) >> 8)) >> 14;
    fbc = sx + mulr(S * 256 - sx, c->gx);       /* what the stages hold, through the lag pole (Q8) */
    u = ((in * c->inv) >> 14) - ((c->kinv * ((fbc + 128) >> 8)) >> 12);
    xin = softclip(u) * 256;                    /* each stage a differential pair: a tanh in front of it */
    v = mulr(xin - s0, G); y1 = v + s0; s0 = y1 + v;
    v = mulr((softclip((y1 + 128) >> 8) * 256) - s1, G); y2 = v + s1; s1 = y2 + v;
    v = mulr((softclip((y2 + 128) >> 8) * 256) - s2, G); y3 = v + s2; s2 = y3 + v;
    v = mulr((softclip((y3 + 128) >> 8) * 256) - s3, G); y4 = v + s3; s3 = y4 + v;
    v = mulr(y4 - sx, c->gx); sx += v + v;      /* the lag pole follows the output */
    f->s0 = s0; f->s1 = s1; f->s2 = s2; f->s3 = s3; f->sx = sx;
    f->yp = (((y4 + 128) >> 8) * c->tr) >> 12;
    return f->yp;
}
