# Zusammenfassung (Stand nach Runde 2)

QUAD ECHO von Younes Jamil.

## Was fertig ist

**Runde 1 (M1-M7):** vier Echo-Einheiten mit allen geforderten Funktionen, Modulation, 10 Routing-Algorithmen, 3D-Platzierung, UI, Tests, Release.

**Runde 2 (nach eurem Feedback):**
- **Binaural-Modus** (Kugel-Kopf-Modell, nur Kopfhörer) und **Pad mit Hörer in der Mitte** (vorne/hinten).
- **Mute** (Einheit überall stumm, auch für nachfolgende Einheiten) und **Solo** je Einheit.
- **Standard:** Algorithmus "Parallel", nur Einheit A an, Delay-Zeiten tempo-synchron. HP und LP im Feedback sind einzeln schaltbar und standardmäßig **aus**.
- **LFO-Schalter** (Hüllkurve moduliert weiter) und **Filter-Phase** (Filter-Modulation gegen die Delay-Modulation verschieben). Das Scope zeigt beides, mit dünneren Kurven und weniger Leuchten.
- **Output** geht von -24 bis +12 dB (kein -inf); zum Abschalten gibt es `UNIT ON` (Bypass) und `M` (Mute).
- **Drive in der Feedback-Schleife** (Band-/Pedal-Charakter): jede Wiederholung läuft erneut durch die Sättigung.
- **Expert-Bereich** (unten ausklappbar, dunkelblauer Look): **Globaler Filter** (jedes Echo, auch das erste; `Include dry` macht ihn zum Master-Filter; eigene Modulation) und **Noise-Quelle** (Bandrauschen und Knistern, Farbe, eigener Filter, eigene Modulation, `Into echoes`, eigener Punkt im 3D-Pad). `SKIP 1ST ECHO` lässt das erste Echo jeder Einheit ungefiltert. Der Expert-Knopf (Zahnrad) sitzt oben in der Kopfzeile, der Bereich ist kompakt.
- **Einheitliche Symbolik und Vintage-Optik:** warmes Schwarz, Creme und gedämpfte Farben; Dry-Ring-Symbol in Cremeweiß, Symbole mittig im Knopf, `Ø` und `φ` für Phase, Modulation pink, Noise stahlblau.
- **Place Dry** je Einheit: Positionierung wirkt auch auf das Original (Standard aus).
- **Input-Gain mit Analog-Gefühl:** Über 0 dB sättigt der Input weich (bis +24 dB), bei 0 dB bleibt alles unverändert.
- **Modulationsseite:** eigene Farbe (Pink) für LFO, Tiefe und Hüllkurve, dazu ein echtes **Live-Scope** im Oszilloskop-Stil mit LFO-Kurve, Hüllkurve und der Wirkung auf Delay und Filter in der Amplitude der jeweiligen Depth.
- **Das Original (Dry) bleibt unplatziert**, nur die Echos werden im Raum platziert.
- **Pad:** Punkte von Einheiten, die im Algorithmus nicht zum Ausgang gehen, sind ausgegraut und nicht ziehbar.
- **Soft-Limiter** am Ende, **globale Feedback-Bremse**, **Doppler-Regler**.
- **UI überarbeitet:** Einheiten-Leiste (An/Aus, Mute, Solo, Pegelbalken), Regler in Karten, Echo-Vorschau, Live-Modulationsanzeige (zeigt, was LFO und Hüllkurve gerade bewirken), Master-Meter, Tooltips, Undo/Redo, Preset-Menü (7 Start-Presets), gespeicherte Fenstergröße und Tab-Auswahl.

- **Fenster:** Standardgröße 960 Pixel breit (4 % kleiner als der Entwurf, skaliert frei). Die Höhe des Expert-Bereichs passt sich der gewählten Seite an.

