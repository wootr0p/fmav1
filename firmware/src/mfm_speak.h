/* SPDX-License-Identifier: GPL-3.0-only */
/* (0.6) The screen for a screen reader on the computer (fm1-reader, https://charlesvestal.github.io/fm1-reader/):
 * announce.c sends the screen's own text over USB-MIDI SysEx, and the reader says it with this build's words file
 * (firmware/words.tsv: "KB CV" is "keyboard C V"). Nothing is built while no reader listens.
 *   WHERE (the reader asks, or it has just started listening): the program, then the page as a new page is said (the
 *     user: no values at the start either)
 *   a new page (a section button, SELECT, REC held for the CCs): its place among its button's pages and its title
 *     ("1 OF 7. DELAY OFF"; one page alone: the title), then (a message of
 *     its own: a longer pause) the knobs' names only ("DELAY SWITCH, SEND, TIME L, TIME R"; REC held: the CCs)
 *   the short words in full (SP_WORDS: "TIME L" is "TIME LEFT", "EG" "ENVELOPE"), with or without the
 *     reader's words file
 *   a knob turned: its name and value, once it is set (SP_SETTLE_MS still: the user, nothing while it turns); the same
 *     for PRESETS (the program, or the slot SAVE writes to), ALGORITHM (the track) and SELECT (the page)
 *   every message on the screen (say / warn: a warning is heard whole); not the octave (the user: playing tells)
 * Never the page's hint at the bottom right (the user: not worth hearing). The audit (tests/sr_audit.sh, fm1-reader's
 * tools/audit.mjs) reads "speakdump" (every page, knob value and program) and the messages the scenario tests made. */
#define SP_SETTLE_MS 500u                       /* (the user) a value is said once it is set: still this long */

static struct {
    uint32_t ctx, ctx_t, knob_t, prog, trk, save_to, save_armed, started, msg_flags;
    uint8_t ctx_pend, knob_pend, prog_pend, save_pend, msg_pend, where_soft, ctx_values, knob_rec, after_page;
} sp;
static char sp_buf[ANN_MAX + 1], sp_msg[ANN_MAX + 1];
static uint32_t sp_n;                           /* (sp_turn_t, sp_in_enc: by encoders(), in fmav1.c) */

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX) sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_num(uint32_t v) { char t[12]; fmt_u(t, v); sp_cat(t); }
static int sp_has(const char *a, const char *b, char c)   /* c in a .. b */
{
    for (; a < b && *a; a++) if (*a == c) return 1;
    return 0;
}
/* a two-line text as one sentence: two spaces are a comma, ">" is "TO" ("KB CV > CUTOFF": from KB CV to the cutoff).
 * The line break "|": a space when the first line runs on (it ends with ":", "?", "," or ">"); a stop when the second
 * line is a new "what: does" ("EFFECTS OFF|REC: ON AGAIN", "KEY TRACKING: SEQ|KB CV > CUTOFF") or the first is a list
 * ("F3 INIT  G3 RANDOM|..."); else join: ", " for a message ("MEMORY RESET, RESTARTING"), " " for a hint ("THE EG
 * OPENS THE VCA") */
static void sp_num(uint32_t v);
/* a note value in words, as the reader page said it (here, so the FM-1's voice says the same): "1/8D" "DOTTED
 * EIGHTH", "1/4T" "QUARTER TRIPLET", "1/1" "WHOLE", "2/1" "2 BARS" */
