/* SPDX-License-Identifier: GPL-3.0-only */
/* The graph screen's pictures (y 142..203, mfm_ui.h's g_begin / g_end): what the page's knobs do, drawn from the sound
 * itself: the VCO's wave as WAVE and PULSE WIDTH make it, the modulation mix, the ladder's response (moving with the
 * EG), the EG with the stage that plays, the LFO with its running dot, the S+H's last steps, the folder, the utilities,
 * the cables of the page's jacks, the keyboard's scale, the arpeggio, the tracks. */
static int g_live(uint32_t g)                   /* drawn again while it plays (or always: the LFO, the S+H, the tracks) */
{
    return g == G_SCOPE || g == G_VCF || g == G_EG || g == G_VCA || g == G_LFO || g == G_SH || g == G_VMOD || g == G_TRACKS
        || g == G_DLY || g == G_REV || g == G_CLK;
}
static int32_t g_y(int32_t v, int32_t top, int32_t h)   /* a value +-32768 -> y in Q8 within [top, top + h] */
{
    return (top * 256 + h * 128) - clampi(v, -32768, 32767) * h / 256;
}
static void g_label(int32_t x, int32_t y, const char *s, uint16_t c) { aa_text(x, y, &FA8, s, blend565(C_SCREEN, c, 170)); }
static void g_label_r(const char *s, uint16_t c) { aa_text_r(GX0 + GW - 2, GY0, &FA8, s, blend565(C_SCREEN, c, 200)); }

/* the waves as the engine makes them (no band-limiting: a picture) */
static int32_t vco_wave(uint32_t ph, uint32_t duty, int32_t w)
{
    int32_t saw = (int32_t)(ph >> 16) - 32768, pls = ph < duty ? 32767 : -32768;
    return ((saw * (32768 - w)) >> 15) + ((pls * w) >> 15);
}
static int32_t lfo_wave(uint32_t ph, int32_t w)
{
    int32_t p = (int32_t)((ph + 0x40000000u) >> 15), tri = p < 65536 ? p - 32768 : 98303 - p, sq = ph < 0x80000000u ? 30000 : -30000;
    return ((tri * (32768 - w)) >> 15) + ((sq * w) >> 15);
}
static uint32_t duty_of(const uint8_t *pv) { return (uint32_t)(DUTY_MIN16 + pv[P_PW] * DUTY_SPAN16 / 127) << 16; }

/* ---- the VCO: two cycles of its wave */
static void g_vco(const uint8_t *pv, uint16_t c)
{
    static int32_t yq[216];
    char s[24];
    int32_t x, top = GY0 + 9, h = 40, w = lin_q15(pv[P_WAVE]);
    uint32_t duty = duty_of(pv);
    g_begin();
    for (x = 0; x < 216; x++) yq[x] = g_y((vco_wave((uint32_t)(((uint64_t)x * 2u << 32) / 216u), duty, w) * 7) >> 3, top, h);
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    g_curve(yq, 12, 216, c, top + h / 2);
    s[0] = 0; mix_name(s, pv[P_WAVE], "SAW", "PULSE");
    if (pv[P_WAVE] > 2) { scat(s, "  PW "); fmt_u(s + slen(s), (uint32_t)(5 + pv[P_PW] * 45 / 127)); scat(s, "%"); }
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0; scat(s, "PITCH "); param_value(s + slen(s), P_PITCH, pv[P_PITCH]);
    g_label_r(s, c);
    g_end();
}

