/* SPDX-License-Identifier: GPL-3.0-only */
/* The effects (0.2, the user: "aggiungi anche negli effetti il delay e il riverbero e chorus"; the reference panel has none), from
 * OberVave's: a CHORUS on each track (an insert after its VCA: the mono voice comes out stereo), the DELAY and the
 * REVERB shared by the four tracks as SENDS: each sound sends by its own SEND (with its ON), and the delay / reverb
 * sound as programmed in the first track whose sound has them on.
 *  CHORUS: the classic polysynth's BBD chorus, a line a side swept by one triangle in opposite directions, the wet
 *   darkened as a bucket brigade does; TYPE I / II / I+II set its RATE and DEPTH as that chorus's buttons did.
 *  DELAY: two lines, note values of the tempo (or the MIDI clock: the Digitakt's), ping-pong or stereo, a low and a
 *   high cut in the feedback, the read heads sliding to a new time as a tape's do.
 *  REVERB: J. Dattorro's figure-of-eight plate tank (1997, OberVave's), at half the rate, DIFFUSION on its input
 *   all-passes.
 * Integers only, and no 64-bit products in the sample loops (the chip multiplies 32 x 32). */
#define RVL(x) ((uint32_t)((x) * 44100u / 29761u / 2u))   /* the paper's 29761 Hz lengths at half our rate */
#define DLY_MAX 40960u                          /* a side of the delay: 929 ms */
#define CHO_N 512u                              /* a side of a track's chorus: 11.6 ms */
#define REV_PRE_N 2048u                         /* up to 93 ms of pre-delay (at the half rate) */
#define RV_IN1 RVL(142)
#define RV_IN2 RVL(107)
#define RV_IN3 RVL(379)
#define RV_IN4 RVL(277)
#define RV_AP1 RVL(672)
#define RV_D1 RVL(4453)
#define RV_AP2 RVL(1800)
#define RV_D2 RVL(3720)
#define RV_AP3 RVL(908)
#define RV_D3 RVL(4217)
#define RV_AP4 RVL(2656)
#define RV_D4 RVL(3163)
#define REV_N (REV_PRE_N + RV_IN1 + RV_IN2 + RV_IN3 + RV_IN4 + RV_AP1 + 16u + RV_D1 + RV_AP2 + RV_D2 + RV_AP3 + 16u + RV_D3 + RV_AP4 + RV_D4)
#define FX_MEM_N (CHO_N + 2u * DLY_MAX + REV_N)

typedef struct { int16_t *b; uint32_t len, w; } line_t;   /* w: where the next sample goes */
static int16_t fx_mem[FX_MEM_N] __attribute__((section(".pool")));
static uint32_t fx_used;