static void sp_div(const char *d)
{
    static const char *const NM[7] = {"WHOLE", "HALF", "QUARTER", "EIGHTH", "SIXTEENTH", "THIRTY-SECOND", "SIXTY-FOURTH"};
    uint32_t a = 0, b = 0, i = 0, k = 0, n = slen(d);
    char last = n ? d[n - 1u] : 0;
    while (d[i] >= '0' && d[i] <= '9') a = a * 10u + (uint32_t)(d[i++] - '0');
    if (d[i] != '/') { sp_cat(d); return; }
    for (i++; d[i] >= '0' && d[i] <= '9'; i++) b = b * 10u + (uint32_t)(d[i] - '0');
    if (b == 1u) { if (a == 1u) sp_cat("WHOLE"); else { sp_num(a); sp_cat(" BARS"); } return; }
    while (k < 6u && (1u << k) < b) k++;
    if (last == 'D') sp_cat("DOTTED ");
    sp_cat(NM[k]);
    if (last == 'T') sp_cat(" TRIPLET");
}
static void sp_line(const char *s, const char *join)
{
    const char *s0 = s, *bar = s, *end;
    int list = 0;
    while (*bar && *bar != '|') { if (bar[0] == ' ' && bar[1] == ' ') list = 1; bar++; }
    for (end = bar; *end; end++) ;
    for (; *s && sp_n + 3u < ANN_MAX; s++) {
        char c = *s;
        if (c == '|') {
            const char *q = s;
            while (q > s0 && q[-1] == ' ') q--;   /* (the first line's last mark) */
            while (sp_n && sp_buf[sp_n - 1u] == ' ') sp_n--;
            sp_buf[sp_n] = 0;
            if (q > s0 && (q[-1] == ':' || q[-1] == '?' || q[-1] == ',' || q[-1] == '>' || q[-1] == '.')) sp_cat(" ");
            else if (list || sp_has(s + 1, end, ':') || (sp_has(s + 1, end, '>') && sp_has(s0, s, ':'))) sp_cat(". ");
            else sp_cat(join);
            continue;
        } else if (c == '>') {
            sp_cat("TO");
            continue;
        } else if (c == ' ' && s[1] == ' ') {
            sp_buf[sp_n++] = ','; sp_buf[sp_n++] = ' ';
            while (s[1] == ' ') s++;
        } else sp_buf[sp_n++] = c;
    }
    sp_buf[sp_n] = 0;
}
/* (0.6, the user) the screen's short words said in full by FMav-1 itself, not only by its words file: a reader without
 * the file (offline, or before it is published) says them right too. Screen -> said, whole words, case as written,
 * the longer of two that start alike first */
static const char *const SP_WORDS[][2] = {
    {"S+H (VCO)", "S H NORMALIZED TO V C O"}, {"ONE (-5)", "ONE NORMALIZED TO MINUS FIVE"},
    {"ATTN (+5)", "ATTENUATOR NORMALIZED TO PLUS FIVE"}, {"OFF (REC)", "OFF BY THE RECORD BUTTON"}, {"SEQ", "SEQUENCE"}, {"RESO", "RESONANCE"},
    {"1V/OCT", "1 VOLT PER OCTAVE"}, {"KB SCALE", "KEYBOARD SCALE"}, {"KB CV", "KEYBOARD C V"}, {"KB", "KEYBOARD"},
    {"CUT FINE", "CUTOFF FINE"}, {"TIME L", "TIME LEFT"}, {"TIME R", "TIME RIGHT"}, {"MIDI CH", "MIDI CHANNEL"},
    {"KEY VEL", "KEY VELOCITY"}, {"LIGHTS", "LIGHTS BRIGHTNESS"}, {"UP/DN", "UP AND DOWN"}, {"DN/UP", "DOWN AND UP"},
    {"PRIO", "LEGATO PRIORITY"}, {"RETRIG", "RETRIGGER"}, {"OCT+", "OCTAVE UP"}, {"OCT-", "OCTAVE DOWN"},
    {"OCT", "OCTAVE"}, {"ONE+TWO", "ONE PLUS TWO"}, {"S+H", "S H"}, {"ATTN", "ATTENUATOR"}, {"AMT", "AMOUNT"},
    {"LVL", "LEVEL"}, {"MOD", "MODULATION"}, {"EG ADSR", "ENVELOPE"}, {"EG", "ENVELOPE"}, {"ADSR", "A D S R"}, {"VCA CV", "V C A C V"},
    {"VCO", "V C O"}, {"VCF", "V C F"}, {"VCA", "V C A"}, {"LFO", "L F O"}, {"PWM", "P W M"}, {"CV", "C V"},
    {"VELO", "VELOCITY"}, {"MODWHL", "MOD WHEEL"}, {"AFTERTC", "AFTERTOUCH"}, {"TRI", "TRIANGLE"}, {"USB", "U S B"},
    {"TRS", "T R S"}, {"FM", "F M"}, {"UFO", "U F O"}, {"PERC", "PERCUSSION"}, {"5TH", "FIFTH"}, {"2X", "2 TIMES"},
    {"50/50", "50 50"}, {"CC", "C C"},
    {"ARP", "ARPEGGIATOR"}, {"BEND", "PITCH BEND"}, {"ANALOG", "ANALOG EFFECT"}, {"TUNE", "GLOBAL TUNE"},
    {"GLIDE", "PORTAMENTO"}, {"SYNC", "SYNCHRONIZATION"},   /* (the user) */ {"kHz", "KILOHERTZ"}, {"Hz", "HERTZ"}, {"ms", "MILLISECONDS"}, {"st", "SEMITONES"},
    {"s", "SECONDS"}, {"c", "CENTS"},
};
static char sp_said[ANN_MAX + 1];
static int sp_edge(char c) { return !c || c == ' ' || c == ',' || c == '.' || c == ':' || c == '?' || c == '(' || c == ')'; }
static const char *sp_out(void)                 /* sp_buf with SP_WORDS said in full: what is sent */
{
    uint32_t i = 0, o = 0, w, n = 0;
    while (sp_buf[i] && o < ANN_MAX) {
        const char *rep = 0;
        if (!i || sp_edge(sp_buf[i - 1u]))
            for (w = 0; w < sizeof SP_WORDS / sizeof SP_WORDS[0]; w++) {
                const char *k = SP_WORDS[w][0];
                for (n = 0; k[n] && sp_buf[i + n] == k[n]; n++) ;
                if (!k[n] && sp_edge(sp_buf[i + n])) { rep = SP_WORDS[w][1]; break; }
            }
        if (rep) { while (*rep && o < ANN_MAX) sp_said[o++] = *rep++; i += n; continue; }
        {                                       /* (the reader page's rules, here: "1-4" "1 TO 4"; a sign before a
                                                 * number "PLUS 1", "MINUS 12") */
            char c = sp_buf[i], nx = sp_buf[i + 1u], pv = i ? sp_buf[i - 1u] : ' ';
            int nd = nx >= '0' && nx <= '9', pd = pv >= '0' && pv <= '9';
            const char *w = c == '-' && pd && nd ? " TO " : (c == '-' || c == '+') && nd && (pv == ' ' || pv == '(') ?
                (c == '-' ? "MINUS " : "PLUS ") : 0;
            if (w) { while (*w && o < ANN_MAX) sp_said[o++] = *w++; i++; continue; }
        }
        sp_said[o++] = sp_buf[i++];
    }
    sp_said[o] = 0;
    return sp_said;
}
/* (0.6) every message goes out here, the same text both ways: to fm1-reader (announce.c) and to the FM-1's own
 * voice (mfm_voice.h, set.voice: PLAY held 2 s), so what is heard on the computer and on the FM-1 never differ */