/* ---- the EG's picture (the knobs' contour), shared by the EG, the VCA and the modulation mix */
static int32_t env_shape(const uint16_t *t, int32_t x, int32_t w)   /* a 33-point shape at x of w, Q15 */
{
    int32_t q = w > 0 ? x * 32 * 256 / w : 32 * 256, i = clampi(q >> 8, 0, 31), f = q & 255;
    if (q >= 32 * 256) return t[32];
    return t[i] + (((t[i + 1] - t[i]) * f) >> 8);
}
static void env_bounds(const uint8_t *pv, int32_t *bx)   /* the stages' x over 216 */
{
    int32_t wa = 4 + pv[P_ATTACK] * 44 / 127, wd = 4 + pv[P_DECAY] * 44 / 127, wr = 4 + pv[P_RELEASE] * 44 / 127;
    bx[0] = 0; bx[1] = wa; bx[2] = wa + wd; bx[3] = 216 - wr; bx[4] = 216;
}
static int32_t env_at(const uint8_t *pv, const int32_t *bx, int32_t x)   /* the contour at x, Q15 */
{
    int32_t s = pv[P_SUSTAIN] * 258;
    if (x < bx[1]) return env_shape(ENVG_ATT, x, bx[1]);
    if (x < bx[2]) return s + (((32767 - s) * env_shape(ENVG_DEC, x - bx[1], bx[2] - bx[1])) >> 15);
    if (x < bx[3]) return s;
    return (s * env_shape(ENVG_DEC, x - bx[3], bx[4] - bx[3])) >> 15;
}
static void g_envelope(const uint8_t *pv, uint32_t ti, uint16_t c, int flat)
{
    static int32_t yq[216];
    int32_t bx[5], x, top = GY0 + 9, h = 36;
    uint32_t st = ui_estage[ti % NTRK];
    int playing = ui_note[ti % NTRK] != 255u;
    env_bounds(pv, bx);
    for (x = 0; x < 216; x++) yq[x] = top * 256 + (32767 - clampi(flat ? 32767 : env_at(pv, bx, x), 0, 32767)) * h / 128;
    if (!flat && playing && st >= E_ATT && st <= E_REL) {   /* the stage that plays, lit (the decay runs into the sustain) */
        int32_t a = st == E_ATT ? 0 : st == E_DEC ? bx[1] : bx[3], b = st == E_ATT ? bx[1] : st == E_DEC ? bx[3] : bx[4];
        cv_fill_a(12 + a, top, b - a, h + 1, c, 34);
    }
    if (!flat) for (x = 1; x < 4; x++) cv_rect(12 + bx[x], top, 1, h + 1, C_GRID2);
    g_curve(yq, 12, 216, c, top + h);
    if (!flat) {
        static const char *const SEG[4] = {"A", "D", "S", "R"};
        for (x = 0; x < 4; x++)
            if (bx[x + 1] - bx[x] >= 6) aa_text_c(12 + (bx[x] + bx[x + 1]) / 2, top + h + 2, &FA8, SEG[x], blend565(C_SCREEN, c, 180));
    }
    if (playing) {                              /* the level now: a line across */
        int32_t lv = flat ? ui_amp[ti % NTRK] : ui_env[ti % NTRK], ly = top + (32767 - clampi(lv, 0, 32767)) * h / 32767;
        for (x = 12; x < 228; x += 4) cv_mix(x, ly, C_TEXT, 170);
    }
}
static void g_env(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[24];
    g_begin();
    g_envelope(pv, ti, c, 0);
    g_label(GX0 + 2, GY0, pv[PJ(I_GATE)] ? "GATE: KEYS + CABLE" : "GATE: THE KEYS", c);
    s[0] = 0; scat(s, "0 .. 8 V");
    g_label_r(s, c);
    g_end();
}
static void g_vca(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[32];
    g_begin();
    g_envelope(pv, ti, c, pv[P_VCA_MODE] == VCA_ON);
    s[0] = 0; scat(s, pv[P_VCA_MODE] == VCA_ON ? "MODE ON: ALWAYS OPEN" : "MODE EG: THE EG OPENS IT");
    g_label(GX0 + 2, GY0, s, c);
    if (pv[PJ(I_VCACV)]) { s[0] = 0; scat(s, "+ "); scat(s, JSRC_NAME[pv[PJ(I_VCACV)] % NJSRC]); g_label_r(s, c); }
    g_end();
}

/* ---- the modulation mix (VCO MOD, VCF MOD): the EG and the LFO faint, their crossfade bright */
static void g_mix_picture(const uint8_t *pv, uint32_t ti, uint32_t mixp, uint16_t c)
{
    static int32_t yq[216], ye[216], yl[216];
    int32_t bx[5], x, top = GY0 + 9, h = 40, m = lin_q15(pv[mixp]), w = lin_q15(pv[P_LFO_WAVE]);
    env_bounds(pv, bx);
    for (x = 0; x < 216; x++) {
        int32_t e = env_at(pv, bx, x) * 2 - 32768, l = lfo_wave((uint32_t)(((uint64_t)x * 3u << 32) / 216u), w);
        ye[x] = g_y(e, top, h); yl[x] = g_y(l, top, h);
        yq[x] = g_y(e + (((l - e) * (m >> 1)) >> 14), top, h);
    }
    for (x = 1; x < 216; x++) {
        cv_wu((11 + x) * 256, ye[x - 1], (12 + x) * 256, ye[x], blend565(C_SCREEN, C_ENV, 70));
        cv_wu((11 + x) * 256, yl[x - 1], (12 + x) * 256, yl[x], blend565(C_SCREEN, C_LFO, 70));
    }
    g_curve(yq, 12, 216, c, -1);
    {                                           /* the LFO's dot, where it is now (its first cycle) */
        uint32_t ph = trk[ti % NTRK].lfo.ph;
        int32_t px = 12 + (int32_t)(ph >> 24) * 72 / 256, py = g_y(lfo_wave(ph, w), top, h) >> 8;
        cv_disc_g(px, py, 2, C_TEXT, C_LFO);
    }
}
static void g_vmod(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[32];
    g_begin();
    g_mix_picture(pv, ti, P_VCO_MIX, c);
    s[0] = 0; mix_name(s, pv[P_VCO_MIX], "EG", "LFO");
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0; scat(s, "PITCH "); param_value(s + slen(s), P_PITCH_AMT, pv[P_PITCH_AMT]);
    scat(s, "  PWM "); param_value(s + slen(s), P_PWM_AMT, pv[P_PWM_AMT]);
    g_label_r(s, c);
    g_end();
}

