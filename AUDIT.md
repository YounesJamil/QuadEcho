# QUAD ECHO – DSP-Audit (Befunde Phase 1, Umsetzung Phase 2)

Stand: Code `36fbeea` (DSP unverändert seit Runde 3), gemessen am 2026-10-28 auf i5-13600KF, MSVC 14.50 Release.
Grenzwerte: [SPEC.md](SPEC.md) (vom Nutzer freigegeben). Jede Zahl hier ist reproduzierbar:

```
QuadEchoTests --harness audit <blocksize|reset|numerics|robust|rtalloc|dumps> [ordner]
QuadEchoTests --harness bench 20 5
python tests/ref/delayunit_ref.py <refdump> --f32 ";index;cur;env;mod;all"
python tests/ref/audit_analysis.py <dumps>
```

Rohdaten: `Q:\QuadEcho\audit\` (Logs), `Q:\QuadEcho\refdump\v1`, `Q:\QuadEcho\audit\dumps`.

**Legende Spalte „Art“:** **R** = Refactor/Effizienz, muss gegen die Baseline bitgenau nullen, braucht keine Freigabe.
**N** = ändert nichts an der Baseline (betrifft nur Fälle, die dort nicht vorkommen, z. B. NaN-Eingang oder Blockgrößen,
die kein Vielfaches von 16 sind), ändert aber das Verhalten. **K** = klangverändernd, nur nach Freigabe.

---

## Stand nach Phase 2 (Abnahme, 2026-10-28)

Alle Befunde außer C8 (Selbstoszillation) und C13 (Nachdunkeln) sind umgesetzt; diese beiden warten auf eine
Entscheidung (Geschmack). Messungen wie in Phase 1, auf dem Release-Build nach dem letzten Commit.

| SPEC | Grenzwert | Phase 1 | jetzt | Status |
|---|---|---|---|---|
| 1 CPU Mittel (192 kHz/32, Worst Case) | ≤ 20 % | 17 % | 18,6 % (mit Fades 19,3 %) | ok |
| 1 CPU p99 / p99,9 | ≤ 50 % / ≤ 75 % | 21–26 % / 30–32 % | 32–34 % / 37–38 % | ok |
| 1 Ereignis-Block | ≤ 100 % | 734 % | 63 % | ok (B1) |
| 1 Speicher anfordern im Audio-Thread | nie | 0 | 0 | ok |
| 2 ganzzahlige Verzögerung | ≤ −120 dBFS | −128 … −143 | −128 … −141 | ok |
| 2 gebrochene Verzögerung | ≤ −110 dBFS | −67 | −140 | ok (C5) |
| 2 moduliert | ≤ −90 dBFS | −39 … −84 | −105 … −144 | ok (C5, C6) |
| 2 Repitch | ≤ −90 dBFS | −11 | −141 | ok (C1) |
| 3 zwei Renders | bitgenau | 804/804 | 852/852 | ok |
| 3 nach `reset()`/`prepare()` | bitgenau | 132/134 | 142/142 | ok (A4) |
| 3 Blockgrößen 1/32/64/441/4096/wechselnd | bitgenau | 86–104/104 | 112/112 (48 und 192 kHz) | ok (A2, A3) |
| 3 Delay-Zeit nach Sprung | ±0,5 Sample | bis 1200 Samples daneben | < 0,05 Samples | ok (C1) |
| 4 Latenz 0, Mix 0 bitgenau | bitgenau | bitgenau | bitgenau (Test) | ok |
| 5 NaN/Inf im Eingang | endlich, erholt sich | dauerhaft NaN | 0 nicht-endliche Samples, Echos kommen wieder | ok (A1) |
| 5 Ausgang mit Limiter | ≤ 0 dBFS | 0,0 dBFS | 0,0 dBFS (ON DRY an); ohne ON DRY nie über dem Original | ok (g) |
| 5 Ausgang extrem | ≤ +6 dBFS | +14,3 dBFS | +6,0 dBFS (mit und ohne Limiter) | ok (C9) |
| 5 Denormals ohne FTZ | ≤ 1,5 × | 2,15 × | 0,99 × | ok (C10) |
| 5 Tail-Länge | berechnet | fest 10 s | berechnet, unendlich bei Dauerschwingung | ok (A5) |
| 6 ASan | ohne Befund | ohne Befund | ohne Befund (Tests, Audit, Baseline) | ok |
| 6 UBSan | ohne Befund | 2 Befunde (NaN-Fall) | 0 Befunde | ok |
| 6 pluginval 10 Debug + Release | grün | grün | grün (auch mit neuem Parameter) | ok |

Weitere Messwerte nach Phase 2:
- Aliasing (7 kHz, 48 kHz): Drive 0 / −6 dBFS −67 … −46 → −135 … −75 dBc; Drive +12 dB erstes Echo −45 → −86 dBc;
  Drive +24 dB / −20 dBFS −15 → −30 dBc; Drive +24 dB / −6 dBFS −11 → −18 dBc (Rest wie bei 96 kHz; nur 4×
  würde hier weiter helfen).
- Schleifen-Sättigung bei −24 dB Input Gain (gleicher Pegel in der Schleife): k3 −23,5 → −164 dB.
- CPU 48 kHz durch das Oversampling: Standard 1110 → 1800 Zyklen/Sample (2,5 % der Blockzeit bei Block 32),
  Worst Case 7,1 %. 96/192 kHz ohne Oversampling.
- Baseline: v1 (Phase 0) → v5 (`tests/baseline/manifest-v5.tsv`, 852 Renders). Jede Klangänderung hat eine eigene
  Version; die Commit-Texte nennen, welche Renders sich um wie viel geändert haben.

### Commits Phase 2
| Commit | Befund | Art | Inhalt |
|---|---|---|---|
| dd12d14 | A1 | N | NaN/Inf am Eingang → 0, Schutz in Schleife und Ausgang |
| 45c694f | B1 | R | Leitungen beim Ausschalten verteilt löschen |
| 9d460d3 | A2/A3 | N | Steuerraster an der absoluten Sample-Position (auch Spatializer, globaler Filter, Noise) |
| 129ff4d | A4 | N | Noise-Seeds in `reset()` |
| 36b91bb | A5 | N | Tail-Länge berechnet |
| fbefa71 | B2/B4 | R | LFO bei „aus“ nicht rechnen, ungenutzte Konstante |
| b28cd43 | B3 | R | `__restrict` in Mischschleifen (Gewinn ~1 %) |
| c8ee130 | g / C11 | K | Limiter wahlweise nur auf die Echos (Schalter ON DRY), Zustandsversion 6 |
| ee7cede | a / C1 | K | Repitch in double, Baseline v2 |
| 826cbf2, f14d83b | b / C5, C6 | K | Index, LFO, Glättung, Hüllkurve in double, Baseline v3 |
| 940c4bc | f / C9 | K | Notbremse +6 dBFS |
| 7d8ac15 | C2 | K | schwächere Schleifen-Sättigung unter 0 dB Input Gain, Baseline v4 |
| a52340f | c / C3 | K | 2-faches Oversampling der Sättigungen (44,1/48 kHz), Baseline v5 |
| c23f114 | h / C12 | K | LFO-Sync folgt der Songposition |
| c920bb6 | C10 | N | Engine setzt FTZ/DAZ selbst |

Noch offen: C8 (Selbstoszillation bei Resonanz/Boost), C13 (Hermite-Nachdunkeln) und die ungemessenen
Vermutungen 12–15 (Abschnitt 8).

---

## Übersicht: Spezifikation gegen Ist (Phase 1, vor den Fixes)

| SPEC | Grenzwert | Ist | Status |
|---|---|---|---|
| 1 CPU Mittel (192 kHz/32, Worst Case) | ≤ 20 % | 17 % | ok |
| 1 CPU p99 / p99,9 | ≤ 50 % / ≤ 75 % | 21–26 % / 30–32 % | ok |
| 1 Ereignis-Block | ≤ 100 % | **734 %** (Einheit aus) | **verletzt** → B1 |
| 1 Speicher anfordern im Audio-Thread | nie | 0 | ok |
| 2 ganzzahlige Verzögerung | ≤ −120 dBFS | −128 … −143 | ok |
| 2 gebrochene Verzögerung | ≤ −110 dBFS | **−67** | **verletzt** → C5 |
| 2 moduliert | ≤ −90 dBFS | **−39 … −84** | **verletzt** → C5, C6 |
| 2 Repitch | ≤ −90 dBFS | **−11** | **verletzt** → C1 |
| 3 zwei Renders, frische Instanz | bitgenau | 804/804 | ok |
| 3 nach `reset()`/`prepare()` | bitgenau | 132/134 (Noise weicht ab) | **verletzt** → A4 |
| 3 Blockgrößen, konstante Parameter | bitgenau | Vielfache von 16: 104/104; sonst 86/104 | **verletzt** → A2, A3 |
| 3 Sample-Raten: Filter | ±0,5 dB | Schleifenfilter 48 ↔ 192 kHz: Maximum gleich auf 0,1 dB | ok |
| 3 Sample-Raten: Delay-Zeit | ±0,5 Sample | Fade/Jump exakt; Repitch bis 1200 Samples daneben | **verletzt** → C1 |
| 4 Latenz 0, Mix 0 bitgenau | bitgenau | bitgenau (unter dem Limiter-Knie) | ok |
| 5 NaN/Inf im Eingang | Ausgang endlich, erholt sich | **Ausgang dauerhaft NaN** | **verletzt** → A1 |
| 5 Ausgang mit Limiter | ≤ 0 dBFS | 0,0 dBFS | ok |
| 5 Ausgang ohne Limiter, extrem | ≤ +6 dBFS | **+14,3 dBFS** | **verletzt** → C9 |
| 5 Denormals ohne FTZ | ≤ 1,5 × | **2,15 ×** (Worst Case) | **verletzt** → C10 |
| 5 Tail-Länge | berechnet | fest 10 s, real bis > 600 s | **verletzt** → A5 |
| 5 Zustandsversionen | laden ohne Absturz | ok (Test + Code, siehe Robustheit) | ok |
| 6 ASan / UBSan | ohne Befund | ASan ohne Befund; UBSan 2 Befunde im NaN-Fall | **verletzt** → A1 |
| 6 clang-tidy, /W4 /analyze | ohne Befund | ohne Befund (nur Hinweise) | ok |
| 6 pluginval 10 Debug + Release | grün | grün | ok |

---

## 1 Invarianz

### 1.1 Blockgrößen (`audit blocksize`)
Alle 67 Baseline-Fälle × Impuls/Rauschen, gegen Block 512.

| Block | 48 kHz ohne Automation | 192 kHz ohne Automation |
|---|---|---|
| 32, 64, 4096 | 104/104 bitgenau | 104/104 bitgenau |
| 1 | 86/104, schlechteste **−5,5 dBFS** (globaler Filter) | 86/104, −10,7 dBFS |
| 441 | 86/104, −28,7 dBFS | 86/104, −32,7 dBFS |
| wechselnd 1…1024 | 86/104, −29,5 dBFS | 86/104, −33,4 dBFS |

Abweichende Fälle: globaler Filter, Noise, alle LFO-Wellen, Hüllkurve. Fälle mit Automation sind nicht vergleichbar,
weil eine Änderung immer am nächsten Blockanfang greift.

- **Befund A2 (LFO-Rate hängt von der Blockgröße ab):** Der LFO des globalen Filters und der Noise-LFO zählen pro
  angefangenem 16er-Teilblock 16 Samples weiter, auch wenn der Teilblock kürzer ist.
  Bei Blockgröße 1 laufen sie **16-mal zu schnell**, bei 441 um 1,6 % zu schnell.
  Ursache: [GlobalFilter.cpp:62](Source/GlobalFilter.cpp#L62) `phase += hz / sr * 16.0;`,
  [NoiseSource.cpp:103](Source/NoiseSource.cpp#L103) `phase += lfoHz / sr * (double) kSub;`.
  Vorschlag: um die tatsächliche Länge des Teilblocks weiterzählen.
- **Befund A3 (Steuerraster beginnt bei jedem Aufruf neu):** Die 16er-Teilblöcke in
  [DelayUnit.cpp:159](Source/DelayUnit.cpp#L159), [Spatializer.cpp:93](Source/Spatializer.cpp#L93),
  [NoiseSource.cpp:96](Source/NoiseSource.cpp#L96), [GlobalFilter.cpp:79](Source/GlobalFilter.cpp#L79) und die
  256er-Chunks in [QuadEngine.cpp:92](Source/QuadEngine.cpp#L92) starten bei jedem `process()` neu. Filterkoeffizienten,
  Hüllkurven-Abbildung und Sättigungskoeffizienten werden daher je nach Host-Puffer an anderen Samples aktualisiert
  (Vermutung 1 bestätigt). Vorschlag: Raster an der absoluten Sample-Position ausrichten (Zähler über Blöcke hinweg).
  Bei Block 512 bleibt die Baseline gleich.

### 1.2 Zwei Renders, `reset()`, `prepare()` (`audit reset`)
- Frische Instanzen: 804/804 bitgenau.
- Dieselbe Instanz nach `reset()` oder `prepare()`: 132/134. Nur `feat_noise_into` weicht ab (−19,6 dBFS ab Sample 0).
- **Befund A4:** Die Zufalls-Seeds des Noise ([NoiseSource.h:25](Source/NoiseSource.h#L25)) werden in
  [NoiseSource.cpp:59](Source/NoiseSource.cpp#L59) `reset()` nicht zurückgesetzt. Vorschlag: Seeds in `reset()` setzen.

### 1.3 Mix 0 gegen Dry
Global Mix 0 (Chain+Solo, 4 Einheiten) liefert bei allen Raten mit und ohne Limiter **bitgenau den Eingang**
(Impuls und Sweep, Pegel unter dem Limiter-Knie 0,8).
- **Befund C11 (Vermutung 8 bestätigt, Design):** Der Soft-Limiter ([QuadEngine.cpp:428](Source/QuadEngine.cpp#L428))
  wirkt auf die ganze Summe inklusive Original. Ein Original-Signal mit Spitze 0,95 kommt mit 0,927 heraus (−0,2 dB),
  auch bei Mix 0. Frage: Soll der Limiter nur auf die Echos wirken?

### 1.4 Transport (LFO-Phase gegen Songposition)
- **Befund C12 (Vermutung 2 bestätigt, Code):** Das Plugin liest vom Host nur das Tempo
  ([PluginProcessor.cpp:45-46](Source/PluginProcessor.cpp#L45)). Die LFO-Position läuft frei
  ([DelayUnit.cpp:217](Source/DelayUnit.cpp#L217)) und wird nur bei `reset()` auf 0 gesetzt. Mit LFO-Sync stimmt die
  Rate, die Phase relativ zum Takt hängt aber davon ab, wann die Wiedergabe gestartet wurde; Springen im Song ändert
  sie nicht. Vorschlag: bei Sync die Phase aus der ppq-Position ableiten (klangverändernd, Feature-Entscheidung).

---

## 2 Numerik

### 2.1 Repitch bleibt vor dem Ziel stehen (`audit numerics`)
Delay-Zeit per Phasenvergleich gemessen, 8 s nach dem Zeitsprung:

| Rate | Sprung | Repitch: Abweichung | Fade/Jump |
|---|---|---|---|
| 48 kHz | 100 → 350 ms | −9,4 Samples (−0,20 ms) | exakt |
| 48 kHz | 1000 → 4000 ms | −75 Samples (−1,56 ms) | exakt |
| 192 kHz | 100 → 350 ms | −150 Samples (−0,78 ms) | exakt |
| 192 kHz | 1000 → 4000 ms | **−1200 Samples (−6,25 ms)** | exakt |
| 192 kHz | 3999 → 3000 ms | +1200 Samples (+6,25 ms) | exakt |

- **Befund C1 (Vermutung 3 bestätigt, Fehler):** Die Glättung `l.cur += (target - l.cur) * rpCoef`
  ([DelayUnit.cpp:75](Source/DelayUnit.cpp#L75)) rechnet in float. Sobald der Schritt kleiner als eine halbe
  float-Stelle von `cur` ist, bewegt sie sich nicht mehr. Die eingestellte Delay-Zeit wird bei Repitch nie erreicht,
  und der Fehler bleibt bis zur nächsten Änderung. Bei Sync-Zeiten läuft das Echo damit neben dem Takt.
  Zusätzlich ist der Koeffizient ([DelayUnit.cpp:33](Source/DelayUnit.cpp#L33)) in float berechnet und bei 192 kHz um
  ~0,2 % falsch. Beweis: Das Python-Modell mit beidem in float trifft das C++ auf −141 dBFS
  (`--f32 cur`), ohne float liegt es bei −11 dBFS.
  Vorschlag: `cur` und Koeffizient in double, am Ziel einrasten. Klangverändernd (Repitch erreicht dann die richtige Zeit).

### 2.2 Auflösung des Delay-Index
| Rate | 0,1 s | 1 s | 4 s |
|---|---|---|---|
| 48 kHz | 1/2048 Sample (10 ns) | 1/256 (81 ns) | 1/64 (326 ns) |
| 192 kHz | 1/512 (10 ns) | 1/64 (81 ns) | 1/16 Sample (326 ns) |

- **Befund C5:** Lese-Index, Zielzeit und Modulation sind float ([DelayUnit.cpp:119](Source/DelayUnit.cpp#L119),
  [:78](Source/DelayUnit.cpp#L78), [DelayLine.h:26](Source/DelayLine.h#L26)). Statische gebrochene Zeiten liegen
  dadurch um bis zu eine halbe Stelle daneben: unhörbar (ns), aber Residuum −67 dBFS statt −139. Beweis: Modell mit
  float-Index (`--f32 index`) → −139 … −158 dBFS. Bei Modulation wird die Delay-Zeit in Stufen von bis zu 1/16 Sample
  bewegt (lange Zeiten, 192 kHz); das ergibt ein Rauschen proportional zur Frequenz (geschätzt um −45 dB relativ bei
  10 kHz und 4 s; im Dump nicht einzeln gemessen).
  Vorschlag: Index in double (ganzzahliger Teil + Bruchteil getrennt). Klangverändernd (minimal).

### 2.3 Hüllkurve und Modulation in float
- **Befund C6:** Bei 192 kHz ist der Release-Koeffizient der Hüllkurve 3,5·10⁻⁵ und in float um ~0,2 % falsch
  ([DelayUnit.cpp:37](Source/DelayUnit.cpp#L37), [:205](Source/DelayUnit.cpp#L205)). Die Modulation folgt damit der
  Hüllkurve leicht anders als vorgesehen: −39 dBFS gegen das Modell, mit float-Hüllkurve im Modell −63 dBFS.
  Der Rest der modulierten Fälle (−64 … −84 dBFS) verteilt sich auf float-LFO-Werte, Modulationsglättung und Index;
  einzeln liegt keiner davon vorn. Vorschlag: Zeitkonstanten-Koeffizienten in double berechnen, Glättungszustände in
  double. Klangverändernd (minimal).

---

## 3 Stabilität (Schleifenverstärkung)

`|fb · H_Filter|`, Maximum über 20 Hz … 0,45 fs, HP 150 Hz + LP 2,5 kHz, Resonanz und Boost für beide gleich.
Analytisch (TPT = bilinear mit Vorverzerrung, Glocke nach RBJ) und gemessen (Echo 2 / Echo 1) stimmen bei 48 kHz auf
0,1 dB überein (bei 192 kHz bis 2,5 dB an einzelnen Frequenzen im Sperrbereich; das Maximum stimmt auf 0,1 dB).

| Resonanz (Q) | Boost | max \|H\| | Schleife bei Feedback 0,5 | bei 1,1 |
|---|---|---|---|---|
| 0 (0,71) | 0 | −0,5 dB | −6,6 dB | +0,3 dB |
| 0 (0,71) | 12 dB | +7,2 dB | **+1,2 dB** | +8,0 dB |
| 0,5 (1,71) | 0 | +2,5 dB | −3,5 dB | +3,4 dB |
| 0,5 (1,71) | 12 dB | +13,8 dB | **+7,8 dB** | +14,7 dB |
| 1 (4,71) | 0 | +10,6 dB | **+4,6 dB** | +11,4 dB |
| 1 (4,71) | 12 dB | +22,5 dB | **+16,5 dB** | +23,3 dB |
| LP 12 kHz allein, Resonanz 1, Boost 18 dB | | +28,5 dB | +22,5 dB | **+29,3 dB** |

- **Befund C8 (Vermutung 4 bestätigt):** Schon mit Feedback 50 % liegt die Schleife bei hoher Resonanz oder Boost über
  0 dB; die Echos wachsen an der Grenzfrequenz, bis der tanh sie begrenzt (Ursache: Güte
  [DelayUnit.cpp:129](Source/DelayUnit.cpp#L129) bis 4,7, Boost bis 18 dB, keine Pegelkorrektur). Das ist hörbar
  „Selbstoszillation“ und vielleicht gewollt. Optionen: so lassen; Feedback-Regler so skalieren, dass 100 % immer
  Schleifenverstärkung 1 bedeutet; Resonanz-Spitze in der Schleife kompensieren. Klangverändernd.
- Feedback 110 % ohne Filter: Schleife +0,3 dB, endet in einer Dauerschwingung bei ca. −5,5 dBFS (tanh-Fixpunkt).

---

## 4 Klangtreue

### 4.1 Hermite-Interpolator (gemessen = analytisch auf 0,01 dB)
| Nachkommaanteil | 5 kHz | 10 kHz | 15 kHz | 20 kHz (48 kHz) |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 dB |
| 0,2 | −0,01 | −0,20 | −0,80 | −1,68 |
| 0,5 | −0,04 | −0,53 | −2,53 | −8,41 |

Kumuliert bei Anteil 0,5: nach 10 Wiederholungen −5,3 dB bei 10 kHz, −25 dB bei 15 kHz; nach 20: −10,7 / −50 dB.
- **Befund C13 (Vermutung 5 bestätigt):** Jede Wiederholung wird je nach Nachkommaanteil dunkler, unabhängig vom
  LP-Filter. Bei Sync-Zeiten, die zufällig ganzzahlig sind, passiert nichts. Optionen: so lassen (wirkt analog);
  bei unmodulierter Zeit ganzzahlig lesen und den Bruchteil mit einem Allpass ausgleichen; höhere Interpolation
  (z. B. 6-Punkt). Klangverändernd.

### 4.2 Aliasing der Sättigung (7-kHz-Sinus, Feedback 0,9, je Echo)
| Rate | Pegel | Drive | Obertöne | Alias (zurückgefaltet) |
|---|---|---|---|---|
| 48 kHz | −20 dBFS | 0 | −62 … −50 dBc | −122 … −97 dBc |
| 48 kHz | −6 dBFS | 0 | −34 … −25 dBc | **−67 … −46 dBc** |
| 48 kHz | −20/−6 dBFS | +24 dB | −10 dBc | **−15 … −11 dBc** |
| 96 kHz | −6 dBFS | 0 | −34 … −24 dBc | −99 … −66 dBc |
| 96 kHz | −6 dBFS | +24 dB | −8 dBc | −12 dBc |

- **Befund C3:** Input-Sättigung und Schleifen-tanh ([DspTypes.h:296](Source/DspTypes.h#L296),
  [:304](Source/DspTypes.h#L304)) laufen ohne Oversampling. Mit Drive liegen die Spiegelfrequenzen fast so laut wie die
  echten Obertöne (unharmonisch, „digital“). Bei Drive 0 und lautem Signal sammelt sich Aliasing über die Echos an
  (−67 → −46 dBc). Vorschlag: 2×–4× Oversampling nur um die Sättigungen oder Anti-Derivative-Antialiasing (ADAA).
  Klangverändernd, kostet CPU.
- **Befund C2 (Nutzerwunsch, freigegeben als Plan):** Der tanh sitzt auch bei Drive 0 in der Schleife und klirrt
  schon bei −20 dBFS messbar (Residuum bis −57 dBFS gegen das lineare Modell). Unter 0 dB Input Gain soll die Sättigung
  schwächer werden, aber nie ganz verschwinden. Umsetzung in Phase 2 mit Vorher/Nachher-Messung.

---

## 5 Echtzeit

### 5.1 Zeit pro Block (`bench 20 5`, CPU-Zeit des Threads, Median aus 5 Läufen)
| 192 kHz, Block 32 | Mittel | p99 | p99,9 | max |
|---|---|---|---|---|
| Standard (Serial) | 5,9 % | 7,0 % | 9,2 % | 15 % |
| Worst Case | 17,4 % | 21,4 % | 30,2 % | 51 % |
| Worst Case + Fades | 18,2 % | 25,5 % | 31,9 % | 51 % |
| Worst Case + Algorithmuswechsel | 15,3 % | 19,9 % | 26,3 % | 44 % |
| Worst Case + Einheit an/aus | 15,7 % | 19,9 % | 28,2 % | **734 %** |

Zyklen pro Sample: Standard ≈ 1070, Worst Case ≈ 3200.

- **Befund B1 (Vermutung 6 bestätigt):** Wird eine Einheit ausgeschaltet, löscht
  [QuadEngine.cpp:247](Source/QuadEngine.cpp#L247) → [DelayUnit.cpp:43](Source/DelayUnit.cpp#L43) vier Delay-Leitungen
  in einem Block: bei 192 kHz 4 × 4 MB, **1,0–1,2 ms** (7-fache Frist); bei 48 kHz 0,24 ms (37 %).
  Vorschlag: Löschen über mehrere Blöcke verteilen, solange die Einheit aus ist; wird sie vorher wieder
  eingeschaltet, den Rest dann löschen. Ausgabe bleibt gleich (Art **R**).
- Algorithmuswechsel ([QuadEngine.cpp:127](Source/QuadEngine.cpp#L127), Spatializer-Reset, 3 × 60 ms-Leitungen) und
  Fades sind unauffällig. Die mit Wanduhr gemessenen Spitzen beim Algorithmuswechsel waren Windows-Unterbrechungen.
- Speicheranforderungen in `process()`: **0** (zählender `operator new`, Selbstprobe zählt korrekt).

### 5.2 Rechenaufwand und Vektorisierung
- **Befund B2 (Vermutung 10 bestätigt):** Die drei LFO-Werte werden pro Sample berechnet und erst danach mit
  `lfoGate` multipliziert ([DelayUnit.cpp:208-210](Source/DelayUnit.cpp#L208)), also auch bei ausgeschaltetem LFO.
  Ebenso läuft der Schleifen-tanh immer. Vorschlag: bei LFO aus nicht berechnen (Art **R**).
- **Befund B3 (Compiler-Report `/Qvec-report:2`):** In den fünf DSP-Dateien wird **keine einzige Schleife vektorisiert**.
  Die meisten sind Rückkopplungen (Filter, Delay) und lassen sich so nicht vektorisieren (Grund 1200). Die einfachen
  Kopier- und Mischschleifen in QuadEngine scheitern an möglichem Aliasing der Member-Arrays (Grund 1200/1305).
  Vorschlag: lokale `__restrict`-Zeiger in den Mischschleifen (Art **R**, geringer Gewinn). Größerer Gewinn wäre SIMD
  über die vier Einheiten oder die zwei Kanäle (großer Umbau, ggf. später).
- Ducking rechnet `log10` und `pow` pro Sample ([QuadEngine.cpp:79](Source/QuadEngine.cpp#L79), [:83](Source/QuadEngine.cpp#L83));
  im Worst Case enthalten, nicht einzeln gemessen.

---

## 6 Robustheit

### 6.1 NaN / Inf im Eingang (`audit robust`)
Ein einzelnes Sample links bei 0,5 s, danach normales Signal; Impuls bei 3 s prüft, ob wieder Echos kommen.

| Konfiguration | NaN | +Inf | 1e30 |
|---|---|---|---|
| 1 Einheit | **Ausgang ab 0,5 s nur noch NaN** | ebenso | endlich, erholt |
| Serial + globaler Filter | ebenso | ebenso | endlich, erholt |
| Worst Case | ebenso | ebenso | endlich, erholt |

- **Befund A1 (Vermutung 7 bestätigt, schwerwiegend):** Ein einziges NaN/Inf vom Host vergiftet Delay-Leitungen und
  Filter dauerhaft: Der Ausgang bleibt NaN bis zum nächsten `reset()` (Host-Neustart der Wiedergabe). NaN im Ausgang
  kann nachfolgende Plugins und den Master-Bus mitreißen. Ursachen: kein Prüfen am Eingang
  ([QuadEngine.cpp:133](Source/QuadEngine.cpp#L133)); die Denormal-Abfrage `abs(w) < 1e-18`
  ([DelayUnit.cpp:265](Source/DelayUnit.cpp#L265)) fängt NaN nicht.
  Zweiter Weg: Das NaN gelangt in den Hüllkurvenfolger ([DelayUnit.cpp:205](Source/DelayUnit.cpp#L205)) und bleibt dort
  für immer. Weil `envMapped * envMix` auch bei Hüllkurven-Anteil 0 gerechnet wird (NaN · 0 = NaN), wird die
  Modulation und damit der **Lese-Index NaN**. UBSan meldet dann undefiniertes Verhalten: NaN → int in
  [DelayLine.h:28](Source/DelayLine.h#L28) und einen Integer-Überlauf in [DelayLine.h:30](Source/DelayLine.h#L30).
  Der Zugriff bleibt dank Maske im Puffer, ist aber formal undefiniert.
  Vorschlag: nicht-endliche Samples am Eingang der Engine durch 0 ersetzen; zusätzlich je Einheit prüfen, ob der
  Schleifenzustand endlich ist, sonst diese Einheit leeren (Art **N**: ändert nichts an der Baseline).

### 6.2 Denormals
Ausklang nach Impuls, 40 s Stille, ohne und mit FTZ/DAZ:
1 Einheit mit Filtern Faktor 1,43; **Worst Case Faktor 2,15**.
- **Befund C10:** Das Plugin selbst setzt FTZ/DAZ in `processBlock` ([PluginProcessor.cpp:32](Source/PluginProcessor.cpp#L32)),
  im Host tritt das Problem also nicht auf. Ohne FTZ (z. B. Engine in anderem Kontext) laufen Filterzustände
  (JUCE-SVF, Spatializer-Tiefpass, Glocken) in Denormals; geschützt ist nur der Schleifeneingang.
  Vorschlag: SPEC-Punkt streichen, weil FTZ im Plugin garantiert ist (Entscheidung), oder Zustände selbst auf 0 setzen.

### 6.3 Extreme Automation (jeder Parameter springt jeden 32er-Block zwischen Minimum und Maximum, 30 s)
Kein NaN/Inf. Spitze mit Limiter 0,0 dBFS, **ohne Limiter +14,3 dBFS**.
- **Befund C9:** Ohne Limiter sind bis +14 dBFS möglich (SPEC: ≤ +6). Vorschlag: harte Sicherheitsgrenze nur ohne
  Limiter, z. B. tanh-Begrenzung ab +6 dBFS. Klangverändernd nur im Extrembereich.

### 6.4 Ausklingzeit (bis unter −100 dBFS, gemeldet werden immer 10 s)
| Einstellung | real |
|---|---|
| Feedback 0,5, 500 ms | 8 s |
| Feedback 0,8, 1 s | 47 s |
| Feedback 0,95, 1 s | 197 s |
| Feedback 0,95, 4 s | 596 s |
| Feedback 0,95, 1 s, LP 3 kHz | 134 s |
| Feedback 1,0 | klingt nicht aus |

- **Befund A5 (Vermutung 9 bestätigt):** [PluginProcessor.h:23](Source/PluginProcessor.h#L23) meldet fest 10 s. Ein Host,
  der nach der Tail-Länge abschaltet oder rendert, schneidet die Echos ab. Vorschlag: aus Zeiten und Feedback
  berechnen, unendlich bei Feedback ≥ 1 oder Freeze (Art **N**, kein Einfluss auf den Klang im Plugin).

### 6.5 Zustandsversionen
Version 5 mit Umrechnung ab Version 1 (Test „Zustands-Umrechnung“ grün). Ein neuerer, unbekannter Zustand wird ohne
Umrechnung geladen ([StateMigration.h:26](Source/StateMigration.h#L26)), unbekannte Parameter ignoriert JUCE. Kein Befund.

---

## 7 Werkzeuge

| Werkzeug | Ergebnis |
|---|---|
| MSVC `/W4` (Tests, Plugin) | keine Warnung in `Source/` |
| MSVC `/analyze` (5 DSP-Dateien) | keine Warnung |
| clang-tidy (bugprone, performance, clang-analyzer) | nur `performance-enum-size` (5 Enums, kosmetisch) |
| clang-cl `-Wall` (UBSan-Build) | ungenutzte Konstante `kTwoPi` ([DelayUnit.cpp:7](Source/DelayUnit.cpp#L7)) → Befund B4 (Art **R**) |
| ASan (MSVC), DSP-Tests | keine Speicherfehler; einzige rote Prüfung ist das CPU-Limit des Tests (ASan bremst) |
| ASan, Audit robust/rtalloc + Baseline | keine Speicherfehler; Baseline 804/804 bitgenau auch im ASan-Build |
| UBSan (clang-cl), DSP-Tests + Audit + Baseline | Tests 237/237 grün, Baseline 804/804 bitgenau (auch mit clang). **2 Befunde, beide nur im NaN-Test:** NaN → int ([DelayLine.h:28](Source/DelayLine.h#L28)) und Integer-Überlauf ([DelayLine.h:30](Source/DelayLine.h#L30)), siehe A1 |
| pluginval Strictness 10, Debug | grün |
| pluginval Strictness 10, Release | grün |

Builds: `Q:\QuadEcho\build-asan` (MSVC `/fsanitize=address`), `Q:\QuadEcho\build-clang` (clang-cl, `-fsanitize=undefined`,
statische Laufzeit), Skript `Q:\QuadEcho\tools\build-tools.sh`, Analyse-Skript `Q:\QuadEcho\tools\analyze.cmd`.

---

## 8 Noch nicht gemessen (Anhang-Vermutungen 12–15 aus AUDIT-PROMPT.md)
- 12: Intermodulation durch die Original-Spur bei Gain on Dry + Drive.
- 13: Freeze über Minuten (Pegel, Höhen), Klickfreiheit je Transition, Feedback-Invert.
- 14: Restwelligkeit des Duckings bei tiefen Tönen.
- 15: Pegel langer Serienketten vor dem Limiter.

---

## 9 Vorschlag für Phase 2 (Reihenfolge nach Schwere)

**Ohne Klangänderung (R/N), jeweils eigener Commit, Regressionstest, Baseline 804/804 bitgenau:**
1. A1 NaN/Inf am Eingang abfangen, Einheit mit vergiftetem Zustand leeren.
2. B1 Löschen beim Ausschalten über mehrere Blöcke verteilen (Spitze 734 % → Ziel < 100 %).
3. A2 LFO von globalem Filter und Noise um die tatsächliche Teilblocklänge weiterzählen.
4. A3 Steuerraster an der absoluten Sample-Position ausrichten (Blockgrößen bitgenau).
5. A4 Noise-Seeds in `reset()` zurücksetzen.
6. A5 Tail-Länge berechnen.
7. B2, B4 LFO bei „aus“ nicht berechnen, `kTwoPi` entfernen; B3 `__restrict` in Mischschleifen.

**Klangverändernd (K), nur nach Freigabe, je mit Vorher/Nachher-Messung:**
- C1 Repitch erreicht die eingestellte Zeit (double). *Empfehlung: ja, das ist ein Fehler.*
- C2 Schwächere Schleifen-Sättigung unter 0 dB Input Gain (bereits als Plan freigegeben).
- C5 Delay-Index in double. *Empfehlung: ja, kaum hörbar, behebt Stufen bei langer Modulation.*
- C6 Koeffizienten und Glättung in double. *Empfehlung: ja, kaum hörbar.*
- C3 Oversampling/ADAA für die Sättigungen. *Empfehlung: ja für Drive > 0, CPU messen.*
- C8 Schleifenverstärkung bei Resonanz/Boost. *Entscheidung: Selbstoszillation gewollt oder nicht?*
- C13 Hermite-Höhenverlust. *Entscheidung: analoges Nachdunkeln behalten oder neutral?*
- C9 Sicherheitsgrenze ohne Limiter. *Empfehlung: ja.*
- C11 Limiter nur auf Echos. *Entscheidung.*
- C12 LFO-Phase an Songposition koppeln. *Entscheidung (eher Feature).*
- C10 Denormal-Grenzwert streichen (FTZ ist im Plugin gesetzt). *Empfehlung: streichen.*