static int sp_on(void) { return ann_on() || set.voice; }
static void sp_send(const char *t, uint32_t flags)
{
    if (ann_on()) ann_say(t, 0, flags);
    if (set.voice) vo_say(t, flags);
}
static void ann_toast(const char *s, uint32_t flags)   /* say() / warn(): the message as one sentence */
{
    if (!sp_on()) return;
    sp_n = 0; sp_buf[0] = 0;
    sp_line(s, ", ");
    if (sp_in_enc) {                            /* (from a turn: ALGORITHM's track, TEMPO locked: once it stops) */
        const char *o = sp_out();
        memcpy(sp_msg, o, slen(o) + 1u); sp.msg_flags = flags; sp.msg_pend = 1;
        return;
    }
    sp_send(sp_out(), flags);
}

static void sp_name(int32_t id, const char *title)   /* a knob's name, on its page without the title's word */
{
    const char *lb = id == P_FOLD ? "WAVE FOLD" : plabel(id);   /* (the user: the folder's amount) */
    if (title) {                                /* (character by character: a name shorter than the title ends first) */
        uint32_t i = 0;
        while (title[i] && lb[i] == title[i]) i++;
        if (!title[i] && lb[i] == ' ') lb += i + 1u;
    }
    sp_cat(lb);
}
/* (0.6.2, the user) an input's name: "S+H (VCO)" says what it is normalized to only when its knob is on NO CABLE
 * (normal 1); with a cable in, in the patchbay's list, in a list of names: "S+H", "ONE", "ATTN" */