/* ---- the VCF: the ladder's response at its resonance, shifted to the cutoff (log axis 20 Hz .. 20 kHz) */
static int32_t fr_db2(int32_t rq8, int32_t iq8)   /* half-dB, interpolated in the resonance and the frequency */
{
    int32_t r = clampi(rq8 >> 8, 0, FR_NROWS - 2), rf = clampi(rq8 - r * 256, 0, 256), i = clampi(iq8 >> 8, 0, FR_N - 2),
            f = clampi(iq8 - i * 256, 0, 256);
    const int8_t *a = FR_DB + (uint32_t)r * FR_N, *b = a + FR_N;
    int32_t va = a[i] + (((a[i + 1] - a[i]) * f) >> 8), vb = b[i] + (((b[i + 1] - b[i]) * f) >> 8);
    return va + (((vb - va) * rf) >> 8);
}
static int32_t res_row(uint32_t res) { return res <= 112u ? (int32_t)res * 16 : 7 * 256 + ((int32_t)res - 112) * 256 / 15; }
static void g_filter(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    static int32_t yq[220];
    char s[64];                                 /* (0.3.2: room for "+ ONE+TWO  IN: FOLDER + NOISE") */
    int32_t knob8 = CUT_LO8 + pv[P_CUTOFF] * 16 * (CUT_SPAN8 >> 3) / 254, cut8 = knob8, x, rq = res_row(pv[P_RES]);
    int playing = ui_note[ti % NTRK] != 255u;
    if (playing) cut8 = ui_cut8[ti % NTRK];
    g_begin();
    {                                           /* the frequencies' grid: 100 Hz, 1 kHz, 10 kHz; 0 dB */
        static const int16_t N8[3] = {11098, 21302, 31508};   /* (MIDI 43.35, 83.21, 123.08 x 256) */
        static const char *const NM[3] = {"100", "1k", "10k"};
        uint32_t k;
        for (k = 0; k < 3u; k++) {
            int32_t px = (N8[k] - 3968) * 220 / 30720;
            cv_rect(GX0 + px, GY0, 1, GH, C_GRID2);
            aa_text(GX0 + px + 2, GY0 + GH - 9, &FA8, NM[k], C_GRID3);
        }
        cv_rect(GX0, GY0 + 16, GW, 1, C_GRID2);   /* (0 dB) */
    }
    for (x = 0; x < 220; x++) {
        int32_t n8 = 3968 + x * 30720 / 220, db = fr_db2(rq, 60 * 256 + (n8 - cut8) / 2);
        yq[x] = GY0 * 256 + 16 * 256 - clampi(db, -120, 60) * 40 * 256 / 120;   /* (+12 dB at the top, -48 dB at the bottom) */
    }
    g_curve(yq, GX0, 220, c, GY0 + GH - 1);
    {                                           /* the cutoff: the knob's mark, the live one's line */
        int32_t kx = GX0 + (knob8 - 3968) * 220 / 30720, lx = GX0 + (cut8 - 3968) * 220 / 30720, y;
        if (kx >= GX0 && kx < GX0 + GW) { cv_rect(kx - 2, GY0, 5, 1, C_TEXT); cv_rect(kx - 1, GY0 + 1, 3, 1, C_TEXT); cv_mix(kx, GY0 + 2, C_TEXT, 256); }
        if (playing && lx >= GX0 && lx < GX0 + GW)
            for (y = GY0 + 4; y < GY0 + GH; y += 3) cv_mix(lx, y, C_TEXT, 150);
    }
    s[0] = 0; scat(s, "LADDER 24 dB");
    if (pv[PJ(I_CUTOFF)]) { scat(s, "  + "); scat(s, JSRC_NAME[pv[PJ(I_CUTOFF)] % NJSRC]); }
    if (pv[PJ(I_FOLD)]) scat(s, "  IN: FOLDER");
    if (pv[P_NOISE]) scat(s, " + NOISE");
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0; fmt_hz(s, cut_hz(pv[P_CUTOFF])); scat(s, " Hz");
    g_label_r(s, c);
    g_end();
}

/* ---- the LFO: two cycles of the wave, the running dot */
static void g_lfo(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    static int32_t yq[216];
    char s[32];
    int32_t x, top = GY0 + 9, h = 40, w = lin_q15(pv[P_LFO_WAVE]);
    g_begin();
    for (x = 0; x < 216; x++) yq[x] = g_y(lfo_wave((uint32_t)(((uint64_t)x * 2u << 32) / 216u), w), top, h);
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    g_curve(yq, 12, 216, c, top + h / 2);
    {
        uint32_t ph = trk[ti % NTRK].lfo.ph;
        int32_t px = 12 + (int32_t)(ph >> 24) * 108 / 256, py = g_y(lfo_wave(ph, w), top, h) >> 8;
        cv_disc_g(px, py, 4, blend565(c, C_SCREEN, 120), blend565(c, C_SCREEN, 160));
        cv_disc_g(px, py, 2, C_TEXT, c);
    }
    s[0] = 0; mix_name(s, pv[P_LFO_WAVE], "TRIANGLE", "SQUARE");
    if (pv[PJ(I_LFORATE)]) { scat(s, "  + "); scat(s, JSRC_NAME[pv[PJ(I_LFORATE)] % NJSRC]); }
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0;
    if (pv[P_LFO_SYNC]) { scat(s, "SYNC "); scat(s, DIV_NAME[(pv[P_LFO_SYNC] - 1u) % NCLKDIV]); }
    else fmt_lhz(s, LFO_HZ_X100[pv[P_LFO_RATE] & 127u]);
    g_label_r(s, c);
    g_end();
}

/* ---- the S+H: its last sixteen steps (+-5 V) */
static void g_sh(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[40];
    uint32_t i, n = ui_sh_n[ti % NTRK];
    int32_t top = GY0 + 10, h = 42;
    g_begin();
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    for (i = 0; i < 16u; i++) {
        int32_t v = ui_sh_hist[ti % NTRK][(n + i) & 15u], y = g_y(clampi(v * 6, -32767, 32767), top, h) >> 8, x = 12 + 13 * (int32_t)i;
        int32_t y0 = top + h / 2;
        cv_fill_a(x + 1, y < y0 ? y : y0, 11, (y < y0 ? y0 - y : y - y0) + 1, c, i == 15u ? 120 : 50);
        cv_rect(x, y, 13, 2, i == 15u ? C_TEXT : c);
    }
    s[0] = 0;
    scat(s, pv[PJ(I_SHIN)] ? JSRC_NAME[pv[PJ(I_SHIN)] % NJSRC] : "NOISE");
    scat(s, " SAMPLED BY ");
    scat(s, pv[PJ(I_SHGATE)] ? JSRC_NAME[pv[PJ(I_SHGATE)] % NJSRC] : "LFO");
    g_label(GX0 + 2, GY0, s, c);
    g_end();
}

