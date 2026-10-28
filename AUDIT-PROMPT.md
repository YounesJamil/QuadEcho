# Audit-Prompt: QUAD ECHO auf professionellem DSP-Niveau

Ziel: QUAD ECHO (Source/) auf professionellem DSP-Niveau prüfen und verbessern – nicht mehr
Funktionen, sondern belegte Korrektheit, Robustheit und Echtzeit-Tauglichkeit. Runde 2 der
ROADMAP.md bleibt davon unberührt. Werkzeuge und Build-Artefakte nach Q:\QuadEcho.

## Phase 0 – Einfrieren und Spezifikation (noch keine Änderung am DSP)

1. Schreib eine Spezifikation mit Zahlen: CPU-Budget im schlimmsten Fall (192 kHz, Block 32),
   erlaubte Abweichung zum Referenzmodell in dBFS, Blockgrößen- und Render-Determinismus,
   Latenz 0, Verhalten bei NaN/Inf-Eingang. Leg mir die Zahlen zur Bestätigung vor.
2. Bau ein Referenz-Render-Harness ins Test-Binary: float32-WAVs (Impulse, Sweeps, Rauschen)
   für jeden Algorithmus, Kanalmodus und Transition, bei 44.1/48/96/192 kHz. Das ist die
   Baseline, gegen die jeder spätere Commit geprüft wird.
3. Schreib ein Referenzmodell einer DelayUnit in Python/numpy (double, linear, ohne Sättigung)
   und miss das Residuum der C++-Implementierung.

## Phase 1 – Messen, bevor du urteilst

Erstelle AUDIT.md mit Befund, Messwert, Ursache im Code (Datei:Zeile) und Vorschlag für:

- Invarianz: Blockgrößen 32/64/441/512/variabel, Sample-Raten, zwei Renders, Mix 0 gegen Dry,
  Transport-Sprünge (LFO-Phase gegen ppq).
- Numerik: float-Auflösung des Delay-Index und der Repitch-Glättung bei 4 s / 192 kHz.
- Stabilität: Schleifenverstärkung fb·|H_hp·H_lp|·Interpolator analytisch und gemessen, inkl.
  Resonanz-Maximum und Feedback 1.1.
- Klangtreue: Frequenzgang des Hermite-Interpolators je Nachkommaanteil, kumuliert über N
  Wiederholungen; Aliasing der Sättigung in der Schleife.
- Echtzeit: Zeitverteilung pro Block (p50/p99/max, Zyklen pro Sample), Spitzen durch reset()/
  clear() im Audio-Thread, Algorithmuswechsel, Crossfade-Phasen; Vektorisierung der inneren
  Schleifen (Compiler-Report).
- Robustheit: NaN/Inf, Denormals, extreme Automation, Tail-Länge, State-Versionierung.
- Werkzeuge: ASan/UBSan (clang-cl), clang-tidy, /W4 /analyze, pluginval 10 Debug+Release.

## Phase 2 – Umsetzen, in getrennten Commits

- Refactor/Effizienz: müssen gegen die Baseline bitgenau nullen.
- Klangverändernde Fixes: nur nach meiner Freigabe, je mit Vorher/Nachher-Messung.
- Jeder Fix bekommt einen Regressionstest, der vorher fehlschlug.

## Abnahme

Alle Null-Tests grün, CPU-Spitzen im Budget, Sanitizer und pluginval sauber, AUDIT.md mit
Zahlen vor/nach. Frag nach, bevor du etwas tust, das den Klang verändert.

---

## Anhang: Vermutungen aus einer ersten Code-Lektüre (ungemessen, in Phase 1 prüfen)

Stand: Commit 3fad2db, ergänzt nach Runde 3 (Echo-Pegel + Original-Spur, Freeze, Ducking,
Zustandsversion; siehe DECISIONS.md). Die Baseline in Phase 0 wird mit dem Stand nach Runde 3
erstellt. Nur der Punkt zum Soft-Limiter ist durch Lesen bestätigt, alles andere ist eine
Hypothese, die erst ein Test belegen oder widerlegen muss.