static void sp_jack_name(int32_t id, const char *title, int normal)
{
    uint32_t n0 = sp_n, i;
    sp_name(id, title);
    if (!normal)
        for (i = n0; i + 1u < sp_n; i++)
            if (sp_buf[i] == ' ' && sp_buf[i + 1u] == '(') { sp_n = i; sp_buf[sp_n] = 0; break; }
}
/* a knob as its page shows it: its name, then its value ("CUTOFF 612Hz"; REC held: "CUTOFF CC 74"). A jack (the user)
 * the other way round, as a cable runs: "LFO TO FOLD IN", "NO CABLE TO PWM", "NO CABLE TO S+H (VCO)" (0.6.2: a cable in,
 * "LFO TO S+H": the normal only with no cable), "NORMAL LFO TO S+H GATE". On its page, a
 * name that starts with the page's (title) says the rest: "LFO. RATE 3.26Hz", not "LFO. LFO RATE" */
/* (0.6.2, the user) a knob's value alone, as a knob turned says it ("612 HERTZ"). A jack's: the cable's output ("LFO"),
 * or with no cable "NO CABLE", "NO CABLE, NORMALIZED TO V C O" (an input whose name gives its normal), "NORMAL L F O"
 * (one whose normal is another output) */
static void sp_value(int32_t id, int32_t v, const char *title)
{
    char t[24];
    if (is_jack(id)) {
        const char *nm = JIN_NORMAL[(uint32_t)(id - P_JACK) % NJIN], *lb = plabel(id);
        uint32_t i, j = 0;
        if (v) { sp_cat(JSRC_NAME[(uint32_t)v % NJSRC]); return; }
        for (i = 0; lb[i] && lb[i] != '('; i++) ;
        if (lb[i]) {                            /* ("S+H (VCO)": "NO CABLE, NORMALIZED TO V C O") */
            for (i++; lb[i] && lb[i] != ')' && j + 1u < sizeof t; i++) t[j++] = lb[i];
            t[j] = 0;
            sp_cat("NO CABLE, NORMALIZED TO "); sp_cat(t);
        } else if (nm[0] == '(') {              /* ("NORMAL LFO", S+H GATE's) */
            for (i = 1; nm[i] && nm[i] != ')' && j + 1u < sizeof t; i++) t[j++] = nm[i];
            t[j] = 0;
            sp_cat("NORMAL "); sp_cat(t);
        } else sp_cat("NO CABLE");
        return;
    }
    if (id == P_LFO_RATE && live.v[P_LFO_SYNC] && title) { sp_cat("SYNCED"); return; }   /* (on its page: SYNC says the value) */
    if (id == P_LFO_RATE && live.v[P_LFO_SYNC]) { sp_cat("SYNC "); sp_div(DIV_NAME[(live.v[P_LFO_SYNC] - 1u) % NCLKDIV]); return; }
    if (id >= V_PAN1 && id < V_PAN1 + 4) {      /* (the screen: "C", "L64", "R20") */
        if (v == 64) { sp_cat("CENTRE"); return; }
        sp_cat(v < 64 ? "LEFT " : "RIGHT "); sp_num((uint32_t)(v < 64 ? 64 - v : v - 64));
        return;
    }
    if (id >= 0 && id < NPARAM && PDEF[id].kind == K_CTYPE) {   /* (the user: the chorus TYPE as numbers) */
        static const char *const CT[NCT] = {"1", "2", "1 PLUS 2", "FREE"};
        sp_cat(CT[(uint32_t)v % NCT]);
        return;
    }
    param_value(t, id, v);
    if (id >= 0 && id < NPARAM && (PDEF[id].kind == K_DDIV || PDEF[id].kind == K_ADIV || PDEF[id].kind == K_SYNC
                                   || PDEF[id].kind == K_CDIV)) { sp_div(t); return; }
    if (id >= 0 && id < NPARAM && (PDEF[id].kind == K_CUT || PDEF[id].kind == K_LHZ || PDEF[id].kind == K_HHZ)) {
        uint32_t n = slen(t);                   /* (the screen: "450", "1.2k": hertz) */
        if (n && t[n - 1u] == 'k') { t[n - 1u] = 0; sp_cat(t); sp_cat(" kHz"); } else { sp_cat(t); sp_cat(" Hz"); }
        return;
    }
    {                                           /* a unit after its number, apart: "1.2s" -> "1.2 s", "0c" -> "0 c"
                                                 * (not a note value: "1/8D") */
        uint32_t i, frac = 0;
        for (i = 0; t[i]; i++) frac |= t[i] == '/';
        for (i = 0; t[i]; i++) {
            if (i && !frac && t[i] >= 'A' && t[i - 1u] >= '0' && t[i - 1u] <= '9') sp_cat(" ");
            { char c[2] = {t[i], 0}; sp_cat(c); }
        }
    }
}
static void sp_knob(int32_t id, int32_t v, const char *title)
{
    if (is_jack(id)) {
        if (!v) {                               /* no cable: the normal it falls back on, if its name doesn't say it */
            const char *nm = JIN_NORMAL[(uint32_t)(id - P_JACK) % NJIN], *lb = plabel(id);
            char t[24];
            uint32_t i, named = 0;
            for (i = 0; lb[i]; i++) named |= lb[i] == '(';
            if (nm[0] == '(' && !named) {       /* ("NORMAL LFO TO S+H GATE") */
                sp_cat("NORMAL ");
                for (i = 1; nm[i] && nm[i] != ')' && i < sizeof t; i++) { t[i - 1u] = nm[i]; t[i] = 0; }
                sp_cat(t);
            } else sp_cat("NO CABLE");
        } else sp_cat(JSRC_NAME[(uint32_t)v % NJSRC]);
        sp_cat(" TO "); sp_jack_name(id, title, !v);
        return;
    }
    sp_name(id, title); sp_cat(" "); sp_value(id, v, title);
}
static const int8_t PB_ORDER[12] = {I_FOLD, I_1VOCT, I_PWM, I_ONE, I_LFORATE, I_CUTOFF, I_GATE, I_VCACV, I_TWO, I_SHIN,
                                    I_SHGATE, I_ATTN};   /* (IT_PATCH's INPUTS pages: the panel's order) */