/* ---- the wave folder: a sine of 5 V through it (faint) and folded (bright) */
static void g_fold(const uint8_t *pv, uint16_t c)
{
    static int32_t yq[216], yi[216];
    char s[40];
    int32_t x, top = GY0 + 9, h = 40, g = 4096 + (int32_t)pv[P_FOLD] * 161, on = pv[PJ(I_FOLD)] != J_NONE;
    g_begin();
    for (x = 0; x < 216; x++) {
        int32_t sn = sine_q15((uint32_t)(((uint64_t)x * 2u << 32) / 216u)), u = (sn * 5 * VOLT) >> 15;
        yi[x] = g_y(sn, top, h);
        yq[x] = g_y((fold1((u * g) >> 12) * 32767) / (5 * VOLT), top, h);
    }
    for (x = 1; x < 216; x++) cv_wu((11 + x) * 256, yi[x - 1], (12 + x) * 256, yi[x], blend565(C_SCREEN, c, 60));
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    g_curve(yq, 12, 216, on ? c : blend565(C_SCREEN, c, 100), top + h / 2);
    s[0] = 0;
    if (on) { scat(s, "FOLD IN: "); scat(s, JSRC_NAME[pv[PJ(I_FOLD)] % NJSRC]); scat(s, "  > VCF"); }
    else scat(s, "NO CABLE IN FOLD IN: NOT IN USE");
    g_label(GX0 + 2, GY0, s, on ? c : C_DIMT);
    s[0] = 0; scat(s, "x"); fmt_frac(s + 1, g * 10 / 4096, 1, 0);
    g_label_r(s, c);
    g_end();
}

/* ---- the utilities: a box a module, the cables in, the knob's share */
static void g_box(int32_t x, int32_t y, int32_t w, const char *s, uint16_t c, int lit)
{
    cv_round(x, y, w, 13, lit ? blend565(C_SCREEN, c, 80) : RGB(22, 24, 28));
    aa_text_c(x + w / 2, y + 2, &FA8, s, lit ? C_TEXT : C_DIMT);
}
static void g_arrow(int32_t x0, int32_t x1, int32_t y, uint16_t c)
{
    cv_wu(x0 * 256, y * 256, x1 * 256, y * 256, c);
    cv_wu((x1 - 3) * 256, (y - 2) * 256, x1 * 256, y * 256, c);
    cv_wu((x1 - 3) * 256, (y + 2) * 256, x1 * 256, y * 256, c);
}
static void g_volts(char *s, int32_t v)         /* volts Q12 -> "+3.1 V" */
{
    fmt_frac(s, v * 10 / VOLT, 1, 1); scat(s, " V");
}
static void g_umix(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[24];
    int32_t one = pv[P_ONE_LVL] * 100 / 127;
    g_begin();
    g_box(14, GY0 + 10, 62, pv[PJ(I_ONE)] ? JSRC_NAME[pv[PJ(I_ONE)] % NJSRC] : "-5 V", c, 1);
    g_box(14, GY0 + 32, 62, pv[PJ(I_TWO)] ? JSRC_NAME[pv[PJ(I_TWO)] % NJSRC] : "---", c, pv[PJ(I_TWO)] != 0);
    cv_round(84, GY0 + 12, 40, 9, RGB(22, 24, 28));
    cv_round(84, GY0 + 12, 2 + one * 38 / 100, 9, c);
    g_arrow(126, 150, GY0 + 16, C_DIMT);
    g_arrow(78, 150, GY0 + 38, C_DIMT);
    g_box(152, GY0 + 21, 72, "ONE+TWO", c, 1);
    s[0] = 0; g_volts(s, trk[ti % NTRK].o[J_ONETWO]);
    aa_text_c(188, GY0 + 37, &FA8, s, blend565(C_SCREEN, c, 200));
    g_label(GX0 + 2, GY0, "ONE x ONE LVL + TWO", c);
    g_end();
}
static void g_attn(const uint8_t *pv, uint32_t ti, uint16_t c)
{
    char s[24];
    int32_t a = pv[P_ATTN] * 100 / 127;
    g_begin();
    g_box(14, GY0 + 10, 62, pv[PJ(I_ATTN)] ? JSRC_NAME[pv[PJ(I_ATTN)] % NJSRC] : "+5 V", c, 1);
    cv_round(84, GY0 + 12, 40, 9, RGB(22, 24, 28));
    cv_round(84, GY0 + 12, 2 + a * 38 / 100, 9, c);
    g_arrow(126, 150, GY0 + 16, C_DIMT);
    s[0] = 0; scat(s, "ATTN "); g_volts(s + slen(s), trk[ti % NTRK].o[J_ATTN]);
    g_box(152, GY0 + 10, 72, s, c, 1);
    g_label(GX0 + 2, GY0, "ATTENUATOR", c);
    g_end();
}

