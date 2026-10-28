# QUAD ECHO – DSP-Spezifikation (Audit Phase 0)

**Status: freigegeben (2026-10-28).** Die Spalte „Vorschlag“ gilt als Grenzwert. Die Spalte „Ist“ zeigt den Stand
vor dem Audit; den Stand danach zeigt [AUDIT.md](AUDIT.md), Abschnitt „Stand nach Phase 2“. Aktuelle Baseline: v5
(`tests/baseline/manifest-v5.tsv`).

Code-Stand: Commit `36fbeea` (Runde 3). Messrechner: i5-13600KF, Windows 11, MSVC 14.50, Release-Build.
Alle Ist-Werte stammen aus dem Harness (`tests/Harness.cpp`) und dem Python-Referenzmodell
(`tests/ref/delayunit_ref.py`). Aufrufe stehen in Abschnitt 7.

Begriffe: **Frist** = Zeit, die ein Audio-Block dauert (bei 192 kHz und 32 Samples: 166,7 µs). **dBFS** = Pegel
relativ zu Vollaussteuerung 1,0. **bitgenau** = jedes Sample hat exakt dasselbe Bitmuster.

---

## 1 Echtzeit (CPU)

Worst-Case-Preset (`worstCase()` im Harness): 4 Einheiten parallel, Mid/Side, HP+LP mit Resonanz 0,8 und Boost 6 dB,
Delay- und Filtermodulation, Hüllkurve, Drive +24 dB, Feedback 0,8, Place Dry, Binaural, Distanz-Laufzeit, frühe
Reflexionen, Noise mit Filtern und Knistern, globaler Filter mit Include Dry und Skip 1st echo, Ducking.

| Kennzahl bei 192 kHz, Block 32 (Frist 166,7 µs) | Ist | Vorschlag |
|---|---|---|
| Mittelwert im Dauerbetrieb, Worst Case | 17 % der Frist | ≤ 20 % |
| p99 (1 % der Blöcke sind langsamer) | 21 % (mit laufenden Fades 26 %) | ≤ 50 % |
| p99,9 | 30 % (mit Fades 32 %) | ≤ 75 % |
| Langsamster Block im Dauerbetrieb | 51 % | ≤ 100 % |
| Einzelblock bei einem Ereignis | **734 %** beim Ausschalten einer Einheit (1,0–1,2 ms, jedes Mal); Algorithmuswechsel 44 %, unauffällig | ≤ 100 % der Frist und ≤ 2 × p99 des Dauerbetriebs |
| Zyklen pro Sample, Worst Case | ≈ 3200 | nur Bericht |
| Speicher anfordern, Locks, Datei/Konsole im Audio-Thread | ungeprüft | nie (Test mit zählendem `operator new`) |

Zum Vergleich: Standardeinstellungen (Serial, 4 Einheiten) brauchen 6 % der Frist im Mittel; 48 kHz/Block 32 und
44,1 kHz/Block 512 liegen im Worst Case bei 4–5 %. Bei 48 kHz kostet das Ausschalten einer Einheit 0,24 ms (37 %).

**Messmethode (freigegeben am 2026-10-28):** CPU-Zeit des Threads (`QueryThreadCycleTime`, über die TSC-Frequenz in
µs umgerechnet) statt Wanduhr. Fester P-Kern, hohe Priorität, 5 Läufe zu je 20 s, Median über die Läufe, erste Sekunde
ausgelassen. Unterbrechungen durch Windows zählen so nicht mit; mit Wanduhr lagen die Max-Werte bei 380–1700 % und
täuschten u. a. eine Spitze beim Algorithmuswechsel vor. Aufruf: `QuadEchoTests --harness bench 20 5`.

## 2 Abweichung vom Referenzmodell

Das Modell rechnet eine DelayUnit in double nach, mit derselben Struktur wie der C++-Code (16er-Teilblöcke,
Hermite-Interpolation, Transitionen, Kanalmodi). Verglichen werden die Echos (ohne Durchlaufsignal). Anregung:
Rauschstoß und Impuls bei **−20 dBFS**, 4 Sample-Raten, Block 512. Angegeben ist der größte Betrag der Differenz.

Zwei Varianten des Modells:
- **Replik:** mit dem tanh, der im Plugin auch bei Drive 0 in der Schleife sitzt. Misst nur Rechengenauigkeit.
- **Linear:** ohne jede Sättigung. Misst zusätzlich, was der tanh am Klang ändert.