static void sp_patchbay(void)                   /* the whole patchbay's cables, "LFO TO PWM" (REC held: the CCs) */
{
    uint32_t i, n = 0;
    for (i = 0; i < 12u; i++) {
        int32_t id = PJ(PB_ORDER[i]);
        uint32_t s = live.v[id];
        if (!s || !jack_ok((uint32_t)PB_ORDER[i], s)) continue;
        if (n++) sp_cat(", ");
        sp_cat(JSRC_NAME[s % NJSRC]); sp_cat(" TO "); sp_jack_name(id, 0, 0);   /* (0.6.2: "LFO TO S+H") */
    }
    if (!n) sp_cat("NO CABLES");
}
static int sp_switch(int32_t id)               /* a switch: "DELAY SWITCH" on a new page */
{
    return id >= 0 && id < NPARAM && (PDEF[id].kind == K_ONOFF || PDEF[id].kind == K_VCAMODE);
}
static void sp_title(void)                      /* the page's title: "1 OF 2. VCF LADDER", "ARPEGGIATOR UP" */
{
    const item_t *it = cur_item();
    const char *sub = page_sub();
    uint32_t i, at = 0, all = 0;
    for (i = 0; i < SEC[sec].n; i++) {         /* (the user) the page among all its button's, first: "3 OF 5" (alone:
                                                 * none) */
        if (i < sec_item[sec] % SEC[sec].n) at += item_of(sec, i)->npg;
        all += item_of(sec, i)->npg;
    }
    at += cur_pg() + 1u;
    if (all > 1u) { sp_num(at); sp_cat(" OF "); sp_num(all); sp_cat(". "); }
    if (on_patchbay()) sp_cat("PATCHBAY OVERVIEW");   /* (the user) */
    else sp_cat(sec == S_EDIT ? "OSCILLATOR" : sec == S_VCF ? "FILTER" : it->title);   /* (the user: the VCO's,
                                                 * the VCF's pages so, alone) */
    if (on_patchbay() || sec == S_FX || sec == S_ARP || sec == S_EDIT || sec == S_VCF) sub = 0;   /* (the user: an
                                                 * effect's or the arp's title without its ON / OFF or mode) */
    if (sub && sub[0]) { sp_cat(sec == S_LFO ? ", " : " "); sp_cat(sub); }   /* ("LFO, TRS CLOCK"; "VCF LADDER") */
}
/* the page's knobs: their names (values: every value; else only a switch's); REC held: their CCs. The whole
 * patchbay: its cables */
