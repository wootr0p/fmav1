/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen (0.2, the user: "un'interfaccia UI ... qualcosa di bello graficamente, con qualche
 * animazione"): our own drawing in the manner of a small black semi-modular panel, after the reference manual's panel
 * drawing (its page 14): white print on black, each module in a box with its name on a vertical tab at its left
 * (V C O, V C F ...), knobs with their scale (the big ones with a dense ring), the little signs under them (saw / pulse,
 * triangle / square, EG / LFO, (-) / (+)), the toggle of VCA MODE, the 13-button keyboard, the patchbay with its inputs
 * in plain and its outputs in reverse print. Nobody's logo or artwork copied; Inter Tight (OFL) for the text.
 * Animated: the red LFO LED (it flashes with the LFO, as the reference panel's does), the tracks' lamps (they glow with the sound
 * and fade), the keyboard's buttons (lit by the notes, fading), the cables (they swing when patched), the splash (the kit
 * put together: the nuts screwed on, a cable plugged, the LED lit).
 * 240 x 240:
 *   0..15    the name, the LFO LED, the four tracks
 *   16..47   the program: its number on an LCD in the track's colour, its name, the patch's state
 *   48..141  the page: the module's box, its tab, its title and page dots, four knobs
 *   142..203 the screen: what the knobs do
 *   204..239 the keyboard (the note that plays lit) and the page's hint, or a message
 * The PATCHBAY pages draw the whole patchbay over 62..239 (mfm_ui_patch). */
#include "mfm_fonts.h"

#define C_BG RGB(9, 9, 10)                      /* the panel */
#define C_BOX RGB(15, 15, 17)                   /* a module's box */
#define C_INK RGB(238, 236, 230)                /* the print */
#define C_INK2 RGB(150, 148, 144)               /* quieter print */
#define C_BOXL RGB(214, 212, 206)               /* the boxes' lines */
#define C_RULE RGB(54, 54, 58)
#define C_SH RGB(0, 0, 0)
#define C_ACC RGB(44, 200, 132)                 /* the green of a set value (the manual's drawing marks them so) */
#define C_LED RGB(255, 46, 30)                  /* the LFO LED */
#define C_SCREEN RGB(4, 6, 6)
#define C_GRID RGB(18, 26, 24)
#define C_GRID2 RGB(30, 44, 40)
#define C_GRID3 RGB(70, 96, 88)
#define C_TEXT C_INK
#define C_DIMT C_INK2
#define C_OFF RGB(72, 70, 66)
/* the module colours stay (their tabs' accents in the graphs and on the values): the green of the panel is the base */
#define C_PERF C_ACC
#define C_VCO RGB(255, 168, 70)
#define C_VCF RGB(255, 96, 82)
#define C_ENV C_ACC
#define C_LFO RGB(196, 160, 255)
#define C_UTL RGB(92, 210, 228)
#define C_JACK RGB(255, 214, 70)
#define C_GLOB C_INK
#define C_FXC RGB(120, 190, 255)

static uint16_t blend565(uint16_t a, uint16_t b, uint32_t t)   /* a * (1 - t) + b * t, t = 0..256 */
{
    uint32_t r = ((a >> 11) * (256u - t) + (b >> 11) * t) >> 8;
    uint32_t g = (((a >> 5) & 63u) * (256u - t) + ((b >> 5) & 63u) * t) >> 8;
    uint32_t bl = ((a & 31u) * (256u - t) + (b & 31u) * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline uint16_t cv_get(int32_t x, int32_t y)
{
    y += cv_oy;
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h)
        return swap16(cv_px[(uint32_t)y * cv_w + (uint32_t)x]);
    return C_BG;
}
static inline void cv_mix(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* c over the pixel at a (0..256) */
{
    if (a >= 256u) cv_pset(x, y, c);
    else if (a) cv_pset(x, y, blend565(cv_get(x, y), c, a));
}
static void cv_fill_a(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t a)   /* a translucent rectangle */
{
    int32_t i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) cv_mix(i, j, c, a);
}

/* ---- the anti-aliased text (mfm_fonts.h): y is the top of the line; tr: tracking (-1: condensed, as the panel's) */
static const aglyph_t *aa_glyph(const afont_t *f, char ch)
{
    if (f->map) {
        uint32_t i;
        for (i = 0; f->map[i]; i++) if (f->map[i] == ch) return &f->g[i];
        return 0;
    }
    if (ch < 32 || ch > 126) ch = '?';
    return &f->g[ch - 32];
}
static int32_t aa_wt(const afont_t *f, const char *s, int32_t tr)
{
    int32_t w = 0;
    for (; *s; s++) { const aglyph_t *g = aa_glyph(f, *s); w += (g ? g->adv : 4) + tr; }
    return w;
}
static int32_t aa_w(const afont_t *f, const char *s) { return aa_wt(f, s, 0); }
static int32_t aa_textt(int32_t x, int32_t y, const afont_t *f, const char *s, uint16_t c, int32_t tr)
{
    for (; *s; s++) {
        const aglyph_t *g = aa_glyph(f, *s);
        int32_t i, j, bw;
        if (!g) { x += 4 + tr; continue; }
        bw = (g->w + 1) >> 1;
        for (j = 0; j < g->h; j++) {
            const uint8_t *row = f->px + g->off + j * bw;
            for (i = 0; i < g->w; i++) {
                uint32_t a = (uint32_t)(i & 1 ? row[i >> 1] & 15u : row[i >> 1] >> 4);
                if (a) cv_mix(x + g->xo + i, y + g->yo + j, c, a == 15u ? 256u : a * 17u);
            }
        }
        x += g->adv + tr;
    }
    return x;
}
static int32_t aa_text(int32_t x, int32_t y, const afont_t *f, const char *s, uint16_t c) { return aa_textt(x, y, f, s, c, 0); }
static void aa_text_c(int32_t cx, int32_t y, const afont_t *f, const char *s, uint16_t c) { aa_text(cx - aa_w(f, s) / 2, y, f, s, c); }
static void aa_text_r(int32_t rx, int32_t y, const afont_t *f, const char *s, uint16_t c) { aa_text(rx - aa_w(f, s), y, f, s, c); }
/* a label in the panel's manner: condensed when it is long, centred */
static void aa_label_c(int32_t cx, int32_t y, const afont_t *f, const char *s, uint16_t c, int32_t maxw)
{
    int32_t tr = aa_w(f, s) > maxw ? -1 : 0;
    aa_textt(cx - aa_wt(f, s, tr) / 2, y, f, s, c, tr);
}
/* a knob's name as the panel prints it: on one line, or broken at its last space onto two (PITCH MOD / AMT) */
static void knob_label(int32_t cx, int32_t y, const char *s, uint16_t c, int32_t maxw)
{
    char a[20], b[12];
    int32_t n = 0, sp = -1, i;
    while (s[n] && n < 19) { if (s[n] == ' ') sp = n; n++; }
    if (aa_w(&FA8, s) <= maxw || sp < 0) { aa_label_c(cx, y + 4, &FA8, s, c, maxw); return; }
    for (i = 0; i < sp; i++) a[i] = s[i];
    a[sp] = 0;
    for (i = sp + 1; i < n && i - sp - 1 < 11; i++) b[i - sp - 1] = s[i];
    b[i - sp - 1] = 0;
    aa_label_c(cx, y, &FA8, a, c, maxw);
    aa_label_c(cx, y + 9, &FA8, b, c, maxw);
}

/* ---- an anti-aliased line (X. Wu), the ends in Q8 pixels */
static void cv_wu(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 - x0, dy = y1 - y0, steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx), t, grad, x, xe, inter;
    if (steep) { t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    dx = x1 - x0; dy = y1 - y0;
    grad = dx > 255 ? (dy * 256) / dx : dy;
    x = (x0 + 128) >> 8; xe = (x1 + 128) >> 8;
    inter = y0 + ((x * 256 - x0) * grad) / 256;
    for (; x <= xe; x++) {
        int32_t yi = inter >> 8;
        uint32_t f = (uint32_t)(inter & 255);
        if (steep) { cv_mix(yi, x, c, 256u - f); cv_mix(yi + 1, x, c, f); }
        else { cv_mix(x, yi, c, 256u - f); cv_mix(x, yi + 1, c, f); }
        inter += grad;
    }
}
static void cv_wu2(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* ~2 px wide */
{
    cv_wu(x0, y0, x1, y1, c);
    cv_wu(x0 + 128, y0 + 128, x1 + 128, y1 + 128, c);
}

/* ---- shapes */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a filled rectangle, its corners rounded */
{
    cv_rect(x + 1, y, w - 2, h, c);
    cv_rect(x, y + 1, 1, h - 2, c);
    cv_rect(x + w - 1, y + 1, 1, h - 2, c);
    cv_mix(x, y, c, 110); cv_mix(x + w - 1, y, c, 110); cv_mix(x, y + h - 1, c, 110); cv_mix(x + w - 1, y + h - 1, c, 110);
}
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a 1 px outline, its corners rounded */
{
    cv_rect(x + 2, y, w - 4, 1, c); cv_rect(x + 2, y + h - 1, w - 4, 1, c);
    cv_rect(x, y + 2, 1, h - 4, c); cv_rect(x + w - 1, y + 2, 1, h - 4, c);
    cv_mix(x + 1, y + 1, c, 200); cv_mix(x + w - 2, y + 1, c, 200); cv_mix(x + 1, y + h - 2, c, 200); cv_mix(x + w - 2, y + h - 2, c, 200);
}
/* an anti-aliased disc with a vertical gradient (top colour -> bottom colour) */
static void cv_disc_g(int32_t cx, int32_t cy, int32_t r, uint16_t ct, uint16_t cb)
{
    int32_t x, y, ro2 = (2 * r + 1) * (2 * r + 1), ri2 = (2 * r - 1) * (2 * r - 1);
    for (y = cy - r - 1; y <= cy + r + 1; y++) {
        uint16_t c = blend565(ct, cb, (uint32_t)clampi((y - cy + r) * 256 / (2 * r + 1), 0, 256));
        for (x = cx - r - 1; x <= cx + r + 1; x++) {
            int32_t dx = 2 * (x - cx), dy = 2 * (y - cy), d2 = dx * dx + dy * dy;
            if (d2 >= ro2) continue;
            cv_mix(x, y, c, d2 > ri2 ? (uint32_t)(256 * (ro2 - d2) / (ro2 - ri2)) : 256u);
        }
    }
}
/* a soft glow (a light seen through the panel): a at the centre, fading to 0 at r */
static void cv_glow(int32_t cx, int32_t cy, int32_t r, uint16_t c, uint32_t a)
{
    int32_t x, y, r2 = r * r;
    for (y = cy - r; y <= cy + r; y++)
        for (x = cx - r; x <= cx + r; x++) {
            int32_t d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d2 < r2) cv_mix(x, y, c, a * (uint32_t)(r2 - d2) / (uint32_t)r2 * (uint32_t)(r2 - d2) / (uint32_t)r2);
        }
}
static void knob_xy(int32_t cx, int32_t cy, int32_t rad, int32_t deg, int32_t *x, int32_t *y)   /* Q8 point on a circle */
{
    int32_t i, f, c0, c1, s0, s1;
    while (deg < 0) deg += 360 * 16;
    deg %= 360 * 16;                            /* (deg in 1/16 degree) */
    i = deg / 80; f = deg % 80;                 /* (the table's 5-degree steps) */
    c0 = COS_Q8[i % 72u]; c1 = COS_Q8[(i + 1) % 72u];
    s0 = COS_Q8[(i + 54) % 72u]; s1 = COS_Q8[(i + 55) % 72u];
    *x = cx * 256 + rad * (c0 + (c1 - c0) * f / 80);
    *y = cy * 256 - rad * (s0 + (s1 - s0) * f / 80);
}
static void cv_arc(int32_t cx, int32_t cy, int32_t rad, int32_t d0, int32_t d1, uint16_t c, int wide)   /* d0 > d1, degrees */
{
    int32_t d, x0, y0, x1, y1;
    knob_xy(cx, cy, rad, d0 * 16, &x0, &y0);
    for (d = d0 * 16 - 64; ; d -= 64) {
        if (d < d1 * 16) d = d1 * 16;
        knob_xy(cx, cy, rad, d, &x1, &y1);
        if (wide) cv_wu2(x0, y0, x1, y1, c); else cv_wu(x0, y0, x1, y1, c);
        x0 = x1; y0 = y1;
        if (d == d1 * 16) break;
    }
}
/* a hex nut (the jacks' nuts, turned by rot degrees: the splash screws them on) */
static void cv_hexnut(int32_t cx, int32_t cy, int32_t r, int32_t rot, uint16_t c)
{
    int32_t k, x0, y0, x1, y1;
    for (k = 0; k < 6; k++) {
        knob_xy(cx, cy, r, (rot + 60 * k) * 16, &x0, &y0);
        knob_xy(cx, cy, r, (rot + 60 * k + 60) * 16, &x1, &y1);
        cv_wu(x0, y0, x1, y1, c);
    }
}
/* a patch point: its nut and socket; a plug in it (with a cable of colour pc) when patched; glow: a ring around it */
static void cv_jack(int32_t cx, int32_t cy, int32_t r, uint16_t pc, uint32_t glow)
{
    if (glow) cv_glow(cx, cy, r + 5, C_ACC, glow);
    cv_disc_g(cx, cy, r, RGB(62, 62, 66), RGB(28, 28, 30));
    cv_hexnut(cx, cy, r, 0, RGB(196, 196, 200));
    cv_disc_g(cx, cy, r - 2, RGB(36, 36, 38), RGB(12, 12, 13));
    if (pc) {
        cv_disc_g(cx, cy, r - 2, blend565(pc, C_INK, 70), blend565(pc, C_SH, 110));
        cv_disc_g(cx, cy, r / 3, blend565(pc, C_SH, 160), C_SH);
    } else
        cv_disc_g(cx, cy, r / 3, C_SH, C_SH);
}
/* a toggle switch (VCA MODE, an ON / OFF): its two positions printed above and below, the bat up or down */
static void cv_toggle(int32_t cx, int32_t cy, int up, const char *top, const char *bot, uint16_t acc)
{
    int32_t tx, ty;
    aa_text_c(cx, cy - 21, &FA8, top, up ? acc : C_INK2);
    aa_text_c(cx, cy + 14, &FA8, bot, up ? C_INK2 : acc);
    cv_hexnut(cx, cy, 8, 30, RGB(170, 170, 176));
    cv_disc_g(cx, cy, 6, RGB(90, 90, 96), RGB(30, 30, 32));
    tx = cx * 256; ty = (cy + (up ? -10 : 10)) * 256;
    cv_wu2(cx * 256 - 128, cy * 256, tx - 128, ty, RGB(220, 220, 226));
    cv_wu2(cx * 256 + 128, cy * 256, tx + 128, ty, RGB(150, 150, 156));
    cv_disc_g(cx, cy + (up ? -10 : 10), 3, C_INK, RGB(120, 120, 126));
}

/* ---- the little signs under the knobs, as the panel prints them (8 x 7 px, x y their top left) */
enum { IC_NONE, IC_SAW, IC_PULSE, IC_TRI, IC_SQUARE, IC_FOLD0, IC_FOLD3, IC_NARROW, IC_EG, IC_LFO, IC_MINUS, IC_PLUS,
       IC_ATT, IC_DEC, IC_SUS, IC_REL, IC_LOW, IC_HIGH, IC_SLOW, IC_FAST };
static void ic_poly(int32_t x, int32_t y, const int8_t *p, uint32_t n, uint16_t c)   /* a polyline, points in px */
{
    uint32_t i;
    for (i = 1; i < n; i++) cv_wu((x + p[2 * i - 2]) * 256, (y + p[2 * i - 1]) * 256, (x + p[2 * i]) * 256, (y + p[2 * i + 1]) * 256, c);
}
static void cv_icon(int32_t x, int32_t y, uint32_t ic, uint16_t c)
{
    static const int8_t SAW[] = {0, 6, 7, 0, 7, 6}, PULSE[] = {0, 6, 2, 6, 2, 0, 5, 0, 5, 6, 8, 6}, TRI[] = {0, 6, 4, 0, 8, 6};
    static const int8_t SQ[] = {0, 6, 0, 1, 4, 1, 4, 6, 8, 6, 8, 1}, F0[] = {0, 6, 4, 1, 8, 6};
    static const int8_t F3[] = {0, 6, 2, 1, 3, 4, 4, 1, 5, 4, 6, 1, 8, 6}, NAR[] = {0, 6, 3, 6, 3, 0, 4, 0, 4, 6, 8, 6};
    static const int8_t ATT[] = {0, 6, 3, 0, 4, 3, 7, 3, 8, 6}, DEC[] = {0, 6, 1, 0, 4, 3, 7, 3, 8, 6};
    static const int8_t SUS[] = {0, 6, 1, 0, 2, 3, 6, 3, 8, 6}, REL[] = {0, 6, 1, 0, 2, 3, 5, 3, 8, 6};
    switch (ic) {
    case IC_SAW: ic_poly(x, y, SAW, 3, c); break;
    case IC_PULSE: ic_poly(x, y, PULSE, 6, c); break;
    case IC_TRI: ic_poly(x, y, TRI, 3, c); break;
    case IC_SQUARE: ic_poly(x, y, SQ, 6, c); break;
    case IC_FOLD0: ic_poly(x, y, F0, 3, c); break;
    case IC_FOLD3: ic_poly(x, y, F3, 7, c); break;
    case IC_NARROW: ic_poly(x, y, NAR, 6, c); break;
    case IC_ATT: ic_poly(x, y, ATT, 5, c); cv_wu((x + 1) * 256, (y + 7) * 256, (x + 3) * 256, (y + 7) * 256, C_ACC); break;
    case IC_DEC: ic_poly(x, y, DEC, 5, c); cv_wu((x + 1) * 256, (y + 7) * 256, (x + 4) * 256, (y + 7) * 256, C_ACC); break;
    case IC_SUS: ic_poly(x, y, SUS, 5, c); cv_wu((x + 2) * 256, (y + 7) * 256, (x + 6) * 256, (y + 7) * 256, C_ACC); break;
    case IC_REL: ic_poly(x, y, REL, 5, c); cv_wu((x + 5) * 256, (y + 7) * 256, (x + 8) * 256, (y + 7) * 256, C_ACC); break;
    case IC_EG: aa_text(x + 1, y + 1, &FA8, "EG", c); break;
    case IC_LFO: aa_textt(x - 3, y + 1, &FA8, "LFO", c, -1); break;
    case IC_MINUS: aa_text(x + 1, y + 1, &FA8, "-", c); break;
    case IC_PLUS: aa_text(x, y + 1, &FA8, "+", c); break;
    case IC_LOW: aa_text(x, y + 1, &FA8, "LO", c); break;
    case IC_HIGH: aa_text(x, y + 1, &FA8, "HI", c); break;
    default: break;
    }
}

/* ---- a knob in a 54 x 80 cell (cell top at y0): the label, the knob (or a jack, or a toggle), its signs, the value */
enum { KS_KNOB, KS_BIG, KS_JACK, KS_TOGGLE };
typedef struct {
    const char *label, *top, *bot;              /* (a toggle: its positions' names) */
    char value[16];
    int32_t v, max, bip, present;
    uint8_t style, icl, icr;                    /* the signs at the scale's ends */
    uint16_t cable;                             /* a jack: its cable's colour (0: no cable) */
} knob_view_t;
#define KCELL_W 54
static void ui_knob_at(int32_t x0, int32_t y0, const knob_view_t *kv, uint16_t acc, uint32_t glow)
{
    int32_t cx = x0 + KCELL_W / 2, cy = y0 + 40, ang, px, py, qx, qy, i;
    if (!kv->present) return;
    if (glow) cv_glow(cx, cy, 26, acc, glow / 3u);   /* (the knob being turned: a soft light under it) */
    knob_label(cx, y0, kv->label, C_INK, KCELL_W - 2);
    if (kv->style == KS_JACK) {                 /* a patch point, its cable going up out of it */
        if (kv->cable) {
            cv_wu2((cx - 3) * 256, (cy - 5) * 256, (cx - 10) * 256, (y0 + 19) * 256, blend565(kv->cable, C_SH, 90));
            cv_wu2((cx - 2) * 256, (cy - 5) * 256, (cx - 9) * 256, (y0 + 19) * 256, kv->cable);
        }
        cv_jack(cx, cy + 2, 9, kv->cable, 0);
        aa_label_c(cx, y0 + 64, &FA10, kv->value, kv->cable ? acc : C_INK2, KCELL_W);
        return;
    }
    if (kv->style == KS_TOGGLE) {
        cv_toggle(cx, cy + 2, kv->v != 0, kv->top, kv->bot, acc);
        aa_label_c(cx, y0 + 64, &FA10, kv->value, acc, KCELL_W);
        return;
    }
    ang = 225 - (kv->max ? 270 * kv->v / kv->max : 0);
    if (kv->style == KS_BIG)                    /* a big knob: a dense ring of fine ticks, as the panel's */
        for (i = 0; i <= 30; i++) {
            int32_t d = (225 - 9 * i) * 16;
            knob_xy(cx, cy, 19, d, &px, &py);
            knob_xy(cx, cy, 22, d, &qx, &qy);
            cv_wu(px, py, qx, qy, i % 5 ? RGB(120, 120, 120) : C_INK);
        }
    else
        for (i = 0; i <= 8; i++) {              /* a small one: nine ticks */
            int32_t d = (225 - (270 * i) / 8) * 16;
            knob_xy(cx, cy, 17, d, &px, &py);
            knob_xy(cx, cy, 21, d, &qx, &qy);
            cv_wu(px, py, qx, qy, C_INK);
        }
    if (kv->bip) {                              /* the value's arc (bipolar: from the top), and the 0 mark */
        if (ang < 90) cv_arc(cx, cy, 24, 90, ang, acc, 1); else cv_arc(cx, cy, 24, ang, 90, acc, 1);
        cv_rect(cx, cy - 26, 1, 3, C_INK);
    } else
        cv_arc(cx, cy, 24, 225, ang, acc, 1);
    cv_disc_g(cx + 1, cy + 2, 14, C_SH, C_SH);  /* the knob: black, a soft top light, a white line */
    cv_disc_g(cx, cy, 14, RGB(70, 70, 74), RGB(14, 14, 15));
    cv_disc_g(cx, cy, 12, RGB(34, 34, 36), RGB(6, 6, 7));
    cv_disc_g(cx - 3, cy - 4, 4, RGB(64, 64, 68), RGB(34, 34, 36));
    knob_xy(cx, cy, 2, ang * 16, &px, &py);
    knob_xy(cx, cy, 13, ang * 16, &qx, &qy);
    cv_wu2(px, py, qx, qy, C_INK);
    if (kv->icl) cv_icon(cx - 25, cy + 16, kv->icl, C_INK2);
    if (kv->icr) cv_icon(cx + 17, cy + 16, kv->icr, C_INK2);
    aa_label_c(cx, y0 + 64, &FA10, kv->value, acc, KCELL_W);
}

/* the graphs a page can show (mfm_graph.h draws them) */
enum { G_SCOPE, G_VCO, G_VMOD, G_VCF, G_FMOD, G_EG, G_VCA, G_LFO, G_SH, G_FOLD, G_UMIX, G_ATTN, G_PATCH, G_KB, G_ARP, G_CHO,
       G_DLY, G_REV, G_TRACKS, G_CLK, NG };
/* the modules (the tab of a page's box; TAB_NONE: the section's own) */
enum { TAB_NONE = -1, TAB_VCO, TAB_VCF, TAB_EG, TAB_VCA, TAB_LFO, TAB_UTL, TAB_JACK, TAB_FX, NTAB };
static const uint16_t TRKC[4] = {RGB(255, 84, 70), RGB(255, 204, 64), RGB(110, 222, 120), RGB(96, 172, 255)};

/* ---- the top strip: the name, the LFO LED (lit 0..256), the tracks (their lamps 0..256, the panel's outlined) */
static void ui_top(uint32_t led, const uint16_t *lamp, uint32_t trk)
{
    uint32_t i;
    cv_begin(240, 16, C_BG);
    aa_textt(6, 2, &FA10, "FMAV-1", C_INK, 0);   /* (0.3: the name; FM-1 is in it) */
    aa_text(84, 4, &FA8, "LFO", C_INK2);                 /* the LFO LED: dark red, lit and glowing with the LFO */
    if (led) cv_glow(108, 8, 8, C_LED, led * 3u / 4u);
    cv_disc_g(108, 8, 3, blend565(RGB(60, 10, 8), RGB(255, 140, 120), led), blend565(RGB(30, 4, 4), C_LED, led));
    for (i = 0; i < NTRK; i++) {
        int32_t x = (NTRK > 1u ? 160 : 216) + 19 * (int32_t)i;
        uint16_t c = TRKC[i & 3u], lit = blend565(blend565(c, C_SH, 190), c, lamp[i]);
        char d[2] = {(char)('1' + i), 0};
        if (lamp[i]) cv_glow(x + 8, 8, 9, c, lamp[i] / 2u);
        cv_round(x, 2, 17, 12, lit);
        if (i == trk % NTRK) cv_frame(x - 1, 1, 19, 14, C_INK);
        aa_text_c(x + 8, 3, &FA8, d, lamp[i] > 128u ? RGB(16, 12, 10) : C_INK);
    }
    cv_blit(0, 0);
}

/* ---- the program: its number on an LCD in the track's colour, the name, the patch's state (or a message) */
static void ui_prog(uint32_t prog, const uint8_t *name, uint32_t edited, const char *tags, const char *msg, uint32_t trk)
{
    char n[PATCH_NAME + 2], d[3];
    uint32_t i;
    cv_begin(240, 32, C_BG);
    {
        uint16_t tc = TRKC[trk & 3u], lt = blend565(RGB(14, 12, 10), tc, 44), lb = blend565(RGB(6, 5, 4), tc, 16);
        cv_round(6, 1, 40, 29, C_SH);
        for (i = 1; i < 28u; i++) cv_rect(7, 1 + (int32_t)i, 38, 1, blend565(lt, lb, i * 9u));
        d[0] = (char)('0' + (prog + 1u) / 10u % 10u); d[1] = (char)('0' + (prog + 1u) % 10u); d[2] = 0;
        aa_text(26 - aa_w(&FA26, d) / 2 + 1, -1, &FA26, d, blend565(lb, tc, 70));
        aa_text_c(26, -2, &FA26, d, tc);
    }
    for (i = 0; i < PATCH_NAME; i++) n[i] = (char)name[i];
    while (i > 0u && n[i - 1u] == ' ') i--;
    n[i] = edited ? '*' : 0;
    n[i + 1u] = 0;
    aa_text(53, 0, &FA16, msg ? msg : n, msg ? C_LED : C_INK);
    aa_text(54, 19, &FA8, tags, C_INK2);
    cv_rect(6, 31, 228, 1, C_RULE);
    cv_blit(0, 16);
}

/* ---- the page: the module's box (its tab on the left, as the panel prints V C O ...), title and dots, four knobs */
static void app_idle(void);
typedef struct { uint8_t n, item, pg, npg[8]; } pmap_t;
static void ui_box(int32_t h, const char *tab, uint16_t acc)   /* the box over a canvas h tall (from y 0) */
{
    int32_t i, n = 0, y;
    while (tab[n]) n++;
    cv_rect(7, 1, 226, h - 2, C_BOX);
    cv_frame(6, 0, 228, h, C_BOXL);
    cv_round(6, 0, 12, h, C_BOXL);              /* the tab: white, its letters black, one under the other */
    y = (h - n * 9) / 2;
    for (i = 0; i < n; i++) {
        char c[2] = {tab[i], 0};
        aa_text_c(12, y + 9 * i, &FA8, c, C_SH);
    }
    cv_rect(17, 2, 1, h - 4, blend565(C_BOXL, acc, 120));
}
static void page_title(const char *title, const char *sub, const pmap_t *pm, const char *note, uint16_t acc)
{
    int32_t pitch = 6, gap = 5, dw = 0, xe, x;
    uint32_t i, j, tot = 0;
    for (i = 0; i < pm->n; i++) tot += pm->npg[i];
    if (tot > 1u) {
        if ((int32_t)tot * 6 + (int32_t)(pm->n - 1u) * 5 > 80) { pitch = 5; gap = 4; }
        dw = (int32_t)tot * pitch + (int32_t)(pm->n - 1u) * gap - (pitch - 4);
    }
    xe = aa_text(22, 2, &FA10, title, C_INK);
    if (sub) xe = aa_text(xe + 6, 2, &FA10, sub, acc);
    if (note) aa_text_r(dw ? 226 - dw : 228, 3, &FA8, note, acc);
    (void)xe;
    if (tot > 1u)
        for (i = 0, x = 229 - dw; i < pm->n; i++, x += gap)
            for (j = 0; j < pm->npg[i]; j++, x += pitch) {
                uint16_t c = i == pm->item ? (j == pm->pg ? acc : blend565(C_BOX, acc, 100)) : RGB(64, 64, 66);
                cv_round(x, 6, 4, 4, c);
            }
}
static void ui_page(const char *tab, const char *title, const char *sub, const pmap_t *pm, const char *note,
                    const knob_view_t *kv, uint16_t acc)
{
    uint32_t i;
    cv_begin(240, 94, C_BG);
    ui_box(94, tab, acc);
    page_title(title, sub, pm, note, acc);
    for (i = 0; i < 4u; i++) {
        ui_knob_at(18 + KCELL_W * (int32_t)i, 15, &kv[i], acc, 0);
        app_idle();                             /* (a knob is ~1 ms: the keys and MIDI in between) */
    }
    cv_blit(0, 48);
}
static void ui_knob(uint32_t i, const knob_view_t *kv, uint16_t acc, uint32_t glow)   /* one knob alone (a turn) */
{
    cv_begin(KCELL_W - ((i & 3u) == 3u), 77, C_BOX);   /* (inside the box: its frame, bottom and right, untouched) */
    ui_knob_at(0, -1, kv, acc, glow);
    cv_blit(18u + KCELL_W * (i & 3u), 64);
}

/* ---- the screen (y 142..203): a dark glass in a thin frame, drawing area x 10..229, y 3..58 of the strip */
#define GX0 10
#define GY0 3
#define GW 220
#define GH 56
static void g_begin(void)
{
    int32_t x;
    cv_begin(240, 62, C_BG);
    cv_round(7, 1, 226, 60, C_SCREEN);
    cv_frame(6, 0, 228, 62, RGB(90, 90, 94));
    for (x = GX0 + 22; x < GX0 + GW; x += 22) cv_rect(x, GY0, 1, GH, C_GRID);
    cv_rect(GX0, GY0 + GH / 2, GW, 1, C_GRID);
}
static void g_end(void) { cv_blit(0, 142); }
/* a curve: n points at x0 .. x0 + n - 1, y in Q8 pixels; fill: the area down to fy translucent (fy < 0: none) */
static void g_curve(const int32_t *yq, int32_t x0, int32_t n, uint16_t c, int32_t fy)
{
    int32_t i;
    if (fy >= 0)
        for (i = 0; i < n; i++) {
            int32_t y = (yq[i] + 128) >> 8, a = y < fy ? y : fy, b = y < fy ? fy : y, j;
            for (j = a; j <= b; j++) cv_mix(x0 + i, j, c, 46);
        }
    app_idle();
    for (i = 1; i < n; i++) {
        cv_wu((x0 + i - 1) * 256, yq[i - 1] + 256, (x0 + i) * 256, yq[i] + 256, blend565(C_SCREEN, c, 80));   /* (the glow under) */
        cv_wu((x0 + i - 1) * 256, yq[i - 1], (x0 + i) * 256, yq[i], c);
    }
}
/* the scope: the output as it plays */
static void g_scope(const int16_t *s, uint32_t n, uint16_t c)
{
    static int32_t yq[220];
    uint32_t i;
    g_begin();
    for (i = 0; i < 220u && i + 10u < n; i++)
        yq[i] = ((GY0 + GH / 2) << 8) - clampi(s[i + 10u] * 24 / 32, -27 * 256, 27 * 256);
    g_curve(yq, GX0, (int32_t)(i < 220u ? i : 220u), c, -1);
    aa_text(GX0 + 2, GY0, &FA8, "OUTPUT", blend565(C_SCREEN, c, 150));
    g_end();
}

/* ---- the foot: the reference panel's 13 buttons (C .. C: 5 above, 8 below), each lit 0..256 by the notes (fading), the page's
 * hint (two lines, split at '|') and the note */
static void ui_keys(const uint16_t *lit, const char *hint, const char *note)
{
    static const uint8_t UP[5] = {1, 3, 6, 8, 10}, LO[8] = {0, 2, 4, 5, 7, 9, 11, 12};
    static const uint8_t UPX[5] = {0, 1, 3, 4, 5};   /* (the upper buttons sit between lower ones: after 0, 1, 3, 4, 5) */
    char l1[24], l2[24];
    uint32_t i, k = 0, j = 0;
    cv_begin(240, 36, C_BG);
    for (i = 0; i < 8u; i++) {                  /* the lower row */
        int32_t x = 8 + 16 * (int32_t)i, y = 20;
        uint32_t a = lit[LO[i]];
        if (a) cv_glow(x + 7, y + 6, 11, C_ACC, a / 2u);
        cv_round(x, y, 14, 13, blend565(C_INK, C_ACC, a));
        cv_round(x + 1, y + 1, 12, 11, blend565(RGB(20, 20, 22), blend565(C_ACC, C_INK, 60), a));
    }
    for (i = 0; i < 5u; i++) {                  /* the upper row */
        int32_t x = 16 + 16 * (int32_t)UPX[i], y = 4;
        uint32_t a = lit[UP[i]];
        if (a) cv_glow(x + 7, y + 6, 11, C_ACC, a / 2u);
        cv_round(x, y, 14, 13, blend565(C_INK, C_ACC, a));
        cv_round(x + 1, y + 1, 12, 11, blend565(RGB(20, 20, 22), blend565(C_ACC, C_INK, 60), a));
    }
    if (note) aa_text_r(232, 3, &FA10, note, C_ACC);
    if (hint) {
        while (hint[k] && hint[k] != '|' && k < 23u) { l1[k] = hint[k]; k++; }
        l1[k] = 0;
        if (hint[k] == '|') { k++; while (hint[k] && j < 23u) l2[j++] = hint[k++]; }
        l2[j] = 0;
        aa_text_r(232, j ? 15 : 22, &FA8, l1, C_INK2);
        if (j) aa_text_r(232, 25, &FA8, l2, C_INK2);
    }
    cv_blit(0, 204);
}
/* a message over the foot: white print on a black plate in a white frame (one or two lines, split at '|') */
static void ui_toast(const char *msg)
{
    char l1[40], l2[40];
    uint32_t k = 0, j = 0;
    cv_begin(240, 36, C_BG);
    cv_round(7, 2, 226, 32, C_SH);
    cv_frame(6, 1, 228, 34, C_INK);
    while (msg[k] && msg[k] != '|' && k < 39u) { l1[k] = msg[k]; k++; }
    l1[k] = 0;
    if (msg[k] == '|') { k++; while (msg[k] && j < 39u) l2[j++] = msg[k++]; }
    l2[j] = 0;
    if (j) { aa_text_c(120, 5, &FA10, l1, C_INK); aa_text_c(120, 19, &FA10, l2, C_ACC); }
    else aa_text_c(120, 11, &FA12, l1, C_INK);
    cv_blit(0, 204);
}

static void ui_wait_ms(uint32_t ms)             /* (polling: USB and the watchdog go on) */
{
    uint32_t t0 = fm1_ms;
    while (fm1_ms - t0 < ms) platform_poll();
}

/* ---- the splash: the kit put together. The case's frame draws itself, the jacks' nuts are screwed on (turning as
 * they come), a cable drops into the VCA's output, the LFO LED lights ("when the red LFO RATE LED lights up, your
 * synthesizer is powered on"), the name comes up */
static void ui_splash(void)
{
    static const uint8_t JX[3] = {40, 120, 200};
    uint32_t t, y;
    for (y = 0; y < 240u; y += 60u) { cv_begin(240, 60, C_BG); cv_blit(0, y); }
    for (t = 0; t <= 36u; t++) {
        uint32_t j, f = t > 30u ? 256u : t * 256u / 30u;
        cv_begin(240, 120, C_BG);
        cv_frame(4, 0, 232, (int32_t)(8u + 112u * (t > 10u ? 256u : t * 256u / 10u) / 256u), blend565(C_BG, C_BOXL, t > 10u ? 256u : t * 25u));
        for (j = 0; j < 3u; j++) {              /* three jacks: each nut turns in, then sits */
            int32_t s = (int32_t)t - 6 - 5 * (int32_t)j, rot = s < 0 ? 0 : s > 8 ? 0 : (8 - s) * 22;
            uint16_t nc = s < 0 ? C_BG : blend565(C_BG, RGB(200, 200, 206), (uint32_t)clampi(s * 40, 0, 256));
            if (s < 0) continue;
            cv_disc_g(JX[j], 30, 10, RGB(60, 60, 64), RGB(26, 26, 28));
            cv_hexnut(JX[j], 30, 10, rot, nc);
            cv_disc_g(JX[j], 30, 6, RGB(36, 36, 38), RGB(10, 10, 11));
            aa_text_c(JX[j], 8, &FA8, j == 0 ? "VCA OUT" : j == 1 ? "CV" : "IN", blend565(C_BG, C_INK2, (uint32_t)clampi(s * 40, 0, 256)));
        }
        if (t >= 22u) {                         /* the cable: from above, swinging into VCA OUT (the audio out: 0.3) */
            int32_t s = (int32_t)t - 22, sw = s < 8 ? (8 - s) * (s & 1 ? 3 : -3) : 0, k;
            int32_t x0 = 120 * 256, y0 = -10 * 256, x1 = (JX[0] + sw) * 256, y1 = (30 - (s < 4 ? (4 - s) * 6 : 0)) * 256;
            int32_t px = x0, py = y0;
            for (k = 1; k <= 20; k++) {
                int32_t u = k * 256 / 20, nx = x0 + (x1 - x0) * u / 256, ny = y0 + (y1 - y0) * u / 256 + 18 * 256 * 4 * u * (256 - u) / 65536;
                cv_wu2(px, py, nx, ny, C_JACK);
                px = nx; py = ny;
            }
            cv_disc_g(x1 >> 8, y1 >> 8, 5, blend565(C_JACK, C_INK, 80), blend565(C_JACK, C_SH, 100));
        }
        if (t >= 28u) {                         /* the LED, then the name */
            uint32_t l = (t - 28u) * 40u > 256u ? 256u : (t - 28u) * 40u;
            cv_glow(120, 58, 14, C_LED, l / 2u);
            cv_disc_g(120, 58, 4, blend565(RGB(60, 10, 8), RGB(255, 150, 130), l), blend565(RGB(30, 4, 4), C_LED, l));
            aa_text_c(120, 70, &FA40, "FMAV-1", blend565(C_BG, C_INK, l));
        }
        (void)f;
        cv_blit(0, 40);
        ui_wait_ms(30);
    }
    cv_begin(240, 60, C_BG);
    aa_text_c(120, 6, &FA16, "FM-1", C_ACC);
    aa_text_c(120, 30, &FA8, NTRK > 1u ? "SEMI-MODULAR  -  4 TRACKS" : "SEMI-MODULAR  -  1 TRACK", C_INK2);
    cv_blit(0, 160);
    ui_wait_ms(700);
}