/* ---- the patchbay (0.3): SEQ's first page, the whole of it laid out as the panel's (3 columns x 7 rows, the mult
 * gone; a label over each jack: the inputs in plain print, the outputs in reverse), every cable drawn from its output
 * to its input, hanging (and swinging when just patched: swing, px). VCA OUT is the audio out: its cable goes off to
 * the headphones. A MIDI source's cable (VELO, MODWHL, PRESS, CLOCK) comes in from outside the panel, from above; a
 * DUMMY is a plug on its own, its cable hanging loose. y 48..239 */
static const uint16_t CABLE_C[NJSRC] = {RGB(60, 60, 60), RGB(240, 240, 236), RGB(96, 172, 255), RGB(255, 150, 56),
    RGB(186, 146, 255), RGB(118, 222, 128), RGB(255, 214, 70), RGB(255, 110, 150), RGB(255, 110, 150), RGB(84, 208, 226),
    RGB(255, 88, 76), RGB(255, 88, 76), RGB(200, 200, 120), RGB(200, 200, 120), RGB(200, 200, 120), RGB(250, 250, 200),
    RGB(130, 130, 136), RGB(214, 226, 255)};
typedef struct { uint8_t out, id; const char *lbl; } pbcell_t;   /* out: an output (id J_*), else an input (id I_*) */
#define PB_ROWS 7u
static const pbcell_t PB[PB_ROWS][3] = {
    {{1, J_VCA, "VCA OUT"}, {1, J_KBCV, "KB CV"}, {0, I_FOLD, "FOLD IN"}},
    {{0, I_1VOCT, "1V/OCT"}, {0, I_PWM, "PWM"}, {0, I_ONE, "ONE (-5)"}},
    {{0, I_LFORATE, "LFO RATE"}, {0, I_CUTOFF, "CUTOFF"}, {1, J_ONE, "ONE"}},
    {{0, I_GATE, "GATE"}, {0, I_VCACV, "VCA CV"}, {0, I_TWO, "TWO"}},
    {{1, J_VCO, "VCO"}, {0, I_SHIN, "S+H (NOISE)"}, {1, J_ONETWO, "ONE+TWO"}},
    {{1, J_LFO, "LFO"}, {0, I_SHGATE, "S+H GATE"}, {0, I_ATTN, "ATTN (+5)"}},
    {{1, J_EG, "EG"}, {1, J_SH, "S+H"}, {1, J_ATTN, "ATTN"}},
};
#define PB_X(c) (50 + 64 * (c))                 /* a jack's centre (canvas: y 0 = screen 48) */
#define PB_Y(r) (44 + 22 * (r))
static int pb_where(uint32_t out, uint32_t id, int32_t *x, int32_t *y)   /* a jack's place (0: not on the panel) */
{
    uint32_t r, c;
    for (r = 0; r < PB_ROWS; r++)
        for (c = 0; c < 3u; c++)
            if (PB[r][c].out == out && PB[r][c].id == id) { *x = PB_X((int32_t)c); *y = PB_Y((int32_t)r); return 1; }
    return 0;
}
static void pb_cable(int32_t ox, int32_t oy, int32_t ix, int32_t iy, int32_t sag, uint16_t c)
{
    int32_t k, px = ox * 256, py = oy * 256, x0 = ox * 256, y0 = oy * 256, x1 = ix * 256, y1 = iy * 256;
    for (k = 1; k <= 18; k++) {                 /* a hanging cable: the chord plus a sag that peaks in the middle */
        int32_t uu = k * 256 / 18, nx = x0 + (x1 - x0) * uu / 256, ny = y0 + (y1 - y0) * uu / 256 + sag * 256 * 4 * uu * (256 - uu) / 65536;
        cv_wu(px, py + 384, nx, ny + 384, blend565(C_BG, C_SH, 200));
        cv_wu2(px, py, nx, ny, c);
        px = nx; py = ny;
    }
}
static void pb_plug(int32_t x, int32_t y, uint16_t c) { cv_disc_g(x, y, 4, blend565(c, C_INK, 70), blend565(c, C_SH, 100)); }
static void ui_patchbay(const uint8_t *pv, const int16_t *swing, const char *sub, const pmap_t *pm)
{
    uint32_t r, c, i;
    cv_begin(240, 192, C_BG);
    ui_box(192, "PATCH", C_JACK);
    page_title("PATCHBAY", sub, pm, 0, C_JACK);
    aa_text(22, 14, &FA8, "IN /", C_INK2);
    cv_round(40, 14, 22, 9, C_INK);
    aa_text(42, 14, &FA8, "OUT", C_SH);
    for (r = 0; r < PB_ROWS; r++)               /* the jacks and their labels (an output's in reverse print) */
        for (c = 0; c < 3u; c++) {
            const pbcell_t *e = &PB[r][c];
            int32_t x = PB_X((int32_t)c), y = PB_Y((int32_t)r), w = aa_wt(&FA8, e->lbl, -1) + 4;
            if (e->out) { cv_round(x - w / 2, y - 15, w, 9, C_INK); aa_textt(x - w / 2 + 2, y - 15, &FA8, e->lbl, C_SH, -1); }
            else aa_label_c(x, y - 15, &FA8, e->lbl, pv[PJ(e->id)] ? C_INK : C_INK2, 60);
            cv_jack(x, y, 5, 0, 0);
        }
    app_idle();
    {                                           /* VCA OUT: the audio out, its cable off the panel (the headphones) */
        int32_t x, y;
        pb_where(1, J_VCA, &x, &y);
        pb_cable(19, y + 6, x, y, 3, CABLE_C[J_VCA]);
        pb_plug(x, y, CABLE_C[J_VCA]);
    }
    for (i = 0; i < NJIN; i++) {                /* the cables, then their plugs */
        uint32_t s = pv[PJ(i)];
        int32_t ox, oy, ix, iy, d, sag;
        if (!s || !jack_ok(i, s) || !pb_where(0, i, &ix, &iy)) continue;
        if (s == J_DUMMY) {                     /* a plug on its own: its cable hangs loose, its other end free */
            pb_cable(ix, iy, ix + 9, iy + 13, 2 + swing[i] / 3, CABLE_C[s]);
            pb_plug(ix, iy, CABLE_C[s]);
            cv_round(ix + 7, iy + 12, 5, 5, blend565(CABLE_C[s], C_SH, 60));
            continue;
        }
        if (JS_EXT(s)) {                        /* from outside the panel (MIDI): in from above, no plug at that end */
            ox = ix - 14; oy = -10;
            pb_cable(ox, oy, ix, iy, 4 + swing[i] / 2, CABLE_C[s]);
            pb_plug(ix, iy, CABLE_C[s]);
            continue;
        }
        if (!pb_where(1, s, &ox, &oy)) continue;
        d = (ox > ix ? ox - ix : ix - ox) + (oy > iy ? oy - iy : iy - oy);
        sag = 6 + d / 4 + swing[i];
        pb_cable(ox, oy, ix, iy, sag, CABLE_C[s]);
        pb_plug(ox, oy, CABLE_C[s]);
        pb_plug(ix, iy, CABLE_C[s]);
    }
    cv_blit(0, 48);
}