static inline int32_t sat16(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }
static void line_init(line_t *l, uint32_t len) { l->b = fx_mem + fx_used; l->len = len; l->w = 0; fx_used += len; }
static void line_clear(line_t *l) { uint32_t i; for (i = 0; i < l->len; i++) l->b[i] = 0; }
static inline __attribute__((always_inline)) int32_t line_read(const line_t *l, uint32_t back)   /* 1 <= back <= len */
{
    uint32_t i = l->w + l->len - back;
    if (i >= l->len) i -= l->len;
    return l->b[i];
}
static inline __attribute__((always_inline)) void line_write(line_t *l, int32_t v)
{
    l->b[l->w] = (int16_t)sat16(v);
    if (++l->w >= l->len) l->w = 0;
}
static inline __attribute__((always_inline)) int32_t ap_step(line_t *l, int32_t in, int32_t g)
{
    int32_t d = line_read(l, l->len), v = in - ((d * g) >> 15);
    line_write(l, v);
    return d + ((v * g) >> 15);
}
static inline __attribute__((always_inline)) int32_t ap_mod(line_t *l, int32_t in, int32_t g, uint32_t base, int32_t e)
{
    uint32_t back = (uint32_t)((int32_t)base + (e >> 8));
    int32_t f = e & 255, a = line_read(l, back), b = line_read(l, back + 1u), d = a + (((b - a) * f) >> 8), v;
    v = in - ((d * g) >> 15);
    line_write(l, v);
    return d + ((v * g) >> 15);
}
static inline int32_t tri_q(uint32_t ph)        /* a triangle -2048..2047 */
{
    int32_t t = (int32_t)(ph >> 19);
    return t < 4096 ? t - 2048 : 6143 - t;
}
static inline __attribute__((always_inline)) int32_t line_read_q16(const line_t *l, int32_t back_q16)
{
    uint32_t b = (uint32_t)back_q16 >> 16;
    int32_t f = (back_q16 >> 8) & 255, a = line_read(l, b), c = line_read(l, b + 1u);
    return a + (((c - a) * f) >> 8);
}
static inline __attribute__((always_inline)) int32_t line_read_q12(const line_t *l, int32_t back_q12)   /* (long lines) */
{
    uint32_t b = (uint32_t)back_q12 >> 12;
    int32_t f = (back_q12 >> 4) & 255, a = line_read(l, b), c = line_read(l, b + 1u);
    return a + (((c - a) * f) >> 8);
}
/* the width of a stereo pair: 0 = mono, 127 = as it is (mid / side) */
static inline void widen(int32_t *l, int32_t *r, int32_t w127)
{
    int32_t m = (*l + *r) >> 1, s = (((*l - *r) >> 1) * (w127 * 129)) >> 14;   /* (x w / 127 without a division) */
    *l = m + s;
    *r = m - s;
}

/* ---- the chorus (a send too: one for the four tracks costs a quarter of one each, the user: "se il chorus costa
 * troppo ... lascia perdere"): one line, two taps swept in opposite directions, the return wet only */
static struct {
    uint8_t on;
    int32_t depth;
    uint32_t inc, ph;
    int32_t lp[2];
    line_t l;
} chs;

/* ---- the shared delay and reverb */
static line_t dl, dr, rv_pre, rv_in[4], rv_ap1, rv_d1, rv_ap2, rv_d2, rv_ap3, rv_d3, rv_ap4, rv_d4;
static uint32_t rv_ph1, rv_ph2, rv_odd;
static int32_t rv_hold, rv_lp_in, rv_lpl, rv_lpr, rv_yl, rv_yr;
static int32_t dly_cl, dly_cr;                  /* the read positions, Q12 samples, sliding to the targets like a tape */
static int32_t dly_lp[2], dly_hp[2];
/* the sends run only while something is sent or their tail still rings (fxs_process measures it): an idle delay /
 * reverb costs nothing. One switched off while ringing is dirty: it is cleared before it runs again, the delay's two
 * long lines a chunk a half (a whole clear at once is a spike of ~80000 writes in one interrupt) */
#define DLY_CLR_CHUNK 8192u
#define FX_SILENT 16                            /* ~-66 dBFS: a tail under it is over */
static uint32_t dl_dirty, rv_dirty, dl_clr = DLY_MAX, dl_live, rv_live;
static struct {
    uint8_t dl_on, dl_ping, rv_on;
    int32_t dl_fb, dl_tl, dl_tr, dl_klo, dl_khi, dl_width;
    int32_t rv_decay, rv_damp_k, rv_width, rv_g1, rv_g2;
    uint32_t rv_pre;
} fxs;
static volatile uint8_t ui_dl_live, ui_rv_live;  /* (the screen: a tail rings) */

static void fx_init(void)
{
    uint32_t t;
    (void)t;
    line_init(&chs.l, CHO_N);
    line_init(&dl, DLY_MAX); line_init(&dr, DLY_MAX);
    line_init(&rv_pre, REV_PRE_N);
    line_init(&rv_in[0], RV_IN1); line_init(&rv_in[1], RV_IN2); line_init(&rv_in[2], RV_IN3); line_init(&rv_in[3], RV_IN4);
    line_init(&rv_ap1, RV_AP1 + 16u); line_init(&rv_d1, RV_D1); line_init(&rv_ap2, RV_AP2); line_init(&rv_d2, RV_D2);
    line_init(&rv_ap3, RV_AP3 + 16u); line_init(&rv_d3, RV_D3); line_init(&rv_ap4, RV_AP4); line_init(&rv_d4, RV_D4);
    dly_cl = dly_cr = 11025 << 12;
}