static void sp_knobs(int values)
{
    const item_t *it = cur_item();
    uint32_t k, any = 0;
    if (on_patchbay()) { sp_patchbay(); return; }   /* (its cables; REC held: its inputs' CCs) */
    for (k = 0; k < 4u; k++) {
        int32_t id = page_param(k);
        if (id < 0) continue;
        if (any++) sp_cat(", ");
        sp_num(k + 1u); sp_cat(" ");               /* (0.6, the user: the knob's number first, "1 CUTOFF, 2 RESONANCE") */
        if (values) {
            sp_knob(id, pget(id), it->title);
            /* (the user) a track whose meter shows (it sounds, through its level): PLAYING */
            if ((id >= V_LVL1 && id < V_LVL1 + (int32_t)NTRK && g_trk_h((uint32_t)(id - V_LVL1)))
                || (id >= V_PAN1 && id < V_PAN1 + (int32_t)NTRK && g_trk_h((uint32_t)(id - V_PAN1))))
                sp_cat(" PLAYING");
        }
        else if (sp_switch(id)) { sp_name(id, 0); sp_cat(" SWITCH"); }   /* (a switch: its whole name, "VCA MODE") */
        else if (is_jack(id)) sp_jack_name(id, it->title, 0);   /* (0.6.2: "S+H"; its normal with NO CABLE) */
        else { sp_name(id, it->title); }   /* ("RESONANCE"; the user: "DELAY
                                                 * SWITCH", its state in the title, "DELAY OFF") */
    }
}
static void sp_prog(uint32_t n, const uint8_t *name)   /* "PROGRAM 3 CLASSIC BASS" */
{
    uint32_t i, e = PATCH_NAME;
    char t[PATCH_NAME + 1];
    while (e && name[e - 1u] == ' ') e--;
    for (i = 0; i < e; i++) t[i] = (char)name[i];
    t[e] = 0;
    sp_cat("PROGRAM "); sp_num(n + 1u);
    if (e) { sp_cat(" "); sp_cat(t); }
}
static uint32_t sp_ctx(void)                    /* the page on the screen (a change: it is said) */
{
    return sec | (sec_item[sec] % 8u) << 4 | cur_pg() << 8 | (uint32_t)on_patchbay() << 13;   /* (0.6: not REC's CCs) */
}
static void sp_where(void)                      /* everything on the screen: the program, the page, its knobs' names */
{
    sp_n = 0;
    sp_prog(set.tprog[ctrk], live.v + PNAME_AT);
    if (edited[ctrk]) sp_cat(", EDITED");
    if (NTRK > 1u) { sp_cat(", TRACK "); sp_num(ctrk + 1u); }
    if (save_armed) { sp_cat(". SAVE TO "); sp_num(save_to % NPROG + 1u); sp_cat("?"); }
    sp_send(sp_out(), sp.where_soft ? 0 : ANN_NOW);   /* (soft: after "SCREEN READER ON") */
    sp.where_soft = 0;
    sp_n = 0; sp_title(); sp_send(sp_out(), 0);
    if (on_patchbay()) { sp_n = 0; sp_knobs(0); sp_send(sp_out(), 0); }   /* (the page: its title; its knobs: touched) */
}
/* (0.6, the user) a discrete knob, 25 values or fewer (a switch, a mode, a cable, a note value), says each one as it
 * comes ("LFO TO FOLD IN", "MODE UP"); turned on against an end (its value can't change: one a detent), its value
 * again, never MINIMUM / MAXIMUM (0.6.2, the user). A continuous knob (more values) says its end the moment it gets
 * there, with its value ("MAXIMUM, 19 KILOHERTZ"); pushed on against it: once, unless it is a new turn */
static int sp_discrete(int32_t id) { return is_jack(id) || pmax(id) + 1u <= 25u; }   /* (the values: 0 .. pmax; the
                                                 * user: 25 at most, so BEND's 0 .. 24 too) */
static uint8_t sp_touch_new;                    /* (0.6.2) the knob turned now was just touched (sp_knob_touch) */
static void sp_knob_change(int32_t id, int32_t was, uint32_t fresh)
{
    int32_t v = pget(id);
    uint32_t touched = sp_touch_new;
    sp_touch_new = 0;
    if (!sp_on()) return;
    sp_n = 0;
    if (sp_discrete(id)) {                      /* (0.6.2, the user) the value only; just touched: its name first,
                                                 * once ("OUTPUT U S B", then "MAIN") */
        if (touched) sp_knob(id, v, 0); else sp_value(id, v, 0);
        sp_send(sp_out(), ANN_NOW);
    } else {                                    /* (0.6.2, the user) "MAXIMUM, 19 KILOHERTZ": no name, said when it
                                                 * was touched; just touched, after it */
        if (!(v <= 0 || v >= (int32_t)pmax(id)) || (v == was && !fresh)) return;
        sp_cat(v <= 0 ? "MINIMUM, " : "MAXIMUM, "); sp_value(id, v, 0);
        sp_send(sp_out(), touched ? 0u : ANN_NOW);
    }
    sp.after_page = 0;
}
/* (0.6, the user) a knob turned: the page's reading stops (it is said, or still to come): what is being said cut
 * off, both ways */