/* ---- an INPUTS page's picture (0.3): its four inputs, each its knob's number, where its cable comes from (or its
 * normal), the cable, the input */
static void g_jacks(const uint8_t *pv, const int16_t *jp, int32_t focus)
{
    uint32_t k, n = 0;
    g_begin();
    for (k = 0; k < 4u; k++) if (jp[k] >= 0) n++;
    for (k = 0; k < 4u; k++) {
        int32_t in = jp[k], y = GY0 + 1 + (int32_t)k * 14, src;
        char d[2] = {(char)('1' + k), 0};
        uint16_t cc, tc;
        if (in < 0) continue;
        src = pv[PJ(in)] % NJSRC;
        cc = src ? CABLE_C[src] : RGB(70, 70, 74);
        tc = (int32_t)k == focus ? C_TEXT : blend565(C_SCREEN, C_TEXT, 200);
        cv_disc_g(GX0 + 7, y + 6, 5, (int32_t)k == focus ? C_ACC : blend565(C_SCREEN, C_ACC, 150), C_SCREEN);
        aa_text_c(GX0 + 7, y + 2, &FA8, d, C_SH);
        aa_text(GX0 + 17, y + 1, &FA10, src ? JSRC_NAME[src] : JIN_NORMAL[in % NJIN], src ? blend565(cc, C_TEXT, 60) : C_DIMT);
        if (src == J_DUMMY) {                   /* a dummy: the cable ends in the air */
            cv_rect(GX0 + 92, y + 6, 40, 2, cc);
            cv_round(GX0 + 88, y + 4, 6, 6, blend565(cc, C_SH, 60));
        } else if (src) {
            cv_rect(GX0 + 80, y + 6, 74, 2, cc);
            cv_rect(GX0 + 150, y + 4, 4, 6, cc);
        } else
            cv_rect(GX0 + 80, y + 7, 74, 1, RGB(50, 50, 54));
        aa_text(GX0 + 160, y + 1, &FA10, JIN_NAME[in % NJIN], tc);
    }
    if (!n) g_label(GX0 + 2, GY0, "THE WHOLE PATCHBAY: SEQ", C_DIMT);
    g_end();
}

/* ---- the clock (0.3): its note value's pulses over two bars, where the clock is now, the tempo and where it comes from */
static void g_clk(const uint8_t *pv, uint16_t c)
{
    char s[32];
    uint32_t tk = DIV_TICKS(pv[P_CLK_DIV] % NCLKDIV), wd = pv[P_CLK_WIDTH] + 1u, x, span = 192u;   /* two bars: 192 clocks */
    int32_t top = GY0 + 16, h = 26, now;
    g_begin();
    for (x = 0; x < 216u; x++) {                /* the pulses, 216 px = two bars */
        uint32_t ck = x * span / 216u, hi = (ck % tk) * 128u < tk * wd;
        cv_rect(12 + (int32_t)x, hi ? top : top + h, 1, 2, c);
        if (x && hi != ((((x - 1u) * span / 216u) % tk) * 128u < tk * wd)) cv_rect(12 + (int32_t)x, top, 1, h + 2, c);
    }
    for (x = 0; x <= 8u; x++) cv_rect(12 + (int32_t)(x * 27u), top + h + 4, 1, x % 4u ? 3 : 6, C_DIMT);   /* the beats */
    now = 12 + (int32_t)((clk_pos >> 20) % span * 216u / span);
    cv_rect(now, top - 3, 1, h + 8, C_TEXT);
    s[0] = 0; scat(s, DIV_NAME[pv[P_CLK_DIV] % NCLKDIV]); scat(s, "  GATE "); fmt_u(s + slen(s), wd * 100u / 128u); scat(s, "%");
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0;
    if (ctl_ext_quarter) { scat(s, ctl_clk_run ? "MIDI " : "MIDI STOP "); fmt_u(s + slen(s), (FS * 60u + quarter_len() / 2u) / quarter_len()); }
    else { scat(s, "TEMPO "); fmt_u(s + slen(s), ctl_tempo); }
    g_label_r(s, c);
    g_end();
}