1. **Blockgrößen-Abhängigkeit:** Die 16er-Teilblöcke (DelayUnit, Spatializer) und die 256er-Chunks
   (QuadEngine::process) beginnen bei jedem processBlock neu. Steuerwerte (Filter-Cutoff,
   envMapped, Sättigungs-Koeffizienten, Raum-Geometrie, gfilter.beginChunk) werden dadurch je nach
   Host-Puffergröße an anderen Samples aktualisiert.
2. **LFO-Sync ohne Phase:** lfoSync nimmt nur die Rate aus dem Tempo, nicht die Phase aus der
   ppq-Position.
3. **Repitch-Glättung bleibt stehen:** `l.cur += (target - l.cur) * rpCoef` in float. Bei 4 s /
   192 kHz (d ≈ 768000, ulp 1/16) hält sie rechnerisch ~1200 Samples (~6 ms) vor dem Ziel an,
   bei 48 kHz ~75 Samples.
4. **Schleifenverstärkung:** Feedback bis 1.1, SVF-Q bis ≈ 4.7 (0.7071 + 4·res²). Am Cutoff ist
   die Verstärkung weit über 1, nur tanh begrenzt.
5. **Hermite in der Schleife:** dämpft Höhen bei Nachkommaanteil 0.5, kumuliert pro Wiederholung.
   tanh in der Schleife ohne Oversampling, Aliasing läuft mit um.
6. **Speicher-Spitze im Audio-Thread:** units[uu].reset() beim Ausschalten einer Einheit löscht
   4 Delay-Leitungen (2 × dl + 2 × first), zusammen ~4 MB bei 48 kHz und ~16 MB bei 192 kHz,
   in einem Block.
7. **NaN bleibt in der Schleife:** Die Denormal-Abfrage `abs(w) < 1e-18f` fängt kein NaN ab, ein
   NaN vom Host bleibt dauerhaft im Feedback.
8. **Soft-Limiter trifft Dry (bestätigt):** softLimit wirkt auf den ganzen Ausgang inkl. Dry, ab
   0.8 (−1.9 dBFS), standardmäßig an, also auch bei Mix 0.
9. **Tail-Länge:** getTailLengthSeconds ist fest 10 s, Feedback bis 1.1 erlaubt endlosen Nachhall.
10. **Rechenlast pro Sample:** 3× std::sin (lfoWave) und 2× tanh (loopSat) pro Sample und Einheit,
    auch bei Drive 0 und ausgeschaltetem LFO.
11. **Benchmark:** misst einen einzigen Durchlauf mit Wanduhr bei Block 512, nur Mittelwert, keine
    Spitzen und keine kleinen Blockgrößen. In Runde 3 schwankte er zwischen Läufen um ±15 %
    (alter und neuer Code gleich), Aussagen zur Effizienz brauchen also bessere Messungen.

Neu seit Runde 3 (ebenfalls prüfen):

12. **Original-Spur:** Echo = Ausgang - Original-Spur. Mit `Gain on Dry` und Drive ist der Dry-Weg
    nichtlinear; der Rest (Intermodulation) landet im Echo-Anteil. Größe messen.
13. **Freeze:** Einrasten auf ganze Samples per Fade (DelayUnit::readLine). Prüfen:
    Pegel- und Höhenstabilität über Minuten, Klickfreiheit bei allen Transition-Modi, Verhalten
    bei Feedback-Invert (Polarität wechselt jede Runde).
14. **Ducking:** `log10` und `pow` pro Sample, solange es an ist. Detektor nur Spitzenwert mit
    fester Release-Zeit von 30 ms: bei tiefen Tönen Restwelligkeit der Absenkung messen.
15. **Lange Ketten:** Serie mit Echo-Pegel 100 % und Feedback summiert Kammfilter (bis ca. +34 dB
    an Resonanzen bei 4 Stufen und Feedback 0,4). Pegel vor dem Limiter messen.