**Runde 3 (Konzept und Optik, 2026-10-28):**
- **Ein Regler für das Original:** Die Einheiten haben statt Dry/Wet einen **Echo level** (das durchlaufende Signal bleibt, wie bei einem Pedal). Wie viel Original zu hören ist, bestimmt nur noch **Global Dry/Wet**; 100 % = nur Echos (Send-Betrieb), in jedem Algorithmus. Standard 50 % / 100 % klingt mit einer Einheit wie vorher. Alte Projekte werden beim Laden umgerechnet.
- **Freeze** je Einheit (Karte Feedback): hält die Echos endlos, ohne dass die Schleife dunkler wird.
- **Ducking** (Expert, dritte Seite): Echos tauchen ab, solange gespielt wird; Threshold, Amount, Attack, Release, Live-Anzeige.
- **Feedback Scale** statt "Global Feedback" (es ist eine Bremse, keine Selbstoszillation).
- **Entfernung:** bis zum 2. Ring präsent (höchstens -3 dB), danach steil bis -12 dB am äußeren Ring.
- **Routing:** Ist eine Ausgangseinheit aus, übernimmt die Einheit davor ihre Position (Serial mit D aus: C ist platziert und ziehbar).
- **Bedienung:** ziehen (Shift = feiner), einmal klicken = Wert eintippen, Doppelklick = Standardwert; für alle Regler und Balken gleich.
- **Zeit wie bei Ableton Echo:** Sync-Modus-Menü (Notes, Triplet, Dotted, 16th; Standard 16th), Offset als ±-Balken unter den Zeiten.
- **Globaler Filter:** `φ Phase R` statt einer Phase ohne Bezug (rechts gegen links, Stereo-Bewegung).
- **Struktur:** Körnung als Material jeder Fläche (Hintergrund grob, Panels fein, Karten fast glatt), Licht von oben, flache Prägung, Knopfkörper, weicher Schatten.
- **Single-Delay am Anfang:** `LINK` aus, R ausgegraut; `LINK` an gibt R frei. **Offset je Kanal** als Zahlenfeld unter jedem Zeitregler.
- **Filter als EQ-Kurven** mit zwei Punkten (HP/LP, eigene Resonanz, 18 dB/Oktave, an/aus über Kästchen HP/LP), **Symbole** statt Auswahl-Menüs, neue LFO-Form **Random** (weich).
- **Sync-Modus je Seite** (kleine Symbole über jedem Zeitregler), **M/S** statt L/R im Mid/Side-Modus, Offset als kleines Feld nur mit dem Wert.
- **Echo-Vorschau** in Takten (1/2/4/8, wächst mit der Delay-Zeit), **Hüllkurve** im Live-Bild wächst mit Envelope mix, **Noise** 12 dB leiser.
- **Optik:** Expert-Bereich und Live-Modulation im gleichen Stil wie der Rest; quadratisches Pad mit echten Kreisen, kompakte Positionstabelle (Werte weiter eintippbar), Karten MASTER und POSITION & ROOM, Doppler abgedunkelt ohne Dist. Delay, neutrale Karten mit der Einheitsfarbe als Akzent, schmalere Chips mit deutlichem An/Aus und Echo-Pegel.

## Prüfergebnisse

- `QuadEchoTests`: **234 von 234** Prüfungen bestanden (Runde 3 neu: Filter-Boost, Filtersteilheit 18 dB/Oktave, Sync-Modus je Kanal, Random-LFO, Offset je Kanal, Sync-Modi, Stereo-Phase des globalen Filters, Platzierung bei ausgeschalteter Ausgangseinheit, Entfernungskurve, 100 % Wet ohne Original auch in Ketten, Gain on Dry am Ausgang, Freeze hält Pegel und Höhen und lässt nichts Neues hinein, Ducking -12 dB beim Spielen und Rückkehr in der Pause, Klickfreiheit beim Freeze, Umrechnung alter Zustände).
- pluginval, Strictness **10** (Maximum): SUCCESS (Runde 3).
- UI per Screenshot der Standalone-App geprüft (Echo- und Modulationsseite).
- CPU im Worst Case: 1,5 % (48 kHz) und 5,7 % (192 kHz) eines Kerns.

## Was nicht gemacht wurde

- **Nicht in einem echten Host getestet** (nur pluginval und Standalone), und **nichts gehört**: Alles wurde messtechnisch geprüft. Das Klangurteil, besonders zu Binaural, steht aus.
- **Nur Windows, nur VST3 und Standalone.** Kein AU, AAX, LV2, kein CI.
- **Echte HRTF-Messdaten** (statt Kugel-Kopf-Modell) und eine Höhenkorrektur für bestimmte Kopfhörer ("Binaural Brightness") sind nicht gebaut.
- **Presets** erscheinen nur im Preset-Menü des Plugins, nicht in der Programmliste des Hosts (siehe unten).
- **Gestrichen auf euren Wunsch:** Zeit-Verhältnisse koppeln, "Dry nur einmal", automatische Pegelanpassung.

## Bekannte Probleme und Entscheidungen