/* ---- the effects (0.2) */
static void g_cho(const uint8_t *pv, uint16_t c)   /* the chorus: its two taps' delays sweeping, opposite */
{
    static int32_t yl[216], yr[216];
    char s[24];
    int32_t x, top = GY0 + 10, h = 38, dep = pv[P_CH_DEPTH] * 32767 / 127;
    uint32_t inc = (uint32_t)(((uint64_t)CH_HZ_X100[pv[P_CH_RATE] & 127u] * 0x100000000ull) / 100u / 216u * 2u);
    g_begin();
    for (x = 0; x < 216; x++) {
        int32_t tq = tri_q((uint32_t)x * (inc ? inc : 1u)) * 16;
        yl[x] = g_y(((tq * dep) >> 15) * 3 / 4, top, h);
        yr[x] = g_y(-((tq * dep) >> 15) * 3 / 4, top, h);
    }
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    g_curve(yr, 12, 216, blend565(C_SCREEN, c, 140), -1);
    g_curve(yl, 12, 216, c, -1);
    s[0] = 0; scat(s, pv[P_CH_ON] ? "CHORUS " : "CHORUS OFF "); scat(s, CTYPE_NAME[pv[P_CH_TYPE] % NCT]);
    g_label(GX0 + 2, GY0, s, pv[P_CH_ON] ? c : C_DIMT);
    s[0] = 0; scat(s, "L / R  "); fmt_lhz(s + slen(s), CH_HZ_X100[pv[P_CH_RATE] & 127u]);
    g_label_r(s, c);
    g_end();
}
static void g_dly(const uint8_t *pv, uint16_t c)   /* the delay: the repeats, left above and right below, fading */
{
    char s[32];
    uint32_t q = quarter_len(), tl = div_frames(q, 18u - pv[P_DL_TIMEL] % NDLYDIV), tr = div_frames(q, 18u - pv[P_DL_TIMER] % NDLYDIV);
    int32_t k, fb = pv[P_DL_FB] * 250, ga = 32767, gb = 32767, span = (int32_t)(FS * 2u), mid = GY0 + 28;
    g_begin();
    cv_rect(12, mid, 216, 1, C_GRID2);
    cv_rect(12, mid - 22, 2, 44, C_INK);
    for (k = 1; k <= 12; k++) {                 /* (ping-pong: the repeats alternate sides) */
        int32_t xl = 12 + (int32_t)((uint32_t)k * tl * 216u / (uint32_t)span), xr = 12 + (int32_t)((uint32_t)k * tr * 216u / (uint32_t)span);
        int32_t hl, hr;
        ga = (int32_t)(((int64_t)ga * fb) >> 15); gb = (int32_t)(((int64_t)gb * fb) >> 15);
        hl = ga * 22 / 32767; hr = gb * 22 / 32767;
        if (pv[P_DL_PING] && (k & 1)) { int32_t t = hl; hl = hr; hr = t; }
        if (xl < 228 && hl > 0) cv_rect(xl, mid - hl, 2, hl, pv[P_DL_ON] ? c : C_DIMT);
        if (xr < 228 && hr > 0) cv_rect(xr, mid + 1, 2, hr, pv[P_DL_ON] ? blend565(C_SCREEN, c, 160) : C_DIMT);
    }
    s[0] = 0; scat(s, pv[P_DL_ON] ? "DELAY " : "DELAY OFF "); scat(s, DIV_NAME[18u - pv[P_DL_TIMEL] % NDLYDIV]); scat(s, " / ");
    scat(s, DIV_NAME[18u - pv[P_DL_TIMER] % NDLYDIV]); if (pv[P_DL_PING]) scat(s, "  PING-PONG");
    g_label(GX0 + 2, GY0, s, pv[P_DL_ON] ? c : C_DIMT);
    if (ui_dl_live) g_label_r("RINGING", c);
    g_end();
}
static void g_rev(const uint8_t *pv, uint16_t c)   /* the reverb: the pre-delay, then the plate's dense tail */
{
    static int32_t yq[216];
    char s[24];
    uint32_t rnd = 0x5EEDu;
    int32_t x, pre = pv[P_RV_PRE] * 30 / 127, dec = 12000 + ((pv[P_RV_DECAY] * pv[P_RV_DECAY] * 19500) >> 14), env = 32767;
    int32_t top = GY0 + 8, h = 44;
    g_begin();
    for (x = 0; x < 216; x++) {
        int32_t v = 0;
        if (x >= pre) {
            v = (int32_t)(xorshift(&rnd) >> 17) - 16384;
            v = (v * (env >> 1)) >> 14;
            env = (int32_t)(((int64_t)env * (24000 + (dec >> 2) + (dec >> 4))) >> 15);
            if (env > 32767) env = 32767;
        }
        yq[x] = top * 256 + h * 128 - (v * h * 256 / 65536);
    }
    cv_rect(12, top + h / 2, 216, 1, C_GRID2);
    g_curve(yq, 12, 216, pv[P_RV_ON] ? c : C_DIMT, -1);
    s[0] = 0; scat(s, pv[P_RV_ON] ? "REVERB  PLATE" : "REVERB OFF");
    g_label(GX0 + 2, GY0, s, pv[P_RV_ON] ? c : C_DIMT);
    if (ui_rv_live) g_label_r("RINGING", c);
    g_end();
}

