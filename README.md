# QUAD ECHO

**von Younes Jamil** (c) 2026

VST3-Plugin, AU (Mac) und Standalone-App für **Windows und macOS** (JUCE 9.0.4, C++17) mit **vier Delay-Einheiten (A-D)**, wählbarem **Routing-Algorithmus** (wie bei Ableton Operator) und **3D-Platzierung** der Echos, optional **binaural** für Kopfhörer.

## Bauen

**Automatisch (Windows und Mac):** Auf Knopfdruck (GitHub → Actions → Build → „Run workflow") baut GitHub Actions ([.github/workflows/build.yml](.github/workflows/build.yml)) beide Systeme, lässt die DSP-Tests und pluginval (Strictness 10) laufen, prüft das AU mit `auval` und stellt `QUAD-ECHO-Windows.zip` und `QUAD-ECHO-macOS.zip` bereit (Actions → Lauf → Artifacts). Mac: Universal Binary (Apple Silicon + Intel), ab macOS 11, ad hoc signiert (ohne Apple-Konto). Ein Versions-Tag (`git tag v0.2.0` und `git push --tags`) legt zusätzlich ein GitHub-Release mit beiden Zips an.

**Lokal unter Windows:** Visual Studio 2026 (oder Build Tools mit C++), CMake >= 3.22, Git, Internet beim ersten Konfigurieren (JUCE wird per FetchContent geholt).

JUCE landet standardmäßig im Build-Ordner; ein anderer Ort ist mit `-DQUADECHO_DEPS_DIR=<pfad>` einstellbar.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel --target QuadEcho_VST3 QuadEcho_Standalone
```

Ergebnis: `build\QuadEcho_artefacts\Release\VST3\QUAD ECHO.vst3` und `...\Standalone\QUAD ECHO.exe`.

### Tests

```powershell
cmake --build build --config Release --parallel --target QuadEchoTests
build\QuadEchoTests_artefacts\Release\QuadEchoTests.exe test-renders
```

679 Prüfungen (inkl. Regressionstests zum DSP-Audit): Delay-Zeiten bei 44,1-192 kHz, Feedback, Modulation, Klickfreiheit, Algorithmen, 3D-Platzierung, Binaural, Doppler, Mute/Solo/Output-aus, Place Dry, analoger Drive, Drive in der Schleife, globaler Filter, Noise, Limiter, Feedback-Bremse, Dry/Wet-Konzept (100 % Wet ohne Original, auch in Ketten), Freeze, Ducking, Umrechnung alter Zustände, Platzierung bei ausgeschalteter Ausgangseinheit, Sync-Modi (Notes/Triplet/Dotted/16th), Offset und Sync-Modus je Kanal, Filtersteilheit 18 dB/Oktave, Filter-Boost, Random-LFO, Stereo-Phase des globalen Filters, Tape, Wow/Flutter, Routing-Modus Phase, Bell-Band (auch mit Filter-Modulation), Schritt-Muster und gezeichnete Stufen, Stereo-Bühne, Binaural-Helligkeit, Ducking mit Sidechain, Chorus-Rate/-Tiefe/-Ton, Solo-Pegel, Freeze All, Auto Level, Stereo/Enhance, Chorus, Fuzz auf NaN/Inf, CPU-Last. Zusätzlich 60 WAV-Renders nach `test-renders/` (nicht im Git).

UI-Pruefung ohne Fenster: `QuadEchoUiShot` rendert den Editor in ein PNG, z. B. `QuadEchoUiShot.exe noise.png expert=1 expertPage=1 nz_on=1 audio=1` (Parameter als `id=wert`, siehe Parameterliste).

Validierung des Plugins mit [pluginval](https://github.com/Tracktion/pluginval):

```powershell
pluginval.exe --strictness-level 10 --validate-in-process --validate "build\QuadEcho_artefacts\Release\VST3\QUAD ECHO.vst3"
```

## Installieren

Ausführlich für Windows und Mac (inklusive der Mac-Freigabe per Terminal): [INSTALL.md](INSTALL.md).

Kurz für Windows: den Ordner `QUAD ECHO.vst3` (aus dem Zip oder dem Build-Ordner) nach `C:\Program Files\Common Files\VST3\` kopieren (Admin-Rechte nötig) oder nach `%LOCALAPPDATA%\Programs\Common\VST3\` (pro Benutzer, wird von vielen Hosts gelesen). Danach im Host neu nach Plugins suchen lassen.

## Bedienung

### Routing (oben links)

Zehn Algorithmen als Blockdiagramm, Klick wählt aus. Wie bei Operator geht nur die unterste Reihe auf den Ausgang (türkise Striche unter den Kästchen). Speisen mehrere Einheiten eine andere, wird gemittelt. Der Wechsel während der Wiedergabe blendet den Ausgang kurz aus und wieder ein (je ca. 15 ms), das ist klickfrei. **Standard: Parallel**, mit nur Einheit A an.

| # | Name | Fluss | Ausgang |
|---|------|-------|---------|
| 1 | Serial | A > B > C > D | D |
| 2 | Dual Chain | A > B, C > D | B, D |
| 3 | Diamond | A > (B, C) > D | D |
| 4 | Chain+Solo | A > B > C, D | C, D |
| 5 | Fan Out | A > (B, C, D) | B, C, D |
| 6 | Fan In | (A, B, C) > D | D |
| 7 | Split End | A > B > (C, D) | C, D |
| 8 | Pair+Solo | A > B, C, D | B, C, D |
| 9 | Merge Chain | (A, B) > C > D | D |
| 10 | Parallel | A, B, C, D | alle |

Eine ausgeschaltete Einheit (`UNIT ON` aus) reicht ihr Eingangssignal unverändert durch. Zweige ohne aktive Einheit zählen nicht zur Ausgangssumme, damit das Dry-Signal nicht mehrfach addiert wird.

**Routing-Modus** (Leiste unter den Kacheln): `AUDIO` (Standard) speist eine Einheit mit dem Klang ihrer Quellen, sie echot also auch deren Echos. Bei `PHASE` bekommt jede gespeiste Einheit stattdessen den Eingang, und die **Echos ihrer Quellen verschieben nur ihre Delay-Zeit** (Phasenmodulation, FM-artig; bei "Serial" eine Modulationskette, hörbar ist D). `PHASE + DRY` moduliert mit dem ganzen Ausgang der Quelle inklusive Original: jeder Ton verbiegt sich selbst. `Depth` daneben: Verschiebung bei Vollaussteuerung, quadratischer Regler bis 10 ms (Standard 0,63 ms). Der Wechsel blendet wie beim Algorithmus kurz aus und ein.

### Zeit (Karte TIME)

Wie bei Ableton Echo: Mit `SYNC` folgen die Zeiten dem Tempo. Die kleinen Symbole über jedem Zeitregler wählen den **Sync-Modus dieser Seite**: `Notes` (gerade Notenwerte), `Triplet`, `Dotted` oder `16th` (Zeit als Anzahl Sechzehntel, 1-16, angezeigt als ganze Zahl; **Standard**). Der Zeitregler geht durch die Werte des Modus. Im Mid/Side-Modus heißen die Seiten M und S statt L und R. Beim Wechsel zwischen Notes, Triplet und Dotted bleibt die Grundnote (1/8 -> 1/8T -> 1/8.). Ohne `SYNC` gelten Millisekunden.

Am Anfang ist jede Einheit ein **Single-Delay**: `LINK` leuchtet, der rechte Regler ist ausgegraut und folgt dem linken. **`LINK` aus** gibt die rechte Zeit und ihren Sync-Modus frei.

Unter jedem Zeitregler steht der **Offset** dieser Seite als kleines Feld (z. B. `+0%`, wie bei Ableton Echo): verschiebt die Zeit um -50 bis +50 % (Swing), auch im Single-Delay. Bei 0 gedimmt, sonst in der Farbe der Einheit. Ziehen, Klick zum Eintippen, Doppelklick = 0.

### Filter als EQ-Kurve

Feedback-Filter, globaler Filter und Noise-Filter sind Kurven mit zwei Punkten (HP und LP), Steilheit **18 dB pro Oktave**. **Ziehen:** links/rechts = Frequenz, nach oben = **Boost** an der Grenzfrequenz (Glocke, bis +18 dB, wandert mit der Modulation mit; ganz unten sitzt der Punkt auf der Filterkurve). **Mit Alt** hoch/runter = Güte Q (Resonanz und Breite der Glocke). Shift = fein, **Doppelklick** = Standard. An und aus über die Kästchen **HP** / **LP** oben rechts (aus = hohler Punkt). HP und LP haben je eine eigene Resonanz. Die Werte stehen klein über der Kurve.

**Bell-Band:** Doppelklick auf eine freie Stelle der Kurve setzt ein freies Glockenband dorthin (Raute; Frequenz und Gain von der Mausposition). Ziehen = Frequenz und Gain (-18 bis +18 dB), Alt = Breite (Q 0,3-10), Doppelklick auf die Raute entfernt es. In der Feedback-Schleife wirkt es pro Wiederholung wie HP und LP (`STABLE` gleicht auch seine Anhebung aus). Beim Ziehen und wenn die Maus über der Raute steht, zeigt sie unter dem Gain auch Frequenz und Q. Das Kästchen `BELL~` (oben links in der Kurve, nur mit Bell-Band) lässt die Bell-Frequenz der Filter-Modulation folgen wie HP und LP; aus = feste Frequenz.

### Symbole statt Auswahl-Menüs

LFO-Formen (Sinus, Dreieck, Säge auf/ab, Rechteck, **Random**: weich gleitende Zufallswerte), Sync-Modus, Channel Mode (Stereo: zwei Spuren, Ping Pong: Ball zwischen zwei Seiten, M/S) und Transition werden als Symbole gewählt. Notenwerte (Delay-Zeit, LFO-Sync) sind Drehregler.

### Modulation (Seite MODULATION)

Jede Einheit hat einen LFO (Form, Rate oder Notenwert mit `LFO SYNC`, `φ Phase R` für die rechte Seite, `φ Filter` für die Filterbewegung) und einen Hüllkurvenfolger. **Envelope mix** mischt die Quelle: 0 = LFO, 100 % = Lautstärke am Eingang der Einheit. **Delay depth** (bis 10 ms) und **Filter depth** (bis +/-2 Oktaven) bestimmen die Wirkung. **LFO ×** und **ENV ×** (0,1-10, Standard 1) vervielfachen den LFO- bzw. Hüllkurven-Anteil, auf Delay und Filter; mit beiden auf 10 reicht die Delay-Modulation bis 100 ms.

**PATTERN** ersetzt die LFO-Welle durch ein Schritt-Muster (`STAIRS`, `ZIGZAG`, `SYNCOPATED`, `RANDOM`; `OFF` = normale Welle). Ein LFO-Zyklus ist ein Muster mit 8 oder 16 Stufen; mit `LFO SYNC` auf 1 Takt also 16tel. `Glide` lässt die Delay-Zeit von Stufe zu Stufe gleiten (0 = springt), `Seed` und `NEW SEED` wählen das Random-Muster. **`DRAW`** ersetzt das Muster durch eigene Stufen (auch bei `OFF`): Sie werden in der Karte darunter gezeichnet (Klicken/Ziehen setzt die Stufen, auch schnell quer darüber; Doppelklick = 0), bei 8 Stufen gelten nur die ersten 8. Jede Stufe ist ein eigener Host-Parameter.

Die **Live-Modulation** darunter zeigt die letzten 4 Sekunden: LFO, Hüllkurve und die Wirkung auf Delay-Zeit und Filter.

### Original und Echos (Dry/Wet)

Jede Einheit arbeitet wie ein Effektpedal: Das durchlaufende Signal bleibt unverändert, die Echos kommen mit dem **Echo level** der Einheit dazu. In einer Kette hört die nächste Einheit also das Original und die Echos davor.

Wie viel **Original** am Ausgang zu hören ist, bestimmt allein **Global Dry/Wet** (Equal-Power): 0 % = nur Original, 50 % = gleich laut (Standard), 100 % = nur Echos, z. B. auf einem Send-Kanal. Das gilt in jedem Algorithmus. Dafür läuft intern eine Spur des reinen Originals durch die Einheiten mit, und am Ausgang wird sie von den Echos getrennt.

Achtung bei langen Ketten: Vier Einheiten in Serie mit Echo level 100 % und viel Feedback addieren sich wie vier Pedale hintereinander und werden laut. Dann den Echo level der Einheiten senken oder `AUTO LEVEL` (Master) einschalten.

Alte Projekte (vor dieser Änderung gespeichert) werden beim Laden automatisch umgerechnet, so dass das Verhältnis von Original zu Echos erhalten bleibt.

### Einheiten-Leiste

Vier Chips A-D: Klick wählt die Einheit zum Bearbeiten, Klick auf den Buchstaben schaltet sie an (gefüllter Punkt) oder aus (Ring). Ein eisblauer Ring zeigt `FREEZE`. `M` = Mute (Einheit überall stumm), `S` = Solo (nur diese Einheit, an ihrer Position hören), darunter der Pegel der Echos dieser Einheit. Rechts daneben die Seiten `ECHO` und `MODULATION`. Die Auswahl bleibt beim Schließen und Öffnen des Fensters erhalten, ebenso die Fenstergröße.

**Freeze** (Karte Feedback): hält die Echos der Einheit als endlose Schleife. Der Eingang wird geschlossen, das Feedback steht auf genau 100 %, Filter und Sättigung werden umgangen und die Delay-Zeit rastet auf ganze Samples ein, damit die Schleife nicht mit jeder Runde dunkler wird. Zeitänderungen warten, bis Freeze wieder aus ist. Ein- und Ausschalten sind weich (50 ms).

**TAPE** (Karte Feedback, oben rechts): Alles, was auf das „Band“ der Einheit geschrieben wird (Eingang plus Feedback), läuft durch eine Bandstufe: Pre-Emphasis, weiche Sättigung, De-Emphasis, Head Bump bei 100 Hz und Höhenverlust ab 10 kHz, auf höchstens 0 dB normiert. Das erste Echo geht einmal hindurch, jede Wiederholung erneut: laute Höhen sättigen früher, die Repeats werden wärmer und dunkler. Input Gain über 0 dB fährt das Band härter an. **Wow** (Wert-Pille links neben TAPE, immer aktiv, auch ohne TAPE): Bandlauf-Schwankung aus langsamem Wow (ca. 0,9 Hz) und schnellem Flutter (ca. 9 Hz), weich zufällig und reproduzierbar; bei 100 % etwa +-0,5 % Tonhöhe. In der Schleife leiert jede Wiederholung stärker, im Freeze aus.

**Mute (`M`)** stellt eine Einheit komplett stumm, egal wie sie geroutet ist (sie liefert auch nichts an nachfolgende Einheiten). `UNIT ON` aus ist dagegen ein Bypass: das Signal läuft unverändert durch. Der Output-Regler geht von -24 bis +12 dB und schaltet nichts ab.

### 3D-Pad (Mitte oben)

Zwei Ansichten, je nach `BINAURAL`. **Stereo (Binaural aus): Bühne.** Der Hörer sitzt unten vor der Bühne, X ist das Panorama, Y die Tiefe: unten die Bühnenkante (FRONT, nah und präsent), oben die Rückwand (BACK). **Binaural: Draufsicht, der Hörer sitzt in der Mitte**, oben vorne, unten hinten; die Ringe zeigen die Entfernung. Y = +1 heißt in beiden Ansichten „vorne“, die Bühne zeichnet das Feld nur gespiegelt. Punkte A-D ziehen: X = links/rechts, Y = vorne/hinten. **Shift-Ziehen oder Mausrad** ändert die Höhe Z (Punktgröße und Schatten), Doppelklick setzt Z auf 0.

Rechts daneben, in der Karte **POSITION & ROOM**, stehen die genauen Werte X/Y/Z je Einheit (und `N` für die Noise): ziehen oder per Doppelklick eintippen, die Leiste unter der Zahl zeigt die Richtung von der Mitte aus. Darunter die Raum-Schalter (siehe unten).

Nur Einheiten, die im Algorithmus zum Ausgang gehören, haben eine hörbare Position. Alle anderen Punkte sind ausgegraut (gestrichelter Ring) und nicht ziehbar. Bei "Parallel" sind alle vier aktiv, bei "Serial" nur D. **Ist eine Ausgangseinheit aus**, reicht sie nur durch: Dann übernimmt die Einheit davor ihre Position (bei "Serial" mit D aus also C, mit C und D aus B). Speisen mehrere Einheiten die ausgeschaltete, werden sie alle zu gleichen Teilen platziert. Ausgänge, in deren Zweig noch gar nichts an ist, bleiben ziehbar, damit man sie vorab platzieren kann.

**Z (Höhe):** In Stereo färbt sie den Klang (Peak oben bzw. Senke unten zwischen 6 und 10 kHz, +/-5 dB), ohne Lautstärke oder Entfernung zu ändern. Im Binaural-Modus kommt die Höhe aus der gemessenen HRTF (+/-45 Grad).

### Psychoakustik der Platzierung

Im Raum platziert werden die **Echos**. Das Original bleibt in der Mitte. Mit `PLACE DRY` wandert der Anteil des Originals, der über diese Einheit läuft, **nur per Pegel** zur linken oder rechten Seite der Einheit, ohne Entfernung, Laufzeit, Luftfilter und Reflexionen. So bleibt das Original gleich laut und färbt sich nicht: Ein verzögerter oder gefilterter Teil würde sich mit dem Rest des Originals zu einem Kammfilter überlagern.

- **Entfernung (Stereo-Bühne):** Die vordere Hälfte bleibt präsent (Mitte ca. -1,8 dB), nach hinten bis 2,8 m (-9 dB), dazu weniger Höhen, schmaler und mehr frühe Reflexionen; X ändert den Pegel nicht.
- **Entfernung (Binaural):** Bis zum 2. Ring bleibt eine Quelle nah und präsent (1 bis 1,4 m, höchstens -3 dB, kaum Luftdämpfung). Danach greift die Entfernung steil: äußerer Ring 4 m (-12 dB), Ecken ca. 5,6 m (-15 dB). Dazu Luftabsorption (Tiefpass), schmalere Stereobreite, optional frühe Reflexionen und Laufzeit.
- **Stereo-Modus:** Equal-Power-Pan nach X, Laufzeitdifferenz bis 0,6 ms, Höhenverlust am abgewandten Ohr. Ein „hinter mir“ gibt es im Stereofeld nicht, Y ist dort die Bühnentiefe.
- **Binaural (Kopfhörer):** gemessene HRTF eines Kunstkopfs (MIT KEMAR, siehe Danksagung), diffusfeld-entzerrt: Pegel- und Klangunterschiede der Ohren, vorne/hinten und Höhe wie gemessen, zwischen den Messrichtungen interpoliert und beim Bewegen weich überblendet. Laufzeitdifferenz nach Woodworth (bis 0,66 ms). Nur mit Kopfhörern sinnvoll, auf Lautsprechern klingt es seltsam. `Tone` unter `BINAURAL` macht den Binaural-Klang heller oder dunkler (Kuhschwanz ab 4 kHz, -6 bis +6 dB, nur im Binaural-Anteil).
- Alle Positionen werden mit 50 ms geglättet, es gibt kein Zippern.

### Globale Regler

**MASTER** (links unter dem Routing):

| Regler | Wirkung |
|--------|---------|
| Global Dry/Wet | der einzige Regler für das Original: 0 % nur Original, 100 % nur Echos |
| Global Output | Master-Pegel hinter dem Routing |
| Feedback Scale | skaliert die Feedbacks aller vier Einheiten gemeinsam, 0-200 % (100 % = wie eingestellt; runterziehen lässt die Echos ausklingen, über 100 % wachsen sie bis in die Sättigung) |
| LIMITER | weicher Limiter am Ende, hält Spitzen unter 0 dBFS |
| AUTO LEVEL | hält Ketten etwa so laut wie eine einzelne Einheit: der Echo-Anteil jeder Einheit, die von einer anderen gespeist wird, wird um das zurückgenommen, was sie hinzufügt (aus Echo-Pegel und Feedback). Aus (Standard) = Pedal-Verhalten, Ketten werden lauter |
| FREEZE ALL | friert alle Einheiten auf einmal ein (zusätzlich zu ihren eigenen `FREEZE`-Schaltern); die Chips zeigen den eisblauen Ring |
| ON DRY | Limiter auch auf das Original. Aus (Standard): das Original bleibt unangetastet, nur die Echos werden in den verbleibenden Platz unter 0 dBFS gedrückt. Ältere Projekte laden mit ON DRY an und klingen wie vorher. Unabhängig davon begrenzt eine Notbremse den Ausgang immer auf höchstens +6 dBFS (darunter bis +4 dBFS unverändert). |

**POSITION & ROOM** (rechts neben dem Pad):

| Regler | Wirkung |
|--------|---------|
| BINAURAL | Kopfhörer-Modus |
| EARLY REFL. | drei sehr kurze Reflexionen (5-13 ms), Anteil steigt mit der Entfernung, kein Hall |
| DIST. DELAY | Laufzeit Abstand/343 m/s (aus: Sync-Delays bleiben exakt am Raster) |
| Doppler | Tonhöhenänderung beim Verschieben von Punkten (0 = keine, 100 % = realistisch, 200 % = übertrieben); wirkt nur mit `DIST. DELAY` und ist ohne abgedunkelt |

**COMPACT** (Kopfzeile) klappt Routing, Master, Pad und Position & Room weg, das Fenster wird rund 300 px niedriger; Einheiten und Expert-Bereich rücken hoch. Regler, die gerade nichts bewirken (z. B. Filter-Tiefe ohne Filter, Noise-Regler bei Noise aus, ON DRY ohne Limiter), sind abgedunkelt, bleiben aber einstellbar.

Oben rechts: Preset-Menü, Undo/Redo (auch Strg+Z, Strg+Y, Strg+Umschalt+Z) und das Master-Meter. Tooltips erklären jeden Regler. Die Oberfläche nutzt eine eingebaute Schrift (*Barlow Semi Condensed*, SIL Open Font License, `Resources/Fonts/`), damit sie auf Windows und Mac gleich aussieht; Texte in warmem Sand, Werte etwas heller. Alle Regler und Zahlenbalken bedienen sich gleich: **ziehen** ändert den Wert (**Shift** = zehnmal feiner, auch mitten im Ziehen), **einmal klicken** öffnet das Eingabefeld (Enter übernimmt, Esc bricht ab), **Doppelklick** setzt auf den Standardwert zurück.

### Presets

Werks-Presets: Init, Slap, Ping Pong, Dotted Eighth, Tape Chain, Wide Space, Binaural Orbit und die wilderen Tape Wobble (Band mit Wow), Phase Chain (Routing PHASE in Serie), Glitch Steps (Zufallsmuster im Takt), Chorus Space (Chorus in der Schleife, breites Stereo), Dub Siren (Feedback über 100 %, Resonanz, Bell-Band, Band), Filtered Dub (Ping Pong, Filter öffnet sich über 2 Takte) Distant Atmosphere (lange Echos hinten auf der Bühne, Chorus, breit) und Total Chaos (Serie mit PHASE + DRY, Zufallsmuster überall, Band, Chorus in der Schleife, Feedback über 100 %: laut, der Limiter bleibt an). Sie erscheinen im Preset-Menü des Plugins (nicht in der Programmliste des Hosts).

**Eigene Presets:** Im Preset-Menü speichert `Save current as...` den aktuellen Klang als Datei (`*.qepreset`) in `Dokumente/QUAD ECHO/Presets`; `Open presets folder` öffnet den Ordner. Eigene Presets stehen im Menü unter "My presets". Gespeichert werden alle Parameter mit der Zustandsversion; ältere Presets werden beim Laden umgerechnet, fehlende Parameter (neuere Funktionen) stehen auf neutral.

### Expert-Bereich (Knopf mit Zahnrad oben in der Kopfzeile)

Klappt unten aus, das Fenster wird höher. Gleicher Stil wie der Rest, Blau als Kennfarbe (wie der Knopf), kompakt gehalten. Vier Seiten:

- **GLOBAL FILTER:** HP und LP (je An/Aus, Frequenz, Resonanz) hinter allen Einheiten. Er filtert die Summe aller Echos einmal, **jedes Echo, auch das erste**. Der Feedback-Filter pro Einheit erreicht das erste Echo nicht, der globale schon. Das Original bleibt unberührt, außer `INCLUDE DRY` ist an (dann ist er ein Master-Filter). Mit `SKIP 1ST ECHO` bleibt das **erste Echo jeder Einheit ungefiltert** und nur die Wiederholungen (2., 3., ...) werden gefiltert. Eigene Modulation: LFO an/aus, Form, Rate/Sync, Tiefe, `φ Phase` (der rechte Filter bewegt sich versetzt zum linken: Stereo-Bewegung; 0° = beide gleich) und `Envelope` (0 = LFO, 100 % = Lautstärke des Eingangs bewegt den Filter). Bell-Band wie bei den Einheiten. Darunter die Live-Modulation (LFO, Hüllkurve, Filter).
- **NOISE:** Rauschquelle (Bandrauschen und Knistern) mit `Level` (100 % = intensiv), `Color` (braun - rosa - weiß), `Crackle`, `Width` und `Into echoes`. `Into echoes` überblendet wie ein Dry/Wet-Regler zwischen "direkt zum Ausgang" (0) und "in die Delays hinein" (100 %): dann wird die Noise mitgeechot und in der Feedback-Schleife gesättigt. Eigener Filter (HP/LP mit Resonanz) und eigene Modulation (LFO an/aus, Form, Rate/Sync, Filter-Tiefe, Pegel-Tiefe, `φ`, Hüllkurve des Eingangs). Die Noise hat einen **eigenen Punkt im 3D-Pad** (Symbol), ziehbar, solange sie an ist. Pad-Punkt und Positionszeile `N` sind sichtbar, wenn die Noise an ist oder der Expert-Modus aufgeklappt ist. Sie läuft unabhängig vom Routing und wird bei Solo stummgeschaltet. **AUTO-PAN** (eigene Karte): eigener Sinus-LFO für das Panorama der Noise mit `Amount` (0 = aus, 100 % = ganz links bis ganz rechts), `Frequency` oder Notenwert mit `SYNC`. Darunter die Live-Modulation (LFO, Hüllkurve, Filter, Pegel, Auto-Pan).
- **DUCKING:** Die Echos werden leiser, solange am Eingang gespielt wird, und kommen in den Pausen zurück (typisch für Gesang und Soli). `Threshold` (ab welchem Eingangspegel, weiches Knie +/-3 dB), `Amount` (wie weit, bis -40 dB), `Attack` (wie schnell sie abtauchen) und `Release` (wie schnell sie zurückkommen). Rechts zeigt `INPUT` den Eingangspegel mit dem Threshold als weiße Marke (auch bei ausgeschaltetem Ducking, zum Einstellen), darunter `ECHO REDUCTION`, wie weit gerade abgesenkt wird. Original und Noise bleiben unberührt. Mit `SIDECHAIN` statt `INPUT` steuert ein externes Signal das Ducking (z. B. die Kick oder der Gesang auf einer anderen Spur); die Anzeige heißt dann `SIDECHAIN`. In Ableton im Gerät den Sidechain aufklappen und die Quell-Spur wählen. Ohne verbundenen Sidechain wird nicht abgesenkt.
- **STEREO:** `Width` (0 = mono, 100 % = unverändert, 200 % = doppelt so breit) und `Enhance` (eine verzögerte, hochpassgefilterte Kopie der Mitte geht auf die Seiten: mehr Raum, verschwindet in Mono, bleibt also mono-kompatibel). Standardmäßig nur auf den Echos; `INCLUDE DRY` nimmt das Original mit. **ANALOG CHORUS** im Stil der klassischen Eimerketten-Chöre: Dreieck-LFO, rechts gegenläufig, dunkel und leicht gesättigt. Modus `I` (langsam, breit), `II` (schneller), `I + II` (schnelles, flaches Flirren), `Mix` (50 % = klassisch), `Rate` und `Depth` (Faktor auf Geschwindigkeit und Tiefe des Modus, x1 = Original), `Tone` (Tiefpass der Eimerkette, 9 kHz = Original; weniger = dunkler). `ECHOES`: einmal auf der Summe der Echos (mit einem Hauch Rauschen der Eimerkette); `IN LOOP`: in der Feedback-Schleife jeder Einheit, jede Wiederholung wird erneut gechorust (verschmiert die Delay-Zeit leicht, wie ein echter Chorus in der Schleife).

### Einheitliche Symbolik

Gleiche Funktion = gleiches Symbol und gleiche Farbe: **Dry** hat ein neutrales Ring-Punkt-Symbol in Cremeweiß (`GAIN ON DRY`, `PLACE DRY`, `INCLUDE DRY`), **Phase** `Ø` für Phasenumkehr (`Ø INVERT`) und `φ` für Phasenversatz, **Modulation** ist pink, **Filter** lila, **Noise** stahlblau. Die Karten haben einen einheitlich neutralen Grund. Auf der Echo-Seite ist die Farbe der Einheit der einzige Akzent, die kleine Marke vor jedem Kartentitel zeigt die Funktionsfarbe.

## Hinweise

- **Standalone:** Windows meldet "Audio input is muted to avoid feedback loop". Das ist der Schutz gegen Rückkopplung zwischen Lautsprecher und Mikro, kein Fehler. Mit `Settings...` lässt sich der Eingang freigeben.
- **Binaural** nur mit Kopfhörern verwenden.
- **Input-Gain / Drive:** Über 0 dB sättigt der Input weich mit leichtem Analog-Charakter (mehr Gain = mehr Drive, bis +24 dB). Bei 0 dB oder darunter bleibt der Input unverändert. Unter 0 dB wird außerdem die Sättigung in der Feedback-Schleife schwächer: Sie bekommt einen sauberen linearen Bereich, der mit sinkendem Gain wächst (bei -24 dB sauber bis -1,9 dBFS), die Obergrenze bleibt, sodass Feedback 110 % begrenzt bleibt. Bei 44,1/48 kHz laufen die Sättigungen mit doppelter Abtastrate (weniger "digitale" Spiegelfrequenzen). Mit `GAIN ON DRY` gilt es auch für den Anteil des Originals, der über diese Einheit läuft. Der Drive sitzt außerdem **in der Feedback-Schleife**: Jede Wiederholung läuft erneut durch die Sättigung, späte Repeats werden dichter und schmutziger (Space-Echo-/Pedal-Verhalten), bleiben aber stabil und klingen bei Feedback unter 100 % ab.
- **Envelope Mix** wirkt nur, wenn `Delay depth` oder `Filter depth` größer als 0 ist (der Filter muss dafür an sein). Die Hüllkurve folgt der Lautstärke des Eingangssignals der Einheit.

## Parameterliste

Je Einheit (Präfix `a_` bis `d_`, 60 Parameter), global 14 Parameter und im Expert-Bereich 68. Alle sind automatisierbar und werden im Host-Zustand gespeichert.

### Echo-Seite

| ID | Name | Bereich | Standard |
|----|------|---------|----------|
| `on` | Unit On | an/aus | A an, B-D aus |
| `mute` | Mute | an/aus | aus |
| `solo` | Solo | an/aus | aus |
| `sync` | Sync | an/aus | an |
| `link` | R folgt L (in der UI: Knopf `LINK` leuchtet bei Single-Delay) | an/aus | an (Single-Delay) |
| `timeL`, `timeR` | Time L / R | 1-16000 ms (16 s = 8 Takte bei 120 BPM) | 250 / 375 / 500 / 125 ms (A-D) |
| `divL`, `divR` | Division L / R | 1/32 bis 8 Takte inkl. triolisch und punktiert; ab einem Takt als Taktzahl (1, 1.5, 2 .. 8; 1.5 = punktiert); über 16 s bleibt die Zeit bei 16 s | A 1/8, B 1/8., C 1/4, D 1/16 |
| `mode` | Channel Mode | Stereo, Ping Pong, Mid/Side | Stereo |
| `offset` | Offset (Swing auf R) | -50..+50 % | 0 |
| `fb` | Feedback | 0-110 % (Soft-Saturation im Pfad) | 40 % |
| `fbInv` | Feedback Invert | an/aus | aus |
| `inGain` | Input Gain (über 0 dB zunehmend Analog-Sättigung) | -24..+24 dB | 0 |
| `inDry` | Gain on Dry | an/aus | aus |
| `posDry` | Place Dry (Original an die Position der Einheit) | an/aus | aus |
| `hpOn`, `lpOn` | HP On / LP On (nur im Feedbackpfad) | an/aus | beide aus |
| `hp`, `lp` | HP / LP Freq | 20-8000 Hz / 200-20000 Hz | 100 Hz / 12 kHz |
| `res` | Resonance | 0-100 % | 20 % |
| `trans` | Transition | Repitch, Fade, Jump | Fade |
| `mix` | Echo Level (das durchlaufende Signal bleibt) | 0-100 % | 100 % |
| `freeze` | Freeze (Schleife halten) | an/aus | aus |
| `out` | Output | -24..+12 dB | 0 |
| `width` | Width | 0-200 % | 100 % |
| `syncMode`, `syncModeR` | Sync Mode L / R | Notes, Triplet, Dotted, 16th | 16th |
| `offsetL` | Offset L (Swing auf L; `offset` = Offset R) | -50..+50 % | 0 |
| `hpRes` | HP Resonance (`res` = LP Resonance) | 0-100 % (Q 0,71-4,7) | 20 % |
| `hpBoost`, `lpBoost` | Boost an der Grenzfrequenz (auch `gf_`, `nz_`) | 0-18 dB | 0 |
| `sixL`, `sixR` | 16ths L / R (im Modus 16th) | 1-16 | 2 / 3 / 4 / 1 (A-D) |

### Modulations-Seite

| ID | Name | Bereich | Standard |
|----|------|---------|----------|
| `lfoOn` | LFO On (Hüllkurve moduliert weiter) | an/aus | an |
| `wave` | LFO Wave | Sine, Triangle, Saw Up, Saw Down, Square | Sine |
| `rate` | LFO Rate | 0,01-20 Hz | 1 Hz |
| `lfoSync` | LFO Sync (folgt während der Wiedergabe der Songposition: Phase 0 auf dem Notenraster ab Songbeginn) | an/aus | aus |
| `lfoDiv` | LFO Division | 1/32 bis 24 Takte (1, 1.5, 2, 3 .. 24) | 1/4 |
| `phase` | LFO Phase R | 0-360 Grad | 0 |
| `fPhase` | LFO Phase Filter (Filter-Modulation gegen Delay-Modulation verschieben) | 0-360 Grad | 0 |
| `pattern`, `steps`, `glide`, `seed` | Schritt-Muster statt der LFO-Welle (Off, Stairs, Zigzag, Syncopated, Random; ein LFO-Zyklus = ein Muster), Stufen, Glide (0 = springt), Seed für Random (Taste NEW SEED) | 5 Stufen, 8/16, 0-100 %, 1-999 | Off, 16, 0, 1 |
| `bellOn`, `bellHz`, `bellGain`, `bellQ` | Freies Bell-Band in der Feedback-Schleife (Doppelklick auf eine freie Stelle der Filterkurve; ziehen = Frequenz/Gain, Alt = Q, Doppelklick auf das Band entfernt es). Gleich für `gf_bell*` und `nz_bell*` | aus, 20 Hz-20 kHz, -18..+18 dB, Q 0,3-10 | aus, 1 kHz, 0 dB, 1 |
| `bellMod` | Bell-Frequenz folgt der Filter-Modulation (Kästchen `BELL~`). Gleich für `gf_bellMod` und `nz_bellMod` | an/aus | aus |
| `patCustom`, `step1` .. `step16` | DRAW: gezeichnete Stufen statt des Musters, Wert je Stufe | an/aus, -1..+1 | aus, 0 |
| `wow` | Wow/Flutter (Bandlauf-Schwankung der Lesestelle, unabhängig vom LFO) | 0-100 % | 0 |
| `tape` | Tape: Schreibweg (Eingang + Feedback) mit Band-Charakter (Pre-/De-Emphasis, weiche Sättigung, Head Bump, Höhenverlust); jede Wiederholung läuft erneut durchs Band | an/aus | aus |
| `lfoAmt`, `envAmt` | LFO Factor, Env Factor (Faktor auf LFO- bzw. Hüllkurven-Anteil, wirkt auf Delay und Filter; ersetzt seit Zustandsversion 8 den Schalter `x4`) | 0,1-10 | 1 |
| `dDepth` | Mod Delay Depth | 0-100 % (bis 10 ms, mal Faktor bis 100 ms) | 0 |
| `fDepth` | Mod Filter Depth | 0-100 % (bis +/-2 Oktaven) | 0 |
| `env` | Envelope Mix | 0-100 % (Eingangspegel statt/zusätzlich zum LFO) | 0 |

### Position

| ID | Name | Bereich | Standard A / B / C / D |
|----|------|---------|------------------------|
| `posX` | Position X | -1 (links) .. +1 (rechts) | -0,4 / -0,15 / 0,15 / 0,4 |
| `posY` | Position Y | -1 (hinten) .. +1 (vorne) | 0,3 / 0,6 / 0,6 / 0,3 |
| `posZ` | Position Z | -1 (tief) .. +1 (hoch) | 0 |

### Global

| ID | Name | Bereich | Standard |
|----|------|---------|----------|
| `algo` | Algorithm | 10 Algorithmen | Parallel |
| `gMix` | Global Dry/Wet (einziger Regler für das Original) | 0-100 % | 50 % |
| `gOut` | Global Output | -24..+12 dB | 0 |
| `freezeAll` | Freeze All (alle Einheiten halten) | an/aus | aus |
| `autoLevel` | Auto Level (Ketten nicht lauter als eine Einheit) | an/aus | aus |
| `routeMode` | Routing-Modus: Audio (Einheiten speisen sich), Phase (die Echos der Quelle verschieben die Lesestelle der gespeisten Einheit, die den Eingang bekommt), Phase + Dry (ganzer Ausgang der Quelle inkl. Original) | 3 Stufen | Audio |
| `routeDepth` | Phase Depth (Lese-Verschiebung bei Vollaussteuerung, quadratisch: 25 % = 0,6 ms, 100 % = 10 ms) | 0-100 % | 25 % |
| `gFb` | Feedback Scale (über 100 % wachsen die Wiederholungen bis in die Sättigung) | 0-200 % | 100 % |
| `doppler` | Doppler | 0-200 % | 100 % |
| `binaural` | Binaural (Headphones) | an/aus | aus |
| `binTone` | Binaural Tone (High-Shelf ab 4 kHz, nur im Binaural-Anteil) | -6..+6 dB | 0 dB |
| `distDelay` | Distance Delay | an/aus | aus |
| `early` | Early Reflections | an/aus | an |
| `limiter` | Soft Limiter | an/aus | an |
| `limitDry` | Limiter On Dry (aus: Original unangetastet) | an/aus | aus |

### Expert: globaler Filter (`gf_*`), Noise (`nz_*`) und Ducking (`dk_*`)

| ID | Name | Bereich | Standard |
|----|------|---------|----------|
| `gf_hpOn`, `gf_lpOn` | Globaler Filter HP / LP an | an/aus | aus |
| `gf_hp`, `gf_lp`, `gf_res` | Frequenzen und Resonanz | 20-8000 Hz, 200-20000 Hz, 0-100 % | 100 Hz, 12 kHz, 20 % |
| `gf_inclDry` | Include dry (auch das Original filtern) | an/aus | aus |
| `gf_skipFirst` | Skip 1st echo (erstes Echo jeder Einheit nicht filtern) | an/aus | aus |
| `gf_lfoOn`, `gf_wave`, `gf_rate`, `gf_sync`, `gf_div` | LFO an, Form, Rate (0,01-20 Hz), Sync, Division | | an, Sine, 0,5 Hz, aus, 1/4 |
| `gf_depth`, `gf_phase` | Modulationstiefe (bis +/-2 Oktaven), Phase (Versatz rechts gegen links) | 0-100 %, 0-360 Grad | 0, 0 |
| `gf_env` | Envelope Mix: Modulationsquelle 0 = LFO, 100 % = Lautstärke des Eingangs (lauter = Filter höher) | 0-100 % | 0 |
| `nz_on` | Noise an | an/aus | aus |
| `nz_level`, `nz_color`, `nz_crackle`, `nz_width` | Pegel, Farbe (0 braun, 0,5 rosa, 1 weiß), Knistern, Breite | 0-100 % | 30 %, 50 %, 0, 100 % |
| `nz_into` | Into echoes (0 direkt, 100 % nur in die Delays) | 0-100 % | 0 |
| `nz_hpOn`, `nz_lpOn`, `nz_hp`, `nz_lp`, `nz_res` | Noise-Filter | wie oben | aus, 200 Hz, 8 kHz, 20 % |
| `nz_lfoOn`, `nz_wave`, `nz_rate`, `nz_sync`, `nz_div` | Noise-LFO | wie oben | an, Sine, 0,5 Hz, aus, 1/4 |
| `nz_depthF`, `nz_depthL`, `nz_phase`, `nz_env` | Filter-Tiefe, Pegel-Tiefe, Phase (Pegel gegen Filter), Hüllkurve | 0-100 %, 0-360 Grad | 0 |
| `nz_posX`, `nz_posY`, `nz_posZ` | Position der Noise | -1..+1 | 0 / 0,2 / 0 |
| `nz_panAmt`, `nz_panRate`, `nz_panSync`, `nz_panDiv` | Auto-Pan der Noise (eigener Sinus-LFO): Amount (0 = aus, 100 % = ganz links bis ganz rechts), Frequenz, Sync, Notenwert | 0-100 %, 0,01-20 Hz | 0, 0,5 Hz, aus, 1/4 |
| `st_width`, `st_enhance`, `st_inclDry` | Stereo: Breite, Enhance, auch das Original | 0-200 %, 0-100 %, an/aus | 100 %, 0, aus |
| `ch_on`, `ch_mode`, `ch_mix`, `ch_place` | Analoger Chorus: an, Modus (I, II, I + II), Mix, Platz (On Echoes, In Loop) | | aus, I, 50 %, On Echoes |
| `ch_rate`, `ch_depth`, `ch_tone` | Chorus: Geschwindigkeit und Tiefe (Faktor auf den Modus), Tiefpass der Eimerkette | x0,25-x4, x0-x2, 2-18 kHz | x1, x1, 9 kHz |
| `dk_on` | Ducking an | an/aus | aus |
| `dk_source` | Steuersignal des Ducking: Eingang oder externer Sidechain | Input, Sidechain | Input |
| `dk_threshold`, `dk_amount` | Threshold, Absenkung | -60..0 dB, 0-40 dB | -30 dB, 12 dB |
| `dk_attack`, `dk_release` | Attack, Release | 0,1-200 ms, 20-3000 ms | 10 ms, 400 ms |

## Technik

- `Source/DelayUnit.*`: eine Echo-Einheit. Hermite-interpolierte Delayline (max. 16 s, exakte Puffergröße, Lese-Index in double), Feedback mit HP/LP und `tanh`-Sättigung (bei 44,1/48 kHz 2-fach überabgetastet, `Source/Oversampler.h`), Übergangsmodi (Repitch glättet die Zeit, Fade und Jump blenden zwischen zwei Abgriffen), LFO, Hüllkurvenfolger.
- `Source/Tape.h`: Bandstufe für den Schreibweg einer Einheit (TAPE). `Source/Chorus.h`: analoger Chorus (Eimerketten-Stil).
- `Source/Spatializer.*`, `Source/Hrtf.h`: 3D-Platzierung (Stereo-Bühne und Binaural mit gemessener HRTF; Daten in `Resources/Hrtf`, erzeugt mit `tools/make_hrtf.py`).
- `Source/GlobalFilter.*`, `Source/NoiseSource.*`: globaler Filter und Noise-Quelle.
- `Source/QuadEngine.*`: vier Einheiten, Algorithmen, Mute/Solo, Original-Spur (trennt Original und Echos am Ausgang), gewichtete Ausgangssumme, globaler Mix, Ducking, Soft-Limiter, Pegel für die UI. Reines DSP ohne Plugin-Abhängigkeit, daher offline testbar.
- `Source/Params.*`, `Source/Presets.*`: APVTS-Layout, lockfreies Lesen der Parameter, Start-Presets.
- `Source/StateMigration.h`: Zustandsversion (Attribut `qeVersion`) und Umrechnung älterer Zustände beim Laden.
- UI: `PluginEditor`, `UnitPanel`, `Widgets.h` (Echo-Anzeige, Modulationsanzeige, Chips), `PadComponent.h`, `RoutingSelector.h`, `UiCommon.h`, `UiExtras.h` (Tooltips, Pegel).
- Audio-Thread: keine Allokation, keine Locks, kein Logging; Denormals werden abgefangen (die Engine setzt FTZ/DAZ selbst); NaN/Inf vom Host wird zu Stille; alle Parameter geglättet. Ausgang unabhängig von der Blockgröße des Hosts (Steuerraster an der absoluten Sample-Position). Tail-Länge wird aus Zeiten, Feedback und Filtern berechnet (unendlich bei Dauerschwingung, Freeze oder Noise). Latenz 0. Mono- und Stereo-Bus (Mono läuft intern stereo und wird heruntergemischt). Tempo-Sync über den Host-Playhead (Standard 120 BPM ohne Host).
- CPU (Thread-CPU-Zeit, `QuadEchoTests --harness bench`): Standardeinstellungen etwa 2,3 % der Blockzeit bei 48 kHz und 6 % bei 192 kHz (Block 32); Worst Case (alles an) etwa 7 % bzw. 18 %.

## Danksagung

Der Binaural-Modus nutzt HRTF-Messungen eines KEMAR-Kunstkopfs: Bill Gardner und Keith Martin, *HRTF Measurements of a KEMAR Dummy-Head Microphone*, MIT Media Lab, 1994 (http://sound.media.mit.edu/resources/KEMAR.html). Die Daten sind frei nutzbar unter der Bedingung, dass die Autoren genannt werden; sie wurden für das Plugin diffusfeld-entzerrt und minimalphasig umgerechnet (`tools/make_hrtf.py`).