static void sp_hush(void)
{
    if (!sp_on()) return;
    sp.ctx_pend = 0;
    if (ann_on()) ann_say(" ", 0, ANN_NOW);
    if (set.voice) { vo_begin(1); vo_end(); }
}
/* (0.6, the user) a knob touched, the first on this page or another than the last: its name at once ("L F O RATE";
 * a knob of few values: its value says it, with its name) */
static int32_t sp_touch_k = -1;                 /* (the knob touched last on this page; -1: none yet) */
static void sp_knob_touch(uint32_t k, int32_t id)
{
    sp_touch_new = 0;
    if (!sp_on() || (int32_t)k == sp_touch_k) return;
    sp_touch_k = (int32_t)k;
    sp_touch_new = 1;
    if (sp_discrete(id)) return;               /* (its name with its first value: sp_knob_change) */
    sp_n = 0; sp_name(id, 0);
    sp_send(sp_out(), ANN_NOW);
    sp.after_page = 0;
}
static void sp_tell(const char *s)              /* (0.6) a text said only (not on the screen), at once */
{
    if (!sp_on()) return;
    sp_n = 0; sp_cat(s);
    sp_send(sp_out(), ANN_NOW);
}
static void speak(void)
{
    uint32_t now = fm1_ms, c, cut = ANN_NOW;
    int where = ann_where();                    /* (asked for, or just started listening) */
    if (!sp_on()) { sp.started = 0; sp.msg_pend = 0; return; }
    if (!sp.started || where) {
        sp.started = 1; sp.ctx = sp_ctx(); sp.knob_t = knob_t; sp.trk = ctrk; sp.prog = set.tprog[ctrk];
        sp.save_armed = save_armed; sp.save_to = save_to;
        sp.ctx_pend = sp.knob_pend = sp.prog_pend = sp.save_pend = sp.msg_pend = 0;
        sp_where();
        return;
    }
    /* what changed: noted now, said once nothing turns (SP_SETTLE_MS) */
    if (sp.save_armed && !save_armed) sp.prog = set.tprog[ctrk];   /* (saved: "SAVED" says it, the slot is the program) */
    if (ctrk != sp.trk) { sp.trk = ctrk; sp.prog = set.tprog[ctrk]; sp.prog_pend = 0; }   /* (its message says it) */
    else if (set.tprog[ctrk] != sp.prog) { sp.prog = set.tprog[ctrk]; sp.prog_pend = 1; }   /* PRESETS, MIDI */
    if (save_armed != sp.save_armed || save_to != sp.save_to) {   /* SAVE pressed: PRESETS picks the slot */
        sp.save_pend = save_armed && sp.save_armed;
        sp.save_armed = save_armed; sp.save_to = save_to;
    }
    c = sp_ctx();
    if (c != sp.ctx || sp_reread || sp_values) {   /* a new page, its button again (the user: said again), or held
                                                 * 1 s (with the values) */
        sp.ctx = c; sp_reread = 0; sp_touch_k = -1;   /* (a page entered: its knobs new again) */
        sp.ctx_t = now - (sp_values ? 200u : 0u); sp.ctx_pend = 1; sp.ctx_values = (uint8_t)sp_values; sp_values = 0;
        sp.knob_pend = 0; sp.knob_t = knob_t;
    }
    if (knob_t != sp.knob_t) { sp.knob_t = knob_t; sp.knob_pend = an_focus >= 0; sp.knob_rec |= (uint8_t)sp_turn_rec; }
    sp_turn_rec = 0;
    if (now - sp_turn_t < SP_SETTLE_MS) return; /* (still turning: nothing yet) */
    if (sp.msg_pend) { sp.msg_pend = 0; sp_send(sp_msg, sp.msg_flags); cut = 0; }
    if (sp.ctx_pend) {                          /* a new page: its title, then its knobs' names (a pause between) */
        if (now - sp.ctx_t < 200u) return;      /* (a button's page: flipping through settles first) */
        sp.ctx_pend = 0;
        sp_n = 0; sp_title(); sp_send(sp_out(), cut);
        if (sp.ctx_values || on_patchbay()) {   /* (the user: a page its title; held, all its knobs with values; the
                                                 * patchbay's overview its cables) */
            sp_n = 0; sp_knobs(sp.ctx_values); if (sp_n) sp_send(sp_out(), 0);
        }
        sp.after_page = 1;                      /* (what follows waits for the page, it doesn't cut it off) */
        return;
    }
    if (sp.prog_pend) { sp.prog_pend = 0; sp_n = 0; sp_prog(sp.prog, live.v + PNAME_AT); sp_send(sp_out(), sp.after_page ? 0 : cut); cut = 0; sp.after_page = 0; }
    if (sp.save_pend && save_armed) {
        sp.save_pend = 0;
        sp_n = 0; sp_cat("SAVE TO "); sp_num(save_to % NPROG + 1u); sp_cat("?");
        sp_send(sp_out(), cut); cut = 0;
    }
    if (sp.knob_pend) {                         /* a knob set: its name and the value it was left on, only if turned
                                                 * with REC held (the user; a knob of few values says each: above) */
        int32_t id = page_param((uint32_t)an_focus);
        sp.knob_pend = 0;
        if (id >= 0 && sp.knob_rec && !sp_discrete(id)) {
            sp_n = 0; sp_knob(id, pget(id), 0); sp_send(sp_out(), sp.after_page ? 0 : cut); sp.after_page = 0;
        }
        sp.knob_rec = 0;
    }
}