/* the chorus, once per ISR call: pc the sound whose chorus it is (0: off; switched on, it starts from silence) */
static void chs_block(const patch_t *pc)
{
    if (pc && !chs.on) { line_clear(&chs.l); chs.lp[0] = chs.lp[1] = 0; }
    chs.on = pc != 0;
    if (!pc) return;
    chs.inc = CH_INC[pc->v[P_CH_RATE] & 127u];
    chs.depth = (int32_t)pc->v[P_CH_DEPTH] * (110 << 16) / 127 / 2048;   /* up to +-2.5 ms, per triangle step (Q16 samples) */
}

static void rev_clear(void)
{
    line_t *const L[] = {&rv_pre, &rv_in[0], &rv_in[1], &rv_in[2], &rv_in[3], &rv_ap1, &rv_d1, &rv_ap2, &rv_d2, &rv_ap3, &rv_d3, &rv_ap4, &rv_d4};
    uint32_t i;
    for (i = 0; i < sizeof L / sizeof L[0]; i++) line_clear(L[i]);
    rv_lp_in = rv_lpl = rv_lpr = rv_yl = rv_yr = rv_hold = 0;
}

/* the shared delay and reverb, once per ISR call: pd / pr the sound whose delay / reverb they are (0: off) */
static void fxs_block(const patch_t *pd, const patch_t *pr, uint32_t quarter)
{
    uint32_t t;
    int32_t d;
    if (pd && !fxs.dl_on && dl_dirty) { dl_clr = 0; dl_dirty = 0; dly_lp[0] = dly_lp[1] = dly_hp[0] = dly_hp[1] = 0; }
    if (pr && !fxs.rv_on && rv_dirty) { rev_clear(); rv_dirty = 0; }
    fxs.dl_on = pd != 0;
    fxs.rv_on = pr != 0;
    if (pd) {
        const uint8_t *v = pd->v;
        fxs.dl_ping = v[P_DL_PING];
        fxs.dl_fb = v[P_DL_FB] * 250;           /* Q15, up to 0.97 */
        t = div_frames(quarter, 18u - v[P_DL_TIMEL] % NDLYDIV); if (t > DLY_MAX - 4u) t = DLY_MAX - 4u; if (t < 32u) t = 32u;
        fxs.dl_tl = (int32_t)(t << 12);
        t = div_frames(quarter, 18u - v[P_DL_TIMER] % NDLYDIV); if (t > DLY_MAX - 4u) t = DLY_MAX - 4u; if (t < 32u) t = 32u;
        fxs.dl_tr = (int32_t)(t << 12);
        fxs.dl_klo = DLC_K[v[P_DL_LOW] & 127u]; fxs.dl_khi = DHC_K[v[P_DL_HIGH] & 127u];
        fxs.dl_width = v[P_DL_WIDTH];
    }
    if (pr) {
        const uint8_t *v = pr->v;
        d = v[P_RV_DECAY]; fxs.rv_decay = 12000 + ((d * d * 19500) >> 14);   /* 0.37 .. 0.96 */
        d = v[P_RV_DAMP]; fxs.rv_damp_k = clampi(32768 - d * d * 2, 64, 32767);   /* (the one-pole at the half rate) */
        fxs.rv_pre = 1u + (uint32_t)v[P_RV_PRE] * (REV_PRE_N - 2u) / 127u;
        fxs.rv_width = v[P_RV_WIDTH];
        d = v[P_RV_DIFF];
        fxs.rv_g1 = 9830 + d * 117;             /* the input diffusers: 0.30 .. 0.75 (the paper's 0.75 at 127) */
        fxs.rv_g2 = 8192 + d * 97;              /* 0.25 .. 0.625 */
    }
}

