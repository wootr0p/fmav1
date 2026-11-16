# FMav-1 0.5.1 per M-VAVE FM-1

Un **sintetizzatore monofonico semi-modulare** dentro l'FM-1, ricostruito controllo per controllo e presa per presa dal
manuale d'uso del suo strumento di riferimento: un **VCO** (dente di sega → impulso, PULSE WIDTH, PWM), un **filtro
ladder a transistor a 4 poli** che auto-oscilla, l'**LFO** (0,165-550 Hz, triangolo → quadra, anche **a tempo**),
l'**EG ADSR**, il **VCA** con l'interruttore **VCA MODE**, la **tastiera** (priorità alla nota più bassa, legato, GLIDE,
KB SCALE), il **wave folder**, il **Sample + Hold**, il **mixer ONE+TWO**, l'**attenuatore**, il **rumore bianco** e il
**patchbay** con le sue **normalizzazioni**. Ogni manopola gira e suona come quella del riferimento (tarata sulle
sue registrazioni). Nessun sequencer: si suona dal Digitakt via MIDI TRS (o USB, o dai tasti).

**Quattro tracce** stile Felucca, ognuna col suo suono, sui canali MIDI N, N+1, N+2, N+3. **Chorus, delay e riverbero**
in comune. **90 programmi di fabbrica**, dal 91 al 99 INIT.

Identità del firmware: `FM-1_3628` (F-M-A-V sulla tastiera del telefono). SHA-256 in `SHA256.txt`.

> **Avvertenza.** FMav-1 è software libero, fornito **così com'è, senza nessuna garanzia**. Installarlo e usarlo è a
> **tuo rischio**: gli autori non rispondono di danni all'FM-1 (compreso un FM-1 che non si avvia più, "brickato"), ad
> altri apparecchi, alle orecchie o ai dati. Non è un prodotto di M-VAVE né di nessun costruttore di sintetizzatori, e
> non ne contiene codice, suoni o grafica. Vedi `LICENSE` (GPL-3.0) e `DISCLAIMER.md`.

## Novità della 0.5.1

- **KEYBOARD passa su SELECT premuto**, come seconda pagina dopo UTL. HOME ora ha MIXER (i LEVEL, poi i PAN), VOICE
  e GLOBAL.
