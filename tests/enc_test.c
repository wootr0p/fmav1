/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the encoders' decoder in hal/fm1_input.h (fm1__frame) through a model of the TIMER5 scan (a column every
 * 100 us, a frame every 11 columns, ~1.1 ms) and of a turned knob (after FMini-1's tests/input_test.c, #126, Ruppert,
 * GPL-3.0). The user, on FMav-1 0.1: "quando giro veloce il knob non mi prende i valori". Checked:
 *   - 12-detent turns at 50 .. 300 detents/s with even transitions, 50 .. 150/s with the 4 transitions bunched in half
 *     a click (the middle states short), over every phase of the scan, both ways: every detent counted;
 *   - slow turns (3 .. 40 detents/s) with contacts bouncing for 1.5 ms: one step a click;
 *   - a knob left alone, one contact chattering or both lost together: never a step.
 *   cc -O1 -o build/host/enc_test tests/enc_test.c && build/host/enc_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t host_now;
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];        /* the GPIO registers, as memory (never really used here) */
#undef FM1_PR
#define FM1_PR(p, r) (host_reg[(p) & 15u][((r) / 4u) & 63u])
static uint32_t host_ticks(void) { return host_now += 2400u; }
#define fm1_ticks host_ticks
#include "../firmware/hal/fm1_input.h"

#define TICK_US 100u
static int fails;
static int check(const char *what, int ok)
{
    printf("%-96s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
    return ok;
}
static uint32_t rs = 0x1234567u;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0u; }
static void reset(void)
{
    uint32_t i;
    memset((void *)&fm1_in, 0, sizeof fm1_in);
    for (i = 0; i < FM1_NENC; i++) fm1_in.enc_prev[i] = fm1_in.enc_last[i] = 0xFF;
}

/* a turned encoder 0: the toggle times of its A (0) and B (1) contacts; a turn of n detents (dir +1 clockwise), per us
 * a detent, its 4 transitions bunched over w (1 = even) of each, each edge bouncing (runs of 30..bmax us) for up to
 * bspan us (at most half the way to the next edge) */
#define ENE 8192
static struct { uint32_t t[ENE], n, i, s; } enl[2];
static void en_clear(void) { memset(enl, 0, sizeof enl); }
static void en_add(uint32_t l, uint32_t t) { if (enl[l].n < ENE && (!enl[l].n || t > enl[l].t[enl[l].n - 1u])) enl[l].t[enl[l].n++] = t; }
static uint32_t en_turn(uint32_t t0, int32_t n, int32_t dir, double per, double w, uint32_t bspan, uint32_t bmax)
{
    int32_t i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < 4; j++) {
            double f = 0.5 - w / 2 + (j + 0.5) * w / 4, nf = j < 3 ? 0.5 - w / 2 + (j + 1.5) * w / 4 : 1.0 + 0.5 - w / 2 + 0.5 * w / 4;
            uint32_t t = t0 + (uint32_t)(per * (i + f)), gap = (uint32_t)(per * (nf - f)), l = (uint32_t)((j & 1) ^ (dir > 0)), end;
            end = t + (bspan < gap / 2u ? bspan : gap / 2u);
            en_add(l, t);
            while (bspan && bmax) {
                uint32_t a = t + 30u + rnd(bmax), b = a + 30u + rnd(bmax);
                if (b >= end) break;
                en_add(l, a);
                en_add(l, b);
                t = b;
            }
        }
    return t0 + (uint32_t)(per * n);
}
static uint32_t en_at(uint32_t l, uint32_t us)
{
    while (enl[l].i < enl[l].n && enl[l].t[enl[l].i] <= us) { enl[l].i++; enl[l].s ^= 1u; }
    return enl[l].s;
}
static int32_t en_run(uint32_t until)              /* the scan of encoder 0 up to `until` us; the steps it counted */
{
    const uint8_t *m = FM1_ENC[0];
    uint32_t us, col = 0;
    for (us = 0; us < until; us += TICK_US) {
        fm1_in.raw[col] = 0;
        if (col == m[0]) fm1_in.raw[col] |= (uint8_t)(en_at(0, us) << m[1]);
        if (col == m[2]) fm1_in.raw[col] |= (uint8_t)(en_at(1, us) << m[3]);
        col = col + 1u == FM1_NCOL ? 0u : col + 1u;
        if (!col) fm1__frame();
    }
    return fm1_in.enc_steps[0];
}

int main(void)
{
    char what[128];
    {
        static const double EVEN[] = {50, 100, 150, 200, 250, 300}, BUNCH[] = {50, 100, 150};
        uint32_t r, ph, d, bad = 0, n = 0, worst = 0;
        for (r = 0; r < 9u; r++)
            for (ph = 0; ph < 40u; ph++)
                for (d = 0; d < 2u; d++) {
                    int32_t dir = d ? -1 : 1, s, off;
                    double rate = r < 6u ? EVEN[r] : BUNCH[r - 6u];
                    reset();
                    en_clear();
                    s = en_run(en_turn(5000u + ph * 53u, 12, dir, 1e6 / rate, r < 6u ? 1.0 : 0.5, 0u, 0u) + 20000u);
                    n++;
                    off = s * dir - 12;
                    if (off) { bad++; if ((uint32_t)(off < 0 ? -off : off) > worst) worst = (uint32_t)(off < 0 ? -off : off); }
                }
        snprintf(what, sizeof what, "fast turns up to 300 detents/s (even), 150/s (bunched): every detent (%u of %u off, worst by %u)",
                 (unsigned)bad, (unsigned)n, (unsigned)worst);
        check(what, !bad);
    }
    {
        static const double SLOW[] = {3, 8, 20, 40};
        uint32_t r, ph, d, bad = 0;
        for (r = 0; r < 4u; r++)
            for (ph = 0; ph < 20u; ph++)
                for (d = 0; d < 2u; d++) {
                    int32_t dir = d ? -1 : 1;
                    reset();
                    en_clear();
                    bad += en_run(en_turn(5000u + ph * 71u, 6, dir, 1e6 / SLOW[r], ph & 1u ? 1.0 : 0.4, 1500u, 400u) + 20000u) != 6 * dir;
                }
        snprintf(what, sizeof what, "slow turns (3..40 detents/s), contacts bouncing 1.5 ms: one step a click (%u off)", (unsigned)bad);
        check(what, !bad);
    }
    {
        uint32_t rest, mode, f, g, moved = 0;
        const uint8_t *m = FM1_ENC[0];
        for (mode = 0; mode < 3u; mode++)
            for (rest = 0; rest < 4u; rest++) {
                reset();
                for (f = 0, g = 0; f < 200000u; f++) {
                    uint32_t st = rest;
                    if (g) { st ^= mode == 0u ? 2u : mode == 1u ? 1u : 3u; g--; }
                    else if (rnd(300) == 0u) g = 1u + rnd(5);
                    if (f < 100u) st = rest;
                    memset((void *)fm1_in.raw, 0, sizeof fm1_in.raw);
                    fm1_in.raw[m[0]] |= (uint8_t)(((st >> 1) & 1u) << m[1]);
                    fm1_in.raw[m[2]] |= (uint8_t)((st & 1u) << m[3]);
                    fm1__frame();
                    moved |= fm1_in.enc_steps[0] != 0;
                }
            }
        check("a still knob: one contact chattering or both lost together never steps", !moved);
    }
    printf(fails ? "SOME ENCODER TESTS FAILED\n" : "encoders: all ok\n");
    return fails != 0;
}