/* one reverb sample: in -> pre-delay -> bandwidth -> 4 diffusers -> the two tank halves */
static inline void rev_tick(int32_t in)
{
    int32_t x, a, b, e1 = tri_q(rv_ph1), e2 = tri_q(rv_ph2), dec = fxs.rv_decay, dk = fxs.rv_damp_k;
    rv_ph1 += 2u * 97391u;                      /* ~1.0 Hz and ~0.7 Hz: the tank's sway */
    rv_ph2 += 2u * 68174u;
    line_write(&rv_pre, in);
    x = line_read(&rv_pre, fxs.rv_pre);
    rv_lp_in += ((x - rv_lp_in) * 24000) >> 15;   /* (~5 kHz at the half rate) */
    x = ap_step(&rv_in[0], rv_lp_in, fxs.rv_g1);
    x = ap_step(&rv_in[1], x, fxs.rv_g1);
    x = ap_step(&rv_in[2], x, fxs.rv_g2);
    x = ap_step(&rv_in[3], x, fxs.rv_g2);
    a = x + ((line_read(&rv_d4, RV_D4) * dec) >> 15);
    a = ap_mod(&rv_ap1, a, -22938, RV_AP1, e1);
    line_write(&rv_d1, a);
    b = line_read(&rv_d1, RV_D1);
    rv_lpl += ((b - rv_lpl) * dk) >> 15;
    b = ap_step(&rv_ap2, (rv_lpl * dec) >> 15, 16384);
    line_write(&rv_d2, b);
    a = x + ((line_read(&rv_d2, RV_D2) * dec) >> 15);
    a = ap_mod(&rv_ap3, a, -22938, RV_AP3, e2);
    line_write(&rv_d3, a);
    b = line_read(&rv_d3, RV_D3);
    rv_lpr += ((b - rv_lpr) * dk) >> 15;
    b = ap_step(&rv_ap4, (rv_lpr * dec) >> 15, 16384);
    line_write(&rv_d4, b);
    a = line_read(&rv_d3, RVL(266)) + line_read(&rv_d3, RVL(2974)) - line_read(&rv_ap4, RVL(1913)) + line_read(&rv_d4, RVL(1996))
      - line_read(&rv_d1, RVL(1990)) - line_read(&rv_ap2, RVL(187)) - line_read(&rv_d2, RVL(1066));
    b = line_read(&rv_d1, RVL(353)) + line_read(&rv_d1, RVL(3627)) - line_read(&rv_ap2, RVL(1228)) + line_read(&rv_d2, RVL(2673))
      - line_read(&rv_d3, RVL(2111)) - line_read(&rv_ap4, RVL(335)) - line_read(&rv_d4, RVL(121));
    rv_yl = ((a >> 2) * 19661) >> 13;          /* (x 0.6; seven taps can reach +-229000) */
    rv_yr = ((b >> 2) * 19661) >> 13;
}

/* the shared sends: cs the chorus's mono send, ds the delay's stereo one, rs the reverb's mono one; their returns are
 * added to out */
