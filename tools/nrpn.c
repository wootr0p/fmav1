/* SPDX-License-Identifier: GPL-3.0-only */
/* The parameter numbers for test scripts and the docs: prints "index cc name" for every parameter (NRPN 0 / index, and
 * the CC it got). cc -Ifirmware/src -Ifirmware/gen -o build/nrpn tools/nrpn.c && build/nrpn */
#include <stdint.h>
#include <stdio.h>
#define FS 44100u
#include "mfm_params.h"

static int cc_reserved(uint32_t cc) { return cc == 64u || (cc >= 71u && cc <= 75u) || (cc >= 91u && cc <= 93u) || cc == 98u || cc == 99u; }   /* (as fmav1.c) */

int main(void)
{
    uint8_t cc_of[NPARAM] = {0};
    uint32_t p, cc = 20;
    for (p = 0; p < NPARAM; p++) if (PDEF[p].cc) cc_of[p] = PDEF[p].cc;
    for (p = 0; p < NPARAM && cc < 120u; p++) {   /* (as cc_map_init in fmav1.c) */
        if (PDEF[p].cc) continue;
        while (cc_reserved(cc)) cc++;
        if (cc >= 120u) break;
        cc_of[p] = (uint8_t)cc++;
    }
    for (p = 0; p < NPARAM; p++) printf("%u %u %s\n", p, cc_of[p], PDEF[p].name);
    return 0;
}
