# FMav-1 0.3 per M-VAVE FM-1

Un **sintetizzatore monofonico semi-modulare** dentro l'FM-1, ricostruito controllo per controllo e presa per presa dal
manuale d'uso del suo strumento di riferimento: un **VCO** (dente di sega → impulso, PULSE WIDTH, PWM), un **filtro
ladder a transistor a 4 poli** che auto-oscilla, l'**LFO** (0,1-550 Hz, triangolo → quadra, ora anche **a tempo**),
l'**EG ADSR**, il **VCA** con l'interruttore **VCA MODE**, la **tastiera** (priorità alla nota più bassa, legato, GLIDE,
KB SCALE), il **wave folder**, il **Sample + Hold**, il **mixer ONE+TWO**, l'**attenuatore** e il **patchbay** con le
sue **normalizzazioni**. Nessun sequencer: si suona dal Digitakt via MIDI TRS (o USB, o dai tasti).

**Quattro tracce** stile Felucca, ognuna col suo suono, sui canali MIDI N, N+1, N+2, N+3. **Chorus, delay e riverbero**
in comune. **90 programmi di fabbrica**, dal 91 al 99 INIT.

Identità del firmware: `FM-1_3628` (F-M-A-V sulla tastiera del telefono). SHA-256 in `SHA256.txt`.

> **Avvertenza.** FMav-1 è software libero, fornito **così com'è, senza nessuna garanzia**. Installarlo e usarlo è a
> **tuo rischio**: l'autore non risponde di danni all'FM-1 (compreso un FM-1 che non si avvia più, "brickato"), ad
> altri apparecchi, alle orecchie o ai dati. Non è un prodotto di M-VAVE né di nessun costruttore di sintetizzatori, e
> non ne contiene codice, suoni o grafica. Vedi `LICENSE` (GPL-3.0) e `DISCLAIMER.md`.

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
    tempo), **CUTOFF** (un trance gate), **VCA CV**, **PWM**…
  - **L'arpeggiatore** con un clock MIDI scatta sulla griglia del clock (la prima nota subito).
  - Senza clock MIDI tutto segue il **TEMPO** (GLO → GLOBAL). Il delay era già a tempo.
- **Il cavo DUMMY**: una spina infilata da sola, senza niente dall'altra parte. Come sul pannello vero, **toglie la
  normalizzazione** e dà 0 V. Un solo scatto dopo `---` su ogni ingresso. Per esempio: DUMMY in **FOLD IN** = il VCF
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
- Corretto: un cavo da **MODWHL, VELO, PRESS** (o CLOCK) sul patchbay arriva da fuori, senza la spina disegnata
  all'altra estremità.
- I programmi salvati con la 0.1 e la 0.2 vengono convertiti da soli; le impostazioni globali restano.

## Installarlo

1. Collega l'FM-1 al PC con un cavo USB dati e accendilo.
2. Da questa cartella:

   ```
   py fm1_install.py fmav1-0.3.fwsc --yes
   ```

   Per vedere cosa c'è installato: `py fm1_install.py --info`

- Funziona sopra le versioni 0.1-0.2, OberVave, Felucca, SLOOP e quasi tutti i loro fork. Se
  l'installer non riconosce il firmware attuale, indica la sua porta MIDI: `--port NomePorta`.
- **I programmi degli altri firmware non vengono toccati**: FMav-1 salva i suoi a 0xD8000 (dove li salvavano le
  versioni 0.1-0.2: vengono convertiti).
- Se l'FM-1 non si avvia più: spegnilo, tieni premuto **OCT-** e riaccendilo (modalità di emergenza), poi rilancia il
  comando.

## I comandi