- **Una sorgente nuova nel patchbay: GATE**, +5 V mentre una nota è tenuta (dai tasti dell'FM-1 o via MIDI, anche
  le note dell'arpeggiatore), 0 V quando la lasci. Per esempio GATE → CUTOFF apre il filtro solo mentre suoni.
- **PRESS si chiama AFTERTC** (l'aftertouch MIDI).
- **I cavi delle sorgenti esterne** (VELO, MODWHL, AFTERTC, GATE) hanno tutti lo stesso colore, cachi, per
  distinguerli dalle uscite del pannello; due che entrano nella stessa colonna non si sovrappongono più.

## Novità della 0.5

Ogni manopola tarata sullo strumento di riferimento (le tue registrazioni, l'oscilloscopio, le foto del pannello), i 90
programmi rifatti dall'INIT in modo da poterli rifare sul riferimento, il pannello riorganizzato.

### Il suono, tarato sullo strumento di riferimento

- **Le manopole girano come quelle del riferimento**: da **ore 7 a ore 5** (300°, prima 270°), con **11 tacche sulle
  ore**. Una manopola che sul pannello vero sta alle 2 si mette qui sulla tacca delle 2.
- **Un filtro ladder più fedele**: ogni stadio satura anche il proprio stato, come le coppie di transistor del circuito
  vero (Huovilainen 2004, D'Angelo & Välimäki 2013; circa l'8% di CPU in più nel caso peggiore). Tarato sulle
  registrazioni del riferimento (il suo INIT, un Do2 a dente di sega, cinque impostazioni di CUTOFF e RESONANCE): il
  taglio a circa 24, 289 e 4800 Hz a ore 9, mezzogiorno e ore 3, il fischio a 2,12 volte il taglio, la RESONANCE che
  comincia a fischiare a metà tra le 3 e le 4. Verificato sull'FM-1 stesso: armoniche entro 0,7 dB a filtro aperto e
  1,3 dB con RESONANCE a metà, il fischio a 589 Hz (583 sul riferimento). CUTOFF a ore 10:28 fa 95 Hz.
- **Il filtro auto-oscilla da solo**, come quello vero: con la RESONANCE oltre la soglia fischia anche senza niente in
  ingresso. Sotto la soglia non si sente niente.
- **CUTOFF tutto a sinistra è muto, senza click.**
- **L'EG**: ATTACK da 0,8 ms a 2,5 s con la curva di un condensatore e che a ogni nuovo gate riparte da zero, come sul
  riferimento; DECAY da 3 ms a 16 s e RELEASE da 3 ms a 18 s, nei secondi del riferimento; a mezzogiorno ATTACK
  153 ms, DECAY 668 ms, RELEASE 627 ms; a ore 9 ATTACK 44 ms, DECAY 437 ms, RELEASE 330 ms; SUSTAIN al 13 % a ore 9,
  al 48 % a mezzogiorno, lineare sopra.
- **VCF MOD AMT** segue |x|^1,5 (prima il quadrato: a ore 2:48 apre 3,3 ottave).
- **L'LFO**: da 0,165 a 550 Hz, 4,7 Hz a metà corsa; tra triangolo e quadra come il riferimento (la quadra è alta
  mentre il triangolo sale: a metà corsa salta sui picchi), entrambi asimmetrici 58 / 42.
- **La larghezza dell'impulso va da 0 a 50 %** (prima da 1 a 99 %): PULSE WIDTH tutta a destra è sempre un'onda
  quadra, e se la modulazione la porta sotto lo 0 % l'impulso sparisce, come sul riferimento.
- **S+H IN senza cavo torna sul VCO**, come dice il pannello di riferimento (S+H (VCO)). I programmi salvati con la 0.4
  e le prime 0.5, che campionavano il rumore, ricevono da soli il cavo NOISE → S+H IN e suonano come prima.
- **Tolta l'uscita CLOCK** del patchbay (sul riferimento non c'è). LFO SYNC, l'arpeggiatore e il delay seguono il clock
  come prima. Un cavo salvato dal CLOCK sparisce; le CC 84 e 85 non fanno più niente.

### I programmi

- **I 90 programmi rifatti dall'INIT**: ognuno cambia solo le manopole che servono al suo suono (le altre restano come
  nell'INIT) e usa solo ciò che ha il pannello di riferimento, più gli effetti: niente rumore bianco, cavi VELO /
  MODWHL / AFTERTC / GATE, FINE, ANALOG, arpeggiatore o LFO SYNC. Così si rifanno sul riferimento partendo dal suo INIT e
  girando solo quelle manopole (l'elenco, nell'ordine del pannello, è in `firmware/src/mfm_patches.h`). Suonano come
  nella 0.4.1, con ogni manopola portata sulle curve nuove a parità di Hz, tempi e livelli. I programmi di rumore (37
  NOISE HIT, 69 CLOCK HATS, 79 WIND, 81 SNARE, 82 HI-HAT, 83 CLAP) fanno il rumore con il VCO e l'LFO che si modulano
  a vicenda; i ritmici hanno l'LFO libero alla velocità che avevano a 120 BPM; 42 ARP PLUCK e 70 SYNC ARP non
  arpeggiano più, 59 WHEEL LEAD non usa più la rotella.
- **Un ordine nuovo**: 01 BRASS LEAD, 02 CLASSIC LEAD, 03 CLASSIC BASS, 04 PWM LEAD, 05 FOLDED TRI, 06 DOTTED LEAD,
  poi tutti gli altri nell'ordine di prima. Le tracce partono da 01, 11, 21, 31 (prima accensione, azzeramento).
- **CLASSIC BASS** con le manopole della foto del riferimento; **BRASS LEAD** con l'EG e l'LFO RATE che hai scelto.
- **INIT** (91-99, SAVE + F3) come quello del riferimento: dente di sega, PULSE WIDTH al 50%, VCO MOD MIX e VCF MOD MIX
  su EG, LFO triangolare, CUTOFF tutto aperto, RESONANCE a 0, ATTACK, DECAY e RELEASE a 0, SUSTAIN al massimo.

### Il pannello

- **I pulsanti riorganizzati**: **HOME** apre il MIXER (i LEVEL, poi i PAN) e dopo KEYBOARD, VOICE e GLOBAL; **EDIT**
  il VCO; **GLO** il VCF. Le pagine PERFORM non ci sono più.
- **La navigazione è sempre uguale** (per chi non vede lo schermo e per la memoria muscolare): un pulsante apre la sua
  sezione sempre dalla prima pagina, premuto di nuovo va alla pagina dopo (**premuto N volte = pagina N**); SELECT si
  ferma alla prima e all'ultima pagina; EDIT e SAVE funzionano anche se li tieni premuti a lungo. La mappa delle
  pagine è in "I comandi".
- **FX: prima il DELAY**, poi il REVERB, infine il CHORUS.
- **CUT FINE** (GLO, terza manopola del VCF) e **RATE FINE** (LFO, quarta manopola): ±2 semitoni di regolazione fine
  (CC 87 / NRPN 70, CC 88 / NRPN 71). Non sono sul pannello di riferimento.
- **ARP premuto di nuovo** non accende e non spegne più l'arpeggiatore: lo fa la manopola MODE.
- **Azzerare la memoria**: tieni premuto **SAVE** e premi **G5** (si accende): compare "RESET ALL MEMORY?". **OCT+**
  (lampeggia) conferma; qualunque altro tasto, o 8 secondi, annulla. Cancella tutti i programmi salvati e tutte le
  impostazioni, poi **l'FM-1 si riavvia e riparte come appena installato**.
- **I LED**: HOLD accende PLAY verde; il tasto LFO non lampeggia più con l'LFO (il suo LED è sullo schermo); corretto
  il LED di REC (effetti accesi), che restava spento all'accensione.
- **I cavi del patchbay si curvano sempre**: un cavo tra due prese della stessa colonna non passa più dritto sopra le
  altre (i lati si alternano) e due cavi paralleli non si sovrappongono mai.
- **La versione nella schermata di avvio.**

## Novità della 0.4.1

- **La sorgente del tempo**: **GLO → GLOBAL → CLOCK** (manopola 3):
  - **INTERNAL** (di serie): tutto segue la manopola **TEMPO**; qualunque clock MIDI, START e STOP vengono ignorati;
  - **USB**: il clock e START / STOP / CONTINUE solo dall'USB;
  - **TRS**: il clock e START / STOP / CONTINUE solo dal MIDI TRS.

  L'altra porta viene ignorata: con il Digitakt collegato sia in USB sia in TRS il clock non arriva più doppio.
- Con **USB** o **TRS** la manopola **TEMPO è bloccata** (grigia; se la giri te lo dice) e mostra **il tempo letto dal
  sync** (es. **TRS 125**; **---** finché non l'ha misurato). Il tempo si misura già dopo **un quarto**.
- Con **USB** o **TRS** **l'arpeggiatore segue solo quel clock**: i passi cadono sui tick del clock. Con il clock fermo
  (o non ancora arrivato) suona la prima nota e aspetta: mai al passo della manopola TEMPO.
- In GLOBAL le manopole ora sono **MIDI CH, TUNE, CLOCK, TEMPO**, poi (seconda pagina) **KEY VEL, LIGHTS**. La pagina
  LFO (con SYNC acceso) scrive **USB CLOCK** / **TRS CLOCK**, **(STOPPED)** quando è fermo, **NO CLOCK** se non arriva niente.
- Dopo l'aggiornamento dalla 0.4 la sorgente parte da **INTERNAL**: per suonare a tempo col Digitakt scegli **TRS**
  (o USB) una volta, resta salvata. Il suono non cambia.

## Novità della 0.4

- **Il rumore bianco (NOISE)**: un generatore per traccia.
  - Il suo livello, **NOISE**, è una manopola che lo **mescola nell'ingresso del VCF**, insieme al VCO (o al folder).
  - È anche **un'uscita del patchbay**: va in qualunque presa (S+H, CUTOFF, PWM, 1V/OCT, FOLD IN…).
  - Con **DUMMY in FOLD IN** il VCF riceve solo il rumore: rullanti, hi-hat, vento, onde.
- **Il S+H campiona il rumore bianco** quando S+H IN è senza cavo: tensioni davvero casuali, il S+H classico. Il
  campionamento del VCO (sequenze legate all'intonazione) resta con un cavo S+H IN ← VCO, e i programmi salvati con le
  versioni precedenti che usano il S+H lo ricevono da soli, così suonano come prima.
- **Nessuna manopola doppia**, a parte la pagina PERFORM (HOME), che resta la pagina veloce:
  - **i cavi si collegano solo da SEQ** (PATCHBAY + INPUTS): le pagine INPUTS dei moduli non ci sono più;
  - **UTL** (SELECT premuto) è **una pagina sola**: FOLD, ONE LVL, ATTENUATOR, NOISE;
  - **TEMPO** sta solo in GLOBAL (non più anche in LFO → CLOCK);
  - la pagina S+H di LFO non c'è più: le sue prese sono in SEQ.
- **Il tasto della sezione premuto di nuovo va alla pagina dopo**, come girare SELECT di uno scatto. Fa eccezione ARP,
  che premuto di nuovo accende e spegne l'arpeggiatore (dalla 0.5 non più: lo fa la manopola MODE).
- **I preset rifatti con il rumore** (18 su 90; gli altri 72 suonano identici alla 0.3.1):
  - su rumore vero: 34 NOISE HIT, 69 CLOCK HATS, 79 WIND, 81 SNARE, 82 HI-HAT, 83 CLAP;
  - con il S+H casuale: 21 S+H FILTER, 22 S+H MELODY, 38 BLIPS, 63 CLOCK S+H, 64 CLOCK MELODY;
  - con un soffio di rumore: 32 TOM, 52 WHISTLE, 54 SOFT LEAD, 77 PAD SWEEP, 78 VOX, 90 DARK DRONE;
  - con lo sweep di rumore: 87 RISER.
- Le CC dei cavi: con una sorgente in più cambiano i valori da mandare per ogni uscita (`CC-NRPN.md`). NOISE ha la CC 86.

## Novità della 0.3.1

- **DUMMY è in fondo all'elenco** di ogni ingresso (dopo le sorgenti MIDI), non più subito dopo `---`.
- **Canale MIDI 1 di serie**: alla prima accensione (o dopo un altro firmware, che azzera le impostazioni globali) la
  traccia 1 è sul canale 1, la 2 sul 2, la 3 sul 3, la 4 sul 4. Un canale già impostato resta quello.
- **SEQ ha due voci**: **PATCHBAY** (tutti i cavi) e **INPUTS** (gli ingressi sulle manopole, 3 pagine). **SEQ premuto
  di nuovo passa dall'una all'altra**; SELECT girato le scorre tutte, come sempre.
- **Le combinazioni si illuminano**: con **SAVE tenuto** si accendono i tasti **F3, G3, A3** della tastiera (INIT,
  RANDOM, MUTATE); con **EDIT tenuto** si accendono **OCT- e OCT+** (undo / redo), se c'è qualcosa da annullare.
- Il suono non cambia: i 90 programmi sono identici alla 0.3 (provato campione per campione).

## Novità della 0.3

- **REC accende e spegne tutti gli effetti insieme.** REC acceso = gli effetti suonano come li hai impostati; REC spento
  = tutti spenti; riacceso tornano **identici a prima** (le impostazioni dei suoni non cambiano: è un interruttore
  generale, ricordato anche da spento). In alto compare `FX OFF`, sulle pagine FX `OFF (REC)`.
- **Il clock MIDI (TRS o USB) entra nel synth**:
  - **LFO SYNC** (LFO, manopola 4): FREE oppure un valore di nota, da 8/1 a 1/32T. A tempo l'LFO è **agganciato al
    clock**, anche di fase: lo START del Digitakt lo riporta all'inizio della battuta. Con un cavo su LFO RATE la
    tensione lo sposta attorno alla velocità a tempo.
  - **CLOCK**, una nuova uscita del patchbay: un'onda quadra 0/+5 V a un valore di nota (LFO → pagina CLOCK: CLOCK, GATE
    = la durata dell'impulso, e il TEMPO). Collegala dove vuoi: **S+H GATE** (S+H a tempo), **GATE** (l'EG ripetuto a
    tempo), **CUTOFF** (un trance gate), **VCA CV**, **PWM**… *(Tolta nella 0.5: non è sul pannello di
    riferimento.)*
  - **L'arpeggiatore** con un clock MIDI scatta sulla griglia del clock (la prima nota subito).
  - Senza clock MIDI tutto segue il **TEMPO** (GLO → GLOBAL). Il delay era già a tempo.
- **Il cavo DUMMY**: una spina infilata da sola, senza niente dall'altra parte. Come sul pannello vero, **toglie la
  normalizzazione** e dà 0 V. In fondo all'elenco di ogni ingresso (0.3.1). Per esempio: DUMMY in **FOLD IN** = il VCF
  non riceve niente (il filtro che auto-oscilla da solo, programma 52 WHISTLE); DUMMY in **ONE** = il mixer senza i
  -5 V; DUMMY in **S+H (VCO)** = il S+H fermo a 0 V.
- **Il MULT non c'è più** (1 ingresso e 2 uscite inutili: un'uscita va già a quante prese vuoi), e **VCA OUT non si
  collega** a nessun ingresso: è l'uscita audio (dove ci sono le cuffie). Sul patchbay il suo cavo esce dal pannello, e
  l'**animazione di avvio** è proprio il cavo che entra in VCA OUT. I programmi vecchi con un cavo dal MULT lo prendono
  direttamente dalla sorgente del MULT (stesso suono).
- **SEQ**: la prima pagina è **il patchbay intero** (tutti i cavi, a schermo pieno); le pagine dopo hanno **tutti gli
  ingressi sulle 4 manopole**, grandi, e sotto ognuno dice da dove arriva il cavo.
- **SELECT premuto apre UTL**: WAVE FOLDER, MIXER, ATTENUATOR (prima stavano in SEQ). **Nessuna pagina ha più
  "l'interruttore" su SELECT**: quello che faceva è diventato una manopola (VCA MODE, l'ON di chorus / delay / reverb,
  i cavi KB CV → CUTOFF e VCO → FOLD IN si scelgono sulle loro manopole).
- **90 programmi di fabbrica** (50 nuovi: bassi, lead, suoni a tempo, pad, tastiere, percussioni, droni). I primi 40
  suonano **identici** alla 0.2 (provato campione per campione); 01 e 11 ora si chiamano CLASSIC BASS e CLASSIC LEAD.
- **OCT- / OCT+ dicono l'ottava**: ±1 accesi fissi, ±2 lampeggio lento, ±3 lampeggio veloce, 0 spenti.
- **La spia dell'LFO** sullo schermo resta accesa fissa quando l'LFO va più veloce di quanto lo schermo possa mostrare
  (~8 Hz; il pulsante LFO sopra ~20 Hz): prima sembrava lampeggiare più piano.
- Corretto: la **manopola illuminata** mentre la giri non cancella più il bordo bianco del riquadro.
- Corretto: un cavo da **MODWHL, VELO, PRESS** sul patchbay arriva da fuori, senza la spina disegnata
  all'altra estremità.
- I programmi salvati con la 0.1 e la 0.2 vengono convertiti da soli; le impostazioni globali restano.

## Installarlo

1. Collega l'FM-1 al PC con un cavo USB dati e accendilo.
2. Da questa cartella:

   ```
   py fm1_install.py fmav1-0.5.1.fwsc --yes
   ```

   Per vedere cosa c'è installato: `py fm1_install.py --info`

- Funziona sopra le versioni 0.1-0.2, OberVave, Felucca, SLOOP e quasi tutti i loro fork. Se
  l'installer non riconosce il firmware attuale, indica la sua porta MIDI: `--port NomePorta`.
- **I programmi degli altri firmware non vengono toccati**: FMav-1 salva i suoi a 0xD8000 (dove li salvavano le
  versioni 0.1-0.2: vengono convertiti).
- Se l'FM-1 non si avvia più: spegnilo, tieni premuto **OCT-** e riaccendilo (modalità di emergenza), poi rilancia il
  comando.

## I comandi

**Un pulsante apre una sezione, sempre dalla sua prima pagina**; premuto di nuovo passa alla pagina dopo e dopo l'ultima
torna alla prima: **premuto N volte = pagina N**, qualunque pagina ci fosse prima. **SELECT girato scorre le pagine della
sezione e si ferma alla prima e all'ultima** (girato tutto a sinistra: sempre la prima). **Le 4 manopole** sono i 4
parametri sullo schermo (accelerano se giri veloce), **SELECT premuto apre UTL**. EDIT e SAVE agiscono quando li
rilasci, anche se li hai tenuti a lungo (se non li hai usati insieme a un altro tasto). Così **la stessa sequenza di
tasti porta sempre alla stessa pagina**: si naviga a memoria, anche senza guardare lo schermo. Ogni manopola sta in un
solo posto; i cavi si collegano da SEQ.

**La mappa delle pagine** (il pulsante premuto 1, 2, 3... volte; con 4 tracce):

- **HOME**: 1 MIXER (i LEVEL) · 2 MIXER (i PAN) · 3 VOICE · 4 GLOBAL (MIDI CH, TUNE, CLOCK, TEMPO) ·
  5 GLOBAL (KEY VEL, LIGHTS)
- **EDIT**: 1 VCO · 2 VCO MOD
- **ENV**: 1 EG · 2 VCA
- **LFO**: 1 LFO
- **FX**: 1-3 DELAY · 4-5 REVERB · 6-7 CHORUS
- **SEQ**: 1 PATCHBAY · 2 INPUTS 1-4 · 3 INPUTS 5-8 · 4 INPUTS 9-12
- **SELECT premuto**: 1 UTL · 2 KEYBOARD
- **ARP**: 1 ARPEGGIATORE
- **GLO**: 1 VCF · 2 VCF MOD

| Pulsante | Le pagine che SELECT scorre |
| --- | --- |
| **HOME** | **MIXER**: i LEVEL delle 4 tracce · i loro PAN → **VOICE**: BEND, ANALOG → **GLOBAL**: MIDI CH, TUNE, CLOCK, TEMPO · KEY VEL, LIGHTS (in alto a destra la CPU) |
| **EDIT** | **VCO**: PITCH, VCO WAVE, PULSE WIDTH, FINE · **MOD**: VCO MOD MIX, PITCH MOD AMT, PWM AMT |
| **ENV** | **EG**: ATTACK, DECAY, SUSTAIN, RELEASE → **VCA**: VOLUME, **VCA MODE** (EG / ON) |
| **LFO** | **LFO**: LFO RATE, LFO WAVE, **SYNC**, RATE FINE |
| **FX** | **DELAY**: ON, SEND, TIME L, TIME R · FEEDBACK, PING-PONG, WIDTH · LOW CUT, HIGH CUT → **REVERB**: ON, SEND, DECAY, DAMP · PRE-DELAY, DIFFUSION, WIDTH → **CHORUS**: ON, TYPE, SEND · RATE, DEPTH |
| **SEQ** | **PATCHBAY**: tutti i cavi → **INPUTS** 1-4: FOLD IN, 1V/OCT, PWM, ONE · 5-8: LFO RATE, CUTOFF, GATE, VCA CV · 9-12: TWO, S+H (VCO), S+H GATE, ATTN |
| **SELECT premuto** | **UTL**: FOLD, ONE LVL, ATTENUATOR, **NOISE** → **KEYBOARD**: KB SCALE, GLIDE, PRIO, RETRIG |
| **ARP** | L'arpeggiatore: MODE (OFF: spento), OCTAVES, RATE, GATE |
| **GLO** | **VCF**: CUTOFF, RESONANCE, CUT FINE · **MOD**: VCF MOD MIX, VCF MOD AMT |
| **ALGORITHM** | **La traccia** sul pannello (1-4): tastiera, pagine e PRESETS lavorano su quella |
| **PRESETS** | Il programma 01-99 della traccia (91-99 INIT) |
| **PLAY** | HOLD (le note restano; PLAY verde) |
| **REC** | **Tutti gli effetti spenti / accesi** (acceso: suonano) |
| **SAVE** | Premuto: `SAVE TO n?` (PRESETS sceglie lo slot), SAVE di nuovo: salvato (scritto quando c'è silenzio). **Tenuto + tasto**: F3 INIT, G3 suono casuale, A3 MUTATE, G5 azzera la memoria (OCT+ conferma) |
| **EDIT + OCT- / OCT+** | Undo / redo |
| **OCT- / OCT+** | Ottava della tastiera (entrambi: ottava 0); i loro LED la mostrano |

**Lo schermo, dall'alto:** "FMAV-1", il **LED dell'LFO** e le **lampade delle 4 tracce 1-2-3-4** (accese col suono,
contornata quella sul pannello) · il numero del programma nel colore della traccia (1 rossa, 2 gialla, 3 verde, 4 blu),
il nome (`*` = modificato), lo stato (VCA EG / ON, i cavi, CHO DLY REV o FX OFF, ARP, HOLD) · il riquadro del modulo con
la sua linguetta e un puntino per pagina · le 4 manopole · il grafico · la tastiera a 13 pulsanti con la nota che suona
e il suggerimento della pagina; i messaggi compaiono lì, in un riquadro bianco.

## Il patchbay

Sul pannello vero i cavi si infilano; qui **ogni presa d'ingresso è una manopola**: la giri e scegli da quale uscita
arriva il suo cavo. `---` = nessun cavo, e allora vale la **normalizzazione**, scritta tra parentesi; **DUMMY** = una
spina senza niente dall'altra parte (la normalizzazione tolta, 0 V).

| Ingresso | Senza cavo | Cosa fa il cavo |
| --- | --- | --- |
| **1V/OCT** | --- | si somma al pitch del VCO, 1 V = 1 ottava |
| **PWM** | --- | si somma alla larghezza d'impulso (4,5 % al volt) |
| **CUTOFF** | --- | si somma al cutoff, 1 V = 1 ottava (KB CV qui = il filtro segue la tastiera al 100 %) |
| **VCA CV** | --- | si somma all'EG nel VCA (0-8 V) |
| **LFO RATE** | --- | si somma alla velocità dell'LFO, 1 V = 1 ottava |
| **GATE** | --- | fa partire l'EG (sopra 3,5 V) insieme alla tastiera |
| **FOLD IN** | --- (folder fuori) | quello che entra passa nel wave folder e va al **VCF al posto del VCO** |
| **S+H (VCO)** | **(VCO)** | quello che il S+H campiona (senza cavo il VCO, come sul pannello di riferimento; 0.4: il rumore bianco) |
| **S+H GATE** | **(LFO)** | quando campiona (a ogni salita) |
| **ONE (-5)** | **(-5V)** | il canale 1 del mixer, col suo ONE LVL |
| **TWO** | --- | il canale 2 del mixer, a guadagno 1 |
| **ATTN (+5)** | **(+5V)** | quello che l'ATTENUATOR riduce (senza cavo: una tensione 0..+5 V) |

Le uscite: **KB CV**, **VCO**, **LFO**, **EG**, **S+H**, **ONE**, **ONE+TWO**, **ATTN**, il **NOISE** (rumore bianco),
le sorgenti esterne **VELO**, **MODWHL**, **AFTERTC** (l'aftertouch MIDI, 0..+5 V) e **GATE** (+5 V mentre una nota è
tenuta, dai tasti dell'FM-1 o via MIDI, anche le note dell'arpeggiatore; 0 V quando la lasci), con i cavi color cachi
che arrivano dall'alto, e **DUMMY**. **VCA OUT** è l'uscita audio. Una stessa uscita può andare a
quante prese vuoi; un ingresso non prende mai le uscite del suo modulo. Le tensioni: VCO, LFO e S+H ±5 V, EG 0-8 V, il
mixer satura a ±8 V, NOISE ±5 V.

**Qualche patch per cominciare:**

- **Wave folding**: SEQ → INPUTS, manopola 1 (FOLD IN) su **VCO**, poi SELECT premuto (UTL) e gira FOLD.
- **Il filtro che segue la tastiera**: SEQ → INPUTS 5-8, manopola 2 (CUTOFF) su **KB CV**. Con la RESONANCE al
  massimo il filtro diventa un oscillatore sinusoidale suonabile (con DUMMY in FOLD IN resta solo lui: 54 WHISTLE).
- **Rumore come sul riferimento** (senza NOISE): LFO RATE ← VCO, VCO MOD MIX tutto su LFO, PITCH MOD AMT e LFO RATE
  alti: il VCO e l'LFO si modulano a vicenda (programmi 37, 79, 81-83). Oppure il VCO che modula sé stesso dal mixer:
  ONE ← VCO, 1V/OCT ← ONE+TWO (69 CLOCK HATS).
- **Solo rumore bianco** (un'estensione, il riferimento non ce l'ha): FOLD IN su **DUMMY**, UTL → NOISE al massimo.
- **Note casuali**: il S+H campiona il VCO (senza cavo); 1V/OCT ← ATTN, ATTN ← S+H, ATTENUATOR basso (programma 25).
- **S+H sul filtro**: CUTOFF ← S+H (S+H GATE è già normalizzato sull'LFO; a tempo: LFO SYNC) (programma 64 CLOCK S+H).
- **Melodie casuali a tempo**: 1V/OCT ← ATTN, ATTN ← S+H, GATE ← LFO, LFO SYNC 1/16 e LFO WAVE quadra (programma 65;
  suona da solo).
- **Wobble a tempo**: VCF MOD MIX tutto su LFO, VCF MOD AMT alto, LFO SYNC 1/8 (programma 62).
- **L'LFO come secondo oscillatore**: LFO RATE ← KB CV, LFO RATE a 116 (unisono) o 122 (una quinta), ONE ← LFO,
  TWO ← VCO, FOLD IN ← ONE+TWO (programmi 20, 47, 58, 61).
- **Tremolo**: VCA CV ← LFO. **Auto-repeat**: GATE ← LFO (a tempo con LFO SYNC). **Drone**: VCA MODE ON (programmi 43, 89, 90).

## Gli effetti

**FX** apre DELAY, REVERB e CHORUS (in quest'ordine), ognuno con il suo **ON** sulla manopola 1; **REC** li spegne e riaccende tutti
insieme. Sono **in comune alle quattro tracce, come mandate**: ogni suono ci manda con il suo **SEND** (e il suo ON), e il
loro carattere (tempi, feedback, decay…) è quello del suono della prima traccia che li usa.

- **CHORUS**: il chorus a BBD dei polifonici classici, TYPE **I**, **II**, **I+II** (o FREE se giri RATE / DEPTH).
- **DELAY**: TIME L e TIME R a valori di nota del tempo (o del **clock del Digitakt**), FEEDBACK, PING-PONG, WIDTH, LOW CUT
  e HIGH CUT nelle ripetizioni.
- **REVERB**: un plate (Dattorro), DECAY, DAMP, PRE-DELAY, DIFFUSION, WIDTH.

## MIDI con il Digitakt

- **TRS MIDI IN** dell'FM-1 (adattatore TRS-DIN tipo A o B) o USB.
- **HOME → GLOBAL → MIDI CH = N** (di serie **1**): il canale N suona la **traccia 1**, N+1 la **2**, N+2 la **3**,
  N+3 la **4**, qualunque traccia sia sul pannello. Con **OMNI** tutto suona la traccia sul pannello.
- Note, pitch bend, aftertouch, mod wheel (CC 1), volume della traccia (CC 7), sustain (CC 64), program change, e **una
  CC per ogni parametro, cavi ed effetti compresi** (`CC-NRPN.md`). Con un **p-lock sulla CC di una presa** il Digitakt
  cambia il cavo a quel passo.
- **CC fisse**: 74 CUTOFF, 71 RESONANCE, 73 ATTACK, 72 RELEASE, 75 DECAY, 5 GLIDE, 93 CHORUS SEND, 92 DELAY SEND,
  91 REVERB SEND.
- **Clock**: attiva l'invio del clock sul Digitakt (SETTINGS → MIDI CONFIG → SYNC → CLOCK SEND, e TRANSPORT SEND per
  START / STOP) e sull'FM-1 scegli da dove arriva: **HOME → GLOBAL → CLOCK = TRS** (o **USB**; **INTERNAL** = la
  manopola TEMPO, il clock esterno ignorato). LFO SYNC, l'arpeggiatore e il delay lo seguono; START
  riallinea tutto all'inizio della battuta. Fermato il clock, l'arpeggiatore si ferma e aspetta il clock; LFO SYNC e il
  delay continuano al tempo misurato.

## I 90 programmi di fabbrica (91-99 INIT)

| | | | | |
| --- | --- | --- | --- | --- |
| 01 BRASS LEAD | 02 CLASSIC LEAD | 03 CLASSIC BASS | 04 PWM LEAD | 05 FOLDED TRI |
| 06 DOTTED LEAD | 07 SQUARE BASS | 08 ACID LINE | 09 RUBBER BASS | 10 FOLD BASS |
| 11 PWM BASS | 12 DIRTY BASS | 13 FM BASS | 14 SUB PLUCK | 15 WOW BASS |
| 16 SQUARE LEAD | 17 FOLD LEAD | 18 RESO LEAD | 19 GLIDE LEAD | 20 DUO LEAD |
| 21 FM LEAD | 22 WAH LEAD | 23 SCREAM LEAD | 24 S+H FILTER | 25 S+H MELODY |
| 26 WOBBLE | 27 TREMOLO | 28 RING TONE | 29 FOLD SWEEP | 30 AUTO REPEAT |
| 31 SLOW SWELL | 32 CROSS MOD | 33 KB SCALE 2X | 34 KICK | 35 TOM |
| 36 METAL | 37 NOISE HIT | 38 ZAP | 39 LASER | 40 SIREN |
| 41 BLIPS | 42 ARP PLUCK | 43 DRONE | 44 DEEP SUB | 45 PICK BASS |
| 46 GROWL BASS | 47 OCTAVE BASS | 48 BLIP BASS | 49 SQUARE ACID | 50 BOOM BASS |
| 51 HOT BASS | 52 SEQ BASS | 53 SOFT BASS | 54 WHISTLE | 55 HOLLOW LEAD |
| 56 SOFT LEAD | 57 BITE LEAD | 58 FIFTH LEAD | 59 WHEEL LEAD | 60 CHIP LEAD |
| 61 SUB LEAD | 62 SYNC WOBBLE | 63 TRANCE GATE | 64 CLOCK S+H | 65 CLOCK MELODY |
| 66 SYNC PWM | 67 CHOPPER | 68 TRIPLET WOB | 69 CLOCK HATS | 70 SYNC ARP |
| 71 STRINGS | 72 BRASS PAD | 73 GLASS KEYS | 74 ORGAN | 75 MALLET |
| 76 BELL | 77 PAD SWEEP | 78 VOX | 79 WIND | 80 AMBIENT KEYS |
| 81 SNARE | 82 HI-HAT | 83 CLAP | 84 BELL HIT | 85 BLIP PERC |
| 86 DROP | 87 RISER | 88 UFO | 89 DRONE 5TH | 90 DARK DRONE |

**Ogni programma parte dall'INIT e cambia solo le manopole che servono a quel suono**, usando solo ciò che ha il
pannello di riferimento (più gli effetti): niente NOISE, VELO, MODWHL, AFTERTC, GATE, FINE, ANALOG, arpeggiatore o LFO SYNC.
Così si rifanno sullo strumento di riferimento partendo dal suo INIT e girando solo quelle manopole: l'elenco di
ognuno, nell'ordine del pannello, è in `firmware/src/mfm_patches.h`.

01-06 i preferiti (BRASS LEAD, CLASSIC LEAD, CLASSIC BASS, PWM LEAD, FOLDED TRI, DOTTED LEAD); poi 07-15 e 44-53
bassi, 16-23 e 54-61 lead, 24-33 patch che usano il patchbay, 62-70 **ritmici** (nati a tempo; dalla 0.5 con l'LFO
libero alla velocità che avevano a 120 BPM, perché il riferimento non ha il clock; 65 CLOCK MELODY e 69 CLOCK HATS
suonano da soli, con l'LFO in GATE), 71-80 pad, tastiere e texture, 34-43 e 81-90 percussioni ed effetti. Livellati come nella 0.4.1. Alla prima accensione e dopo
l'azzeramento della memoria: traccia 1 su 01 BRASS LEAD, 2 su 11 PWM BASS, 3 su 21 FM LEAD, 4 su 31 SLOW SWELL. **43 DRONE, 89 DRONE 5TH e 90 DARK DRONE suonano senza note**
(VCA ON); anche 27 TREMOLO e 28 RING TONE (l'LFO in VCA CV) e i suoni con l'LFO in GATE.

## Le differenze dal pannello di riferimento

- Il **PITCH** va a semitoni (comodo col MIDI), con **FINE** per l'intonazione fine.
- I cavi si "impilano" (un'uscita a più prese, senza bisogno di un MULT); un anello di retroazione gira con un campione
  di ritardo.
- Niente ingressi e uscite di tensione verso l'esterno: tutto il patchbay è dentro. L'uscita è quella dell'FM-1.
- La deriva analogica è simulata (**ANALOG**, HOME → VOICE; di serie poca).
- Un salvataggio aspetta il silenzio (la scrittura in memoria ferma l'audio per ~50 ms): con un **drone** acceso parte
  quando lo spegni.

## La CPU

Il budget è 5805 µs ogni 256 campioni. Misurato sull'FM-1 con la 0.2: a riposo 7 %, una voce normale circa 12 %; il caso
più estremo possibile (4 tracce, ognuna con tutto a frequenza audio, più chorus, delay e riverbero) 92 %, senza mai un
blocco in ritardo. I suoni veri pesano molto meno; REC spento toglie anche il costo degli effetti. `py fm1_cpu.py` (con
l'FM-1 collegato) rifà le misure.

## Ricompilarlo

Tutto è in Ubuntu (WSL) su questo PC, in `~/fm1/fmav1`:

```
cd ~/fm1/fmav1 && sh build.sh --release 0.5.1          # -> build/fmav1-0.5.1.fwsc
OBV_DEFS=-DMFM_NTRK=1u sh build.sh                     # la versione a una traccia
sh tests/run_tests.sh                                 # tutti i test (sanitizer e fuzz compresi, ~3 min)
QUICK=1 sh tests/run_tests.sh                         # gli stessi senza i 90 programmi sotto sanitizer (~5 min)
```

FMav-1 è un lavoro indipendente: non è fatto, approvato né supportato da nessun costruttore di sintetizzatori e non
contiene niente dei loro strumenti (né codice, né suoni, né grafica). "M-VAVE" e "FM-1" sono dei loro proprietari.