| Klasse | Fälle | Ist (Replik, schlechteste Rate) | Vorschlag |
|---|---|---|---|
| Ganzzahlige Verzögerung, statisch, auch mit Filtern | r01, r09, r10, r14 | −128 dBFS (r14 bei 192 kHz), sonst −139 bis −143 | ≤ −120 dBFS |
| Gebrochene Verzögerung, statisch | r02, r04, r05, r11 | **−67 dBFS** (r02 bei 192 kHz) | ≤ −110 dBFS |
| Moduliert (LFO, Hüllkurve) | r06, r07, r12, r13 | **−39 dBFS** (Hüllkurve bei 192 kHz) | ≤ −90 dBFS |
| Repitch nach Zeitsprung | r08 | **−11 dBFS** | ≤ −90 dBFS |
| Selbstoszillation (Schleifenverstärkung > 1) | r03 | −48 dBFS | kein Grenzwert; nur endlich und begrenzt |

Was die Ist-Werte bedeuten (Ursachen werden in Phase 1 belegt, hier nur der erste Befund):
- **Gebrochene Verzögerung:** Der Lese-Index ist ein float. Bei 100 ms und 192 kHz (19 270 Samples) ist seine
  Auflösung etwa 1/500 Sample. Das ist eine Zeitverschiebung von wenigen Nanosekunden, also unhörbar, aber sie
  verhindert −110 dBFS. Abhilfe wäre ein double-Index; das ändert die Ausgabe minimal und braucht deshalb Freigabe.
- **Repitch:** Die Glättung der Delay-Zeit (`l.cur += (target - l.cur) * rpCoef`) bleibt in float vor dem Ziel
  stehen. Gemessen ist die Delay-Zeit nach einem Sprung dauerhaft falsch (Vermutung 3 aus AUDIT-PROMPT.md bestätigt;
  Größe in Samples folgt in Phase 1). Das ist ein echter Fehler, kein Rundungsrauschen.
- **Hüllkurve bei 192 kHz:** Der Hüllkurvenfolger hat bei 192 kHz einen Koeffizienten von 3,5·10⁻⁵ und rechnet in
  float; er läuft dadurch messbar anders als in double.
- **Linear gegen Replik:** Schon bei −20 dBFS Eingang verändert der tanh bei Drive 0 die Echos um bis zu −57 dBFS
  (Klirr in der Schleife; r01: −64 dBFS gegenüber −140 dBFS in der Replik). Das ist eine Designfrage, siehe Entscheidung 2.

## 3 Determinismus

| Eigenschaft | Ist | Vorschlag |
|---|---|---|
| Zwei Renders mit frischer Instanz | **bitgenau** (804 von 804) | bitgenau, Pflicht |
| Dieselbe Instanz nach `reset()` bzw. erneutem `prepare()` | ungeprüft; Vermutung: Noise weicht ab (Zufalls-Seeds werden in `reset()` nicht zurückgesetzt) | bitgenau |
| Blockgrößen 1, 32, 64, 441, 512, 4096 und wechselnde Größen, Parameter konstant | ungeprüft (Vermutung 1) | bitgenau |
| Blockgrößen mit Automation | – | Änderung greift am nächsten Blockanfang; bis dahin bitgenau |
| Sample-Raten 44,1 / 48 / 88,2 / 96 / 176,4 / 192 kHz | ungeprüft | Delay-Zeit ±0,5 Sample; Frequenzgang der Filter ±0,5 dB bis 16 kHz; Zeitkonstanten ±1 % |
| Baseline (Abschnitt 7) | erstellt, 804 Renders | Refactor/Effizienz: 804 von 804 bitgenau. Klangänderung: nur nach Freigabe, dann neue Baseline-Version |

Bitgenauigkeit gilt für denselben Build (Compiler, Flags, Plattform). Mac und Windows vergleichen wir nicht bitgenau.

## 4 Latenz

| Eigenschaft | Ist | Vorschlag |
|---|---|---|
| Gemeldete Latenz | 0 | 0 |
| Global Mix 0, Pegel unter dem Limiter-Knie (0,8), mit und ohne Limiter | **bitgenau** gleich dem Eingang (Impuls und Sweep, alle 4 Raten) | bitgenau |
| Durchlaufsignal | nicht verzögert | nicht verzögert, Pflicht |

## 5 Robustheit