#ifdef OBV_HOST
/* "speakdump" (tests/host.c): every page as it is said, every value of every knob on it, REC held's CCs, the
 * programs' names, the hints, as KIND<TAB>text, for fm1-reader's audit (tests/sr_audit.sh) */
static void speak_dump(void)
{
    uint32_t s, i, p, k, sv = sec, si[NSEC], sync = live.v[P_LFO_SYNC], ccv = cc_view;
    static uint32_t ip[NSEC][8];
    patch_t pt;
    memcpy(si, sec_item, sizeof si); memcpy(ip, item_pg, sizeof ip);
    cc_view = 0;
    {
        for (s = 0; s < NSEC; s++)
            for (i = 0; i < SEC[s].n; i++)
                for (p = 0; p < item_of(s, i)->npg; p++) {
                    sec = s; sec_item[s] = i; item_pg[s][i % 8u] = p;
                    sp_n = 0; sp_title(); printf("SCREEN\t%s\n", sp_out());
                    sp_n = 0; sp_knobs(0); if (sp_n) printf("SCREEN\t%s\n", sp_out());
                    sp_n = 0; sp_knobs(1); if (sp_n) printf("SCREEN\t%s\n", sp_out());
                    for (k = 0; k < 4u; k++) {
                        int32_t id = page_param(k), v;
                        if (id < 0) continue;
                        if (is_jack(id)) {          /* (a jack: the choices its knob steps through, JORDER) */
                            for (v = 0; v < (int32_t)NJORDER; v++)
                                if (jack_ok((uint32_t)(id - P_JACK), JORDER[v])) { sp_n = 0; sp_knob(id, JORDER[v], 0); printf("KNOB\t%s\n", sp_out()); }
                        } else
                            for (v = 0; v <= (int32_t)pmax(id); v++) { sp_n = 0; sp_knob(id, v, 0); printf("KNOB\t%s\n", sp_out()); }
                        if (id == P_LFO_RATE)   /* (SYNC on: the note values) */
                            for (live.v[P_LFO_SYNC] = 1; live.v[P_LFO_SYNC] <= NCLKDIV; live.v[P_LFO_SYNC]++) {
                                sp_n = 0; sp_knob(id, 0, 0); printf("KNOB\t%s\n", sp_out());
                            }
                        live.v[P_LFO_SYNC] = (uint8_t)sync;
                    }
                }
    }
    cc_view = ccv; sec = sv; memcpy(sec_item, si, sizeof si); memcpy(item_pg, ip, sizeof ip);
    for (i = 0; i < NPROG; i++) { prog_get(i, &pt); sp_n = 0; sp_prog(i, pt.v + PNAME_AT); printf("SCREEN\t%s\n", sp_out()); }
    fflush(stdout);
}
#endif