/* ---- the keyboard: its 13 keys (C .. C) and the pitch they give (KB SCALE: 1 .. 5 octaves an octave) */
static void g_kb(const uint8_t *pv, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    char s[40];
    int32_t i, scl = 10 + pv[P_KB_SCALE] * 40 / 127, xw = 0;
    g_begin();
    for (i = 0; i <= 12; i++) if (!BLACK[i % 12]) {   /* the white keys, then the black */
        cv_round(14 + 13 * xw, GY0 + 34, 12, 20, RGB(220, 220, 214));
        xw++;
    }
    for (i = 0, xw = 0; i <= 12; i++) {
        if (BLACK[i % 12]) cv_round(14 + 13 * xw - 4, GY0 + 34, 8, 12, RGB(20, 20, 22));
        else xw++;
    }
    for (i = 0; i <= 50; i += 10) cv_rect(120, GY0 + 52 - i, 104, 1, C_GRID2);   /* (an octave a line) */
    cv_wu(120 * 256, (GY0 + 52) * 256, 224 * 256, (GY0 + 52 - clampi(scl, 0, 50)) * 256, c);
    cv_wu(120 * 256, (GY0 + 51) * 256, 224 * 256, (GY0 + 51 - clampi(scl, 0, 50)) * 256, c);
    s[0] = 0; param_value(s, P_KB_SCALE, pv[P_KB_SCALE]); scat(s, " / OCT");
    aa_text_r(224, GY0 + 12, &FA8, s, blend565(C_SCREEN, c, 200));
    s[0] = 0; scat(s, "PRIO "); scat(s, PRIO_NAME[pv[P_PRIO] % NPR]); scat(s, pv[P_RETRIG] ? "  RETRIG ALWAYS" : "  LEGATO");
    g_label(GX0 + 2, GY0, s, c);
    s[0] = 0; scat(s, "GLIDE "); param_value(s + slen(s), P_GLIDE, pv[P_GLIDE]);
    aa_text(14, GY0 + 22, &FA8, s, blend565(C_SCREEN, c, 200));
    g_end();
}

/* ---- the arpeggio over a C major chord held: its steps on a little roll */
static void g_arp(const uint8_t *pv, uint16_t c)
{
    static const uint8_t CH[3] = {0, 4, 7};
    uint32_t mode = pv[P_ARP_MODE] % NAR, oct = pv[P_ARP_OCT] % 4u + 1u, total = 3u * oct, i, rnd = 0x9E3779B9u;
    int32_t gate = 3 + pv[P_ARP_GATE] * 9 / 127, top = 12 * (int32_t)oct - 5;   /* (the highest note: G of the last octave) */
    g_begin();
    for (i = 0; i < 16u; i++) {
        uint32_t k, L, q;
        int32_t note, y;
        switch (mode) {
        case AR_DOWN: k = total - 1u - i % total; break;
        case AR_UPDN: case AR_DNUP: L = total > 1u ? 2u * total - 2u : 1u; q = i % L; k = q < total ? q : L - q; if (mode == AR_DNUP) k = total - 1u - k; break;
        case AR_UPDN2: L = 2u * total; q = i % L; k = q < total ? q : L - 1u - q; break;
        case AR_RANDOM: k = xorshift(&rnd) % total; break;
        default: k = i % total; break;
        }
        note = CH[k % 3u] + 12 * (int32_t)(k / 3u);
        y = GY0 + 44 - note * 34 / top;
        cv_round(14 + 13 * (int32_t)i, y, gate, 4, mode ? c : C_GRID2);
    }
    g_label(GX0 + 2, GY0, mode ? ARP_NAME[mode] : "ARP OFF", c);
    g_label_r(DIV_NAME[ARP_DIV0 + pv[P_ARP_RATE] % NARPDIV], c);
    g_end();
}

/* ---- the tracks: their levels as they play */
static void g_tracks(uint32_t ti)
{
    static int32_t pk[4];
    uint32_t t;
    int32_t pitch = NTRK > 1u ? 52 : 100, x0 = NTRK > 1u ? 18 : 100;
    g_begin();
    for (t = 0; t < NTRK; t++) {
        int32_t p = (int32_t)ui_trk_peak[t], h, b = 0, lg, x = x0 + pitch * (int32_t)t;
        ui_trk_peak[t] = 0;
        pk[t] = p > pk[t] ? p : pk[t] - (pk[t] >> 3);
        while (b < 20 && (pk[t] >> b)) b++;     /* (a log meter: ~6 dB a bit, 60 dB over its height) */
        lg = b ? b * 256 + ((pk[t] << 8 >> (b - 1)) - 256) : 0;
        h = clampi((lg - 7 * 256) * 38 / (10 * 256), 0, 38);
        cv_round(x, GY0 + 2, 40, 41, RGB(22, 24, 28));
        for (b = 0; b < 38; b += 4) cv_rect(x + 4, GY0 + 41 - b, 32, 1, RGB(30, 33, 38));
        if (h) cv_round(x + 6, GY0 + 42 - h, 28, h, TRKC[t & 3u]);
        {
            char s[8] = {'T', 'R', 'A', 'C', 'K', ' ', (char)('1' + t), 0};
            aa_text_c(x + 20, GY0 + 45, &FA8, s, t == ti % NTRK ? C_TEXT : C_DIMT);
        }
    }
    g_end();
}