static void fxs_process(int32_t *out, const int32_t *cs, const int32_t *ds, const int32_t *rs, uint32_t n)
{
    uint32_t i, i0;
    int32_t pk = 0;
    if (chs.on)                                 /* CHORUS: the wet, darkened as a bucket brigade (~9 kHz) */
        for (i = 0; i < n; i++) {
            int32_t tq = tri_q(chs.ph) * chs.depth, wl, wr;
            chs.ph += chs.inc;
            wl = line_read_q16(&chs.l, (154 << 16) + tq);
            wr = line_read_q16(&chs.l, (154 << 16) - tq);
            line_write(&chs.l, cs[i]);
            chs.lp[0] += ((wl - chs.lp[0]) * 23000) >> 15;
            chs.lp[1] += ((wr - chs.lp[1]) * 23000) >> 15;
            out[2u * i] += chs.lp[0];
            out[2u * i + 1u] += chs.lp[1];
        }
    if (fxs.dl_on && dl_clr < DLY_MAX) {        /* (still clearing: a chunk this half, the delay silent meanwhile) */
        for (i0 = dl_clr; i0 < dl_clr + DLY_CLR_CHUNK && i0 < DLY_MAX; i0++) dl.b[i0] = dr.b[i0] = 0;
        dl_clr = i0;
        dl_live = 1;
    } else if (fxs.dl_on) {                     /* DELAY: L / R note values, ping-pong or stereo, low / high cut in the feedback */
        for (i = 0; i < n; i++) {
            int32_t wl, wr, fl, fr, inl = ds[2u * i], inr = ds[2u * i + 1u];
            if (dly_cl < fxs.dl_tl) { dly_cl += 1 << 10; if (dly_cl > fxs.dl_tl) dly_cl = fxs.dl_tl; }
            else if (dly_cl > fxs.dl_tl) { dly_cl -= 1 << 10; if (dly_cl < fxs.dl_tl) dly_cl = fxs.dl_tl; }
            if (dly_cr < fxs.dl_tr) { dly_cr += 1 << 10; if (dly_cr > fxs.dl_tr) dly_cr = fxs.dl_tr; }
            else if (dly_cr > fxs.dl_tr) { dly_cr -= 1 << 10; if (dly_cr < fxs.dl_tr) dly_cr = fxs.dl_tr; }
            wl = line_read_q12(&dl, dly_cl);
            wr = line_read_q12(&dr, dly_cr);
            dly_lp[0] += (((wl - dly_lp[0]) >> 1) * fxs.dl_khi) >> 14;
            dly_lp[1] += (((wr - dly_lp[1]) >> 1) * fxs.dl_khi) >> 14;
            dly_hp[0] += (((dly_lp[0] - dly_hp[0]) >> 1) * fxs.dl_klo) >> 14;
            dly_hp[1] += (((dly_lp[1] - dly_hp[1]) >> 1) * fxs.dl_klo) >> 14;
            fl = ((dly_lp[0] - dly_hp[0]) * fxs.dl_fb) >> 15;
            fr = ((dly_lp[1] - dly_hp[1]) * fxs.dl_fb) >> 15;
            if (fxs.dl_ping) { line_write(&dl, ((inl + inr) >> 1) + fr); line_write(&dr, fl); }   /* in on the left, the repeats cross */
            else { line_write(&dl, inl + fl); line_write(&dr, inr + fr); }
            widen(&wl, &wr, fxs.dl_width);
            out[2u * i] += clampi(wl, -65535, 65535);
            out[2u * i + 1u] += clampi(wr, -65535, 65535);
            if (wl > pk) pk = wl; else if (-wl > pk) pk = -wl;
        }
        dl_live = pk > FX_SILENT;
    }
    pk = 0;
    if (fxs.rv_on) {                            /* REVERB: a tick every other sample, the output between ticks */
        for (i = 0; i < n; i++) {
            int32_t yl, yr, in = sat16(rs[i]);
            if (!(rv_odd ^= 1u)) {
                int32_t pl = rv_yl, pr = rv_yr;
                rev_tick((rv_hold + in) >> 1);
                yl = (pl + rv_yl) >> 1; yr = (pr + rv_yr) >> 1;
            } else { yl = rv_yl; yr = rv_yr; }
            rv_hold = in;
            widen(&yl, &yr, fxs.rv_width);
            out[2u * i] += clampi(yl, -65535, 65535) * 2;
            out[2u * i + 1u] += clampi(yr, -65535, 65535) * 2;
            if (yl > pk) pk = yl; else if (-yl > pk) pk = -yl;
        }
        rv_live = pk > FX_SILENT / 2;
    }
    ui_dl_live = (uint8_t)(fxs.dl_on && dl_live);
    ui_rv_live = (uint8_t)(fxs.rv_on && rv_live);
}