| Eigenschaft | Ist | Vorschlag |
|---|---|---|
| NaN/Inf im Eingang | ungeprüft (Vermutung 7: ein NaN bleibt dauerhaft in der Schleife) | Nicht-endliche Samples werden am Eingang der Engine zu 0. Endliche Werte bleiben unangetastet. Der Ausgang ist immer endlich. Spätestens einen Block danach normales Verhalten, ohne Eingriff des Hosts |
| Nicht-endlicher Zustand intern (z. B. durch extreme Automation) | ungeprüft | wird erkannt; nur die betroffene Einheit wird still und läuft im selben Block weiter |
| Ausgangspegel mit Limiter | ≤ 1,0 | ≤ 1,0 (0 dBFS) |
| Ausgangspegel ohne Limiter, Feedback 1,1 bei höchster Resonanz | +0,2 dBFS | ≤ +6 dBFS |
| Denormals | FTZ/DAZ über `ScopedNoDenormals` | zusätzlich ohne FTZ im Ausklang höchstens 1,5 × Rechenzeit |
| Tail-Länge | fest 10 s | berechnet aus Delay-Zeiten und Feedback; unendlich bei Feedback ≥ 1 oder Freeze. Nach der gemeldeten Zeit liegt der Ausgang unter −100 dBFS |
| Gespeicherter Zustand | Version 5 mit Umrechnung alter Stände | jede ältere Version wird geladen und umgerechnet; unbekannte neuere Versionen laden ohne Absturz |

## 6 Werkzeuge für die Abnahme

| Werkzeug | Verfügbar | Ziel |
|---|---|---|
| AddressSanitizer (MSVC `/fsanitize=address`) | ja | Tests und Harness ohne Befund |
| UndefinedBehaviorSanitizer | **nein**: braucht clang-cl, das nicht installiert ist (Entscheidung 3) | ohne Befund |
| clang-tidy | ja (VS-Installation auf C:) | ohne Befund in `Source/` für die vereinbarten Checks |
| MSVC `/W4 /analyze` | ja | ohne Warnung in `Source/` |
| pluginval Strictness 10, Debug und Release | ja (`Q:\QuadEcho\tools`) | grün |

## 7 Harness und Baseline (Phase 0, Schritt 2 und 3)

```
QuadEchoTests --harness baseline-write <ordner> [manifest]   # float32-WAVs + Prüfsummen
QuadEchoTests --harness baseline-check <ordner>              # neu rendern und vergleichen (Exit 1 bei Abweichung)
QuadEchoTests --harness manifest-check <manifest>            # nur Prüfsummen, ohne WAVs
QuadEchoTests --harness refdump <ordner>                     # Einzel-Einheit für das Python-Modell
QuadEchoTests --harness bench [sekunden] [laeufe]            # CPU-Zeit pro Block
python tests/ref/delayunit_ref.py <refdump-ordner> [--sat tanh|linear|both]
```

- **Baseline v1:** `Q:\QuadEcho\baseline\v1` (804 WAVs, 877 MB). Prüfsummen im Repo:
  `tests/baseline/manifest-v1.tsv`. Inhalt: jeder Algorithmus × Kanalmodus (30), jede Transition × Kanalmodus mit
  Zeitsprüngen (9), 28 Einzelfunktionen (Freeze, Ducking, Noise, globaler Filter, Binaural mit Bewegung, Drive,
  Feedback 1,1, alle LFO-Wellen, Sync, Mix 0/1, Place Dry, Solo/Mute, Ein-/Ausschalten, Algorithmuswechsel …),
  je mit Impuls, Sweep und Rauschen, bei 44,1/48/96/192 kHz, Block 512, je 1,5 s.
- **Referenz-Dumps:** `Q:\QuadEcho\refdump\v1` (14 Fälle × 4 Raten), Ergebnis `Q:\QuadEcho\refdump\residual-v1.tsv`.
- Die Baseline läuft mit Block 512. Weil 512 ein Vielfaches der internen Raster (16 und 256) ist, bleibt sie gültig,
  falls die Raster später an der absoluten Sample-Position ausgerichtet werden.

## 8 Zu entscheiden

1. **Die Zahlen** in den Spalten „Vorschlag“ (Abschnitte 1 bis 5).
2. **Referenz für Abschnitt 2:** Soll der tanh bei Drive 0 zum Soll gehören (Replik, mein Vorschlag: er schützt bei
   Feedback 1,1 und ist so gewollt), oder soll Drive 0 linear sein? Bei „linear“ würde sich der Klang bei hohem
   Feedback ändern.
3. **clang-cl installieren** (Visual Studio Build Tools auf `Q:\VS\BuildTools`, Komponente „C++ Clang Compiler for
   Windows“, einige GB auf Q:), damit UBSan läuft. Ohne clang-cl entfällt UBSan, ASan läuft trotzdem.