**Un pulsante apre una sezione** (premuto di nuovo salta all'elemento dopo), **SELECT girato scorre tutte le sue
pagine**, **le 4 manopole** sono i 4 parametri sullo schermo (accelerano se giri veloce), **SELECT premuto apre UTL**.

| Pulsante | Le pagine che SELECT scorre |
| --- | --- |
| **HOME** | **PERFORM**: CUTOFF, RESONANCE, VCF MOD AMT, DECAY · VCO WAVE, PULSE WIDTH, LFO RATE, GLIDE · PITCH MOD AMT, PWM AMT, FOLD, VOLUME → **MIXER**: i LEVEL delle 4 tracce · i loro PAN |
| **EDIT** | **VCO**: PITCH, VCO WAVE, PULSE WIDTH, FINE · **MOD**: VCO MOD MIX, PITCH MOD AMT, PWM AMT · **INPUTS**: le prese 1V/OCT e PWM → **VCF**: CUTOFF, RESONANCE, VCF MOD MIX, VCF MOD AMT · **INPUTS**: le prese CUTOFF (KB CV = il filtro segue la tastiera) e FOLD IN, FOLD |
| **ENV** | **EG**: ATTACK, DECAY, SUSTAIN, RELEASE · **INPUTS**: la presa GATE → **VCA**: VOLUME, **VCA MODE** (EG / ON), la presa VCA CV |
| **LFO** | **LFO**: LFO RATE, LFO WAVE, la presa LFO RATE, **SYNC** → **S+H**: le prese S+H (VCO) e S+H GATE → **CLOCK**: CLOCK (valore di nota), GATE, TEMPO |
| **FX** | **CHORUS**: ON, TYPE, SEND · RATE, DEPTH → **DELAY**: ON, SEND, TIME L, TIME R · FEEDBACK, PING-PONG, WIDTH · LOW CUT, HIGH CUT → **REVERB**: ON, SEND, DECAY, DAMP · PRE-DELAY, DIFFUSION, WIDTH |
| **SEQ** | **PATCHBAY**: tutti i cavi → INPUTS 1: FOLD IN, 1V/OCT, PWM, ONE → INPUTS 2: LFO RATE, CUTOFF, GATE, VCA CV → INPUTS 3: TWO, S+H (VCO), S+H GATE, ATTN |
| **SELECT premuto** | **UTL**: **WAVE FOLDER**: FOLD, la presa FOLD IN → **MIXER**: ONE LVL, le prese ONE e TWO → **ATTENUATOR**: ATTENUATOR, la presa ATTN |
| **ARP** | L'arpeggiatore: MODE, OCTAVES, RATE, GATE. ARP premuto di nuovo: acceso / spento |
| **GLO** | **KEYBOARD**: KB SCALE, GLIDE, PRIO, RETRIG → **VOICE**: BEND, ANALOG → **GLOBAL**: MIDI CH, TUNE, TEMPO, KEY VEL · LIGHTS (in alto a destra la CPU) |
| **ALGORITHM** | **La traccia** sul pannello (1-4): tastiera, pagine e PRESETS lavorano su quella |
| **PRESETS** | Il programma 01-99 della traccia (91-99 INIT) |
| **PLAY** | HOLD (le note restano) |
| **REC** | **Tutti gli effetti spenti / accesi** (acceso: suonano) |
| **SAVE** | Premuto: `SAVE TO n?` (PRESETS sceglie lo slot), SAVE di nuovo: salvato (scritto quando c'è silenzio). **Tenuto + tasto**: F3 INIT, G3 suono casuale, A3 MUTATE |
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
| **S+H (VCO)** | **(VCO)** | quello che il S+H campiona |
| **S+H GATE** | **(LFO)** | quando campiona (a ogni salita) |
| **ONE (-5)** | **(-5V)** | il canale 1 del mixer, col suo ONE LVL |
| **TWO** | --- | il canale 2 del mixer, a guadagno 1 |
| **ATTN (+5)** | **(+5V)** | quello che l'ATTENUATOR riduce (senza cavo: una tensione 0..+5 V) |

Le uscite: **KB CV**, **VCO**, **LFO**, **EG**, **S+H**, **ONE**, **ONE+TWO**, **ATTN**, il **CLOCK**, tre sorgenti MIDI
**VELO**, **MODWHL**, **PRESS** (0..+5 V) e **DUMMY**. **VCA OUT** è l'uscita audio. Una stessa uscita può andare a
quante prese vuoi; un ingresso non prende mai le uscite del suo modulo. Le tensioni: VCO, LFO e S+H ±5 V, EG 0-8 V, il
mixer satura a ±8 V, CLOCK 0/+5 V.

**Qualche patch per cominciare:**

- **Wave folding**: SELECT premuto → WAVE FOLDER, manopola 2 (FOLD IN) su **VCO**, poi gira FOLD.
- **Il filtro che segue la tastiera**: EDIT → VCF → INPUTS, manopola 1 (CUTOFF) su **KB CV**. Con la RESONANCE al
  massimo il filtro diventa un oscillatore sinusoidale suonabile (con DUMMY in FOLD IN resta solo lui: 52 WHISTLE).
- **S+H a tempo sul filtro**: S+H GATE ← CLOCK, CUTOFF ← S+H (programma 63 CLOCK S+H).
- **Melodie casuali a tempo**: 1V/OCT ← ATTN, ATTN ← S+H, S+H GATE ← CLOCK, GATE ← CLOCK (programma 64; suona da solo).
- **Wobble a tempo**: VCF MOD MIX tutto su LFO, VCF MOD AMT alto, LFO SYNC 1/8 (programma 61).
- **L'LFO come secondo oscillatore**: LFO RATE ← KB CV, LFO RATE a 116 (unisono) o 122 (una quinta), ONE ← LFO,
  TWO ← VCO, FOLD IN ← ONE+TWO (programmi 16, 44, 56, 60).
- **Tremolo**: VCA CV ← LFO. **Auto-repeat**: GATE ← LFO o CLOCK. **Drone**: VCA MODE ON (programmi 40, 89, 90).

## Gli effetti

**FX** apre CHORUS, DELAY e REVERB, ognuno con il suo **ON** sulla manopola 1; **REC** li spegne e riaccende tutti
insieme. Sono **in comune alle quattro tracce, come mandate**: ogni suono ci manda con il suo **SEND** (e il suo ON), e il
loro carattere (tempi, feedback, decay…) è quello del suono della prima traccia che li usa.

- **CHORUS**: il chorus a BBD dei polifonici classici, TYPE **I**, **II**, **I+II** (o FREE se giri RATE / DEPTH).
- **DELAY**: TIME L e TIME R a valori di nota del tempo (o del **clock del Digitakt**), FEEDBACK, PING-PONG, WIDTH, LOW CUT
  e HIGH CUT nelle ripetizioni.
- **REVERB**: un plate (Dattorro), DECAY, DAMP, PRE-DELAY, DIFFUSION, WIDTH.

## MIDI con il Digitakt

- **TRS MIDI IN** dell'FM-1 (adattatore TRS-DIN tipo A o B) o USB.
- **GLO → GLOBAL → MIDI CH = N**: il canale N suona la **traccia 1**, N+1 la **2**, N+2 la **3**, N+3 la **4**,
  qualunque traccia sia sul pannello. Con **OMNI** tutto suona la traccia sul pannello.
- Note, pitch bend, aftertouch, mod wheel (CC 1), volume della traccia (CC 7), sustain (CC 64), program change, e **una
  CC per ogni parametro, cavi ed effetti compresi** (`CC-NRPN.md`). Con un **p-lock sulla CC di una presa** il Digitakt
  cambia il cavo a quel passo.
- **CC fisse**: 74 CUTOFF, 71 RESONANCE, 73 ATTACK, 72 RELEASE, 75 DECAY, 5 GLIDE, 93 CHORUS SEND, 92 DELAY SEND,
  91 REVERB SEND.
- **Clock**: attiva l'invio del clock sul Digitakt (SETTINGS → MIDI CONFIG → SYNC → CLOCK SEND, e TRANSPORT SEND per
  START / STOP). LFO SYNC, l'uscita CLOCK, l'arpeggiatore e il delay lo seguono; START riallinea tutto all'inizio della
  battuta. Fermato il clock, continuano al tempo misurato.

## I 90 programmi di fabbrica (91-99 INIT)

| | | | | |
| --- | --- | --- | --- | --- |
| 01 CLASSIC BASS | 02 SQUARE BASS | 03 ACID LINE | 04 RUBBER BASS | 05 FOLD BASS |
| 06 PWM BASS | 07 DIRTY BASS | 08 FM BASS | 09 SUB PLUCK | 10 WOW BASS |
| 11 CLASSIC LEAD | 12 SQUARE LEAD | 13 FOLD LEAD | 14 RESO LEAD | 15 GLIDE LEAD |
| 16 DUO LEAD | 17 PWM LEAD | 18 FM LEAD | 19 WAH LEAD | 20 SCREAM LEAD |
| 21 S+H FILTER | 22 S+H MELODY | 23 WOBBLE | 24 TREMOLO | 25 RING TONE |
| 26 FOLD SWEEP | 27 AUTO REPEAT | 28 SLOW SWELL | 29 CROSS MOD | 30 KB SCALE 2X |
| 31 KICK | 32 TOM | 33 METAL | 34 NOISE HIT | 35 ZAP |
| 36 LASER | 37 SIREN | 38 BLIPS | 39 ARP PLUCK | 40 DRONE |
| 41 DEEP SUB | 42 PICK BASS | 43 GROWL BASS | 44 OCTAVE BASS | 45 BLIP BASS |
| 46 SQUARE ACID | 47 BOOM BASS | 48 HOT BASS | 49 SEQ BASS | 50 SOFT BASS |
| 51 BRASS LEAD | 52 WHISTLE | 53 HOLLOW LEAD | 54 SOFT LEAD | 55 BITE LEAD |
| 56 FIFTH LEAD | 57 WHEEL LEAD | 58 FOLDED TRI | 59 CHIP LEAD | 60 SUB LEAD |
| 61 SYNC WOBBLE | 62 TRANCE GATE | 63 CLOCK S+H | 64 CLOCK MELODY | 65 SYNC PWM |
| 66 DOTTED LEAD | 67 CHOPPER | 68 TRIPLET WOB | 69 CLOCK HATS | 70 SYNC ARP |
| 71 STRINGS | 72 BRASS PAD | 73 GLASS KEYS | 74 ORGAN | 75 MALLET |
| 76 BELL | 77 PAD SWEEP | 78 VOX | 79 WIND | 80 AMBIENT KEYS |
| 81 SNARE | 82 HI-HAT | 83 CLAP | 84 BELL HIT | 85 BLIP PERC |
| 86 DROP | 87 RISER | 88 UFO | 89 DRONE 5TH | 90 DARK DRONE |

01-10 e 41-50 bassi, 11-20 e 51-60 lead, 21-30 patch che usano il patchbay, 61-70 **a tempo** (LFO SYNC, l'uscita
CLOCK; 64 CLOCK MELODY e 69 CLOCK HATS suonano da soli, col clock in GATE), 71-80 pad, tastiere e texture, 31-40 e 81-90
percussioni ed effetti. Livellati a misura (picchi tra -8 e -5 dBFS con il mixer a 100). Alla prima accensione: traccia
1 CLASSIC BASS, 2 CLASSIC LEAD, 3 S+H FILTER, 4 KICK. **40 DRONE, 89 DRONE 5TH e 90 DARK DRONE suonano senza note**
(VCA ON); anche 24 TREMOLO e 25 RING TONE (l'LFO in VCA CV) e i suoni con LFO o CLOCK in GATE.

## Le differenze dal pannello di riferimento

- Il **PITCH** va a semitoni (comodo col MIDI), con **FINE** per l'intonazione fine.
- I cavi si "impilano" (un'uscita a più prese, senza bisogno di un MULT); un anello di retroazione gira con un campione
  di ritardo.
- Niente ingressi e uscite di tensione verso l'esterno: tutto il patchbay è dentro. L'uscita è quella dell'FM-1.
- La deriva analogica è simulata (**ANALOG**, GLO → VOICE; di serie poca).
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
cd ~/fm1/fmav1 && sh build.sh --release 0.3            # -> build/fmav1-0.3.fwsc
OBV_DEFS=-DMFM_NTRK=1u sh build.sh                     # la versione a una traccia
sh tests/run_tests.sh                                 # tutti i test (sanitizer e fuzz compresi)
```

FMav-1 è un lavoro indipendente: non è fatto, approvato né supportato da nessun costruttore di sintetizzatori e non
contiene niente dei loro strumenti (né codice, né suoni, né grafica). "M-VAVE" e "FM-1" sono dei loro proprietari.