- **Presets nicht als Host-Programme:** Wenn die Presets als Programme des Plugins angemeldet sind, schlägt der pluginval-Test "Plugin state restoration" bei den Schaltern fehl (die Ursache liegt im Zusammenspiel von Programmwechsel und APVTS, nicht in der Zustandsspeicherung selbst). Damit pluginval auf Strictness 10 besteht, laufen die Presets nur über das Menü im Plugin.
- **Position nur am Ausgang hörbar** (Operator-Prinzip). Das ist euer Wunsch gewesen, beim Start ist "Parallel" gewählt, damit A allein sofort an seiner Position klingt. Bei "Serial" hört man nur D.
- **Hinten ohne Binaural:** Stereo kann nicht wirklich "hinten". Es klingt dumpfer und weiter weg. Den Effekt kann man verstärken.
- **Doppler braucht Distance Delay.** Ohne Distance Delay gibt es keine Laufzeit und damit auch keinen Doppler.
- **Algorithmuswechsel** blendet kurz aus und ein (je ca. 15 ms), kein echtes Crossfade.
- **Resonanz im Feedbackpfad** kann bei hohen Werten die Schleife über 100 % anheben. Die Sättigung hält sie begrenzt.
- **Global Dry/Wet ist Equal-Power.** Bei 50 % sind Original und Echos je -3 dB. Der Echo level der Einheiten ist linear.
- **Lange Ketten werden laut:** Vier Einheiten in Serie mit Echo level 100 % addieren sich wie Pedale hintereinander. Echo level senken.
- **Mono-Bus:** intern stereo, Ausgang wird heruntergemischt.
- **Tempo ohne Host:** 120 BPM. Die Tail-Länge ist mit 10 s angegeben.
- **Standard-Klang ist leise-ish:** Die Platzierung dämpft mit der Entfernung (A bei Y 0,3 etwa -7 dB). `Global Output` oder `Output` je Einheit gleichen das aus.
- **Build-Ordner und JUCE liegen auf Q:** (`Q:\QuadEcho`).
- Schätzwerte zum Nachjustieren in `Spatializer.cpp`: Entfernungsskala (1-4 m), Reflexionszeiten, Stärke der Rück-Dämpfung, Kopfschatten.

## Was ihr zuerst testen solltet

1. `dist\QUAD ECHO.vst3` laden: erscheint es, läuft es mit Audio?
2. Einheit A bei 1/8 (Standard), dann Punkt A im Pad nach links/rechts und nach vorne/hinten ziehen: stimmt das Panning, ist die Lautstärke ok?
3. `BINAURAL` mit Kopfhörern (auch DT 990 Pro) einschalten und einen Punkt hinter den Hörer ziehen: klingt das nach hinten, oder nur dumpfer? Ist es zu hell oder zu scharf?
4. Weitere Einheiten einschalten, Algorithmus wechseln (Serial, Diamond, Parallel): klickt etwas? Verhalten die Pad-Punkte sich wie erwartet (ausgegraut, wo sie nichts bewirken)?
5. Mute und Solo ausprobieren, `UNIT ON` aus als Bypass.
6. `Feedback Scale` herunterziehen, `LIMITER` an/aus bei hohem Feedback.
7. Modulationsseite: Delay depth oder Filter depth hochziehen, `Envelope mix` auf 100 %, dann die Live-Anzeige beobachten.
8. `DIST. DELAY` einschalten und einen Punkt verschieben: hört man den Doppler, ist er zu stark (Doppler-Regler)?
9. Presets durchprobieren, Projekt speichern und neu laden.
10. Strg+Z / Strg+Y im Plugin-Fenster und die Undo/Redo-Knöpfe.
11. Runde 3: `Global Dry/Wet` auf 100 % (nur Echos), auch bei "Serial". Ein altes Projekt laden: klingt es wie vorher?
12. `FREEZE` bei laufendem Signal: hält es sauber, ohne Klick, und klingt es nach dem Ausschalten normal aus?
13. Expert > `DUCKING` mit Gesang oder einem Solo: Passen Threshold und Release, kommen die Echos in den Pausen schön zurück?
14. Punkte im Pad vom Hörer weg ziehen (Boxen reichen): Bis zum 2. Ring sollte es präsent bleiben, danach deutlich weiter weg klingen. Passt der Übergang?
15. "Serial" wählen und D ausschalten: C muss im Pad ziehbar sein und die Echos an seiner Position klingen.
16. Zeit: Sync-Symbole (Notes, Triplet, Dotted, 16th) durchschalten, `LINK` an (R frei) und aus (Single-Delay), Offset-Felder ziehen. Regler: einmal klicken und Wert tippen, Doppelklick = Standardwert, Shift-Ziehen = fein.
17. EQ-Kurven: Punkte ziehen (links/rechts = Frequenz, hoch = Boost), Alt-Ziehen = Q, Kästchen HP/LP = an/aus, Doppelklick = Standard. LFO-Form Random auf der Modulationsseite.
