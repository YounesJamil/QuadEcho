# Auftrag: QUAD ECHO (VST3-Plugin)

Du arbeitest heute Nacht komplett autonom. Ich schlafe und kann keine Fragen beantworten. Triff alle Entscheidungen selbst, dokumentiere sie in DECISIONS.md und arbeite weiter, bis alle Meilensteine unten erledigt sind.

WICHTIG: Du wirst von einem Skript (nachtlauf.ps1) gestartet, das dich bei Nutzungslimits oder Abbruechen immer wieder neu startet. Jeder Lauf beginnt OHNE Erinnerung an den vorherigen. Deshalb:
- Lies zu Beginn jedes Laufs PLAN.md, PROGRESS.md und DECISIONS.md (falls vorhanden) und mach genau dort weiter, wo aufgehoert wurde. Fang nicht von vorne an.
- Aktualisiere PROGRESS.md oft (mindestens nach jedem Teilschritt): Was ist fertig, woran arbeitest du gerade, was ist der naechste konkrete Schritt, welche Probleme gibt es. So, dass ein neuer Lauf ohne Vorwissen sofort weitermachen kann.
- Committe haeufig, damit bei einem Abbruch nichts verloren geht.
- Erst wenn ALLE Meilensteine M1-M7 erledigt und geprueft sind, lege im Projektordner eine leere Datei namens DONE an. Vorher niemals.

## Ziel
Baue ein Audio-Plugin (Arbeitstitel "QUAD ECHO") in C++ mit JUCE (aktuelle stabile Version, CMake-Build). Format: VST3, zusaetzlich Standalone zum Testen. Es besteht aus 4 Delay-Einheiten (A-D). Ihr Signalfluss ist per "Algorithmus" waehlbar, wie bei Ableton Operator. Jede Einheit laesst sich in einem 3D-Raum platzieren.

## Pro Delay-Einheit (Funktionsumfang orientiert an Ableton Echo)
Echo-Sektion:
- Delay-Zeit L/R, jeweils Sync (Notenwerte inkl. triolisch/punktiert) oder ms, Link-Schalter
- Kanalmodus: Stereo / Ping-Pong / Mid-Side
- Offset (Swing) fuer eine Seite
- Feedback 0 bis ca. 110 % mit Soft-Saturation im Feedbackpfad, damit >100 % musikalisch bleibt statt zu explodieren
- Feedback-Invert (Phase Ø)
- Input-Gain mit Schalter "auch auf Dry-Signal anwenden"
- Filter im Feedbackpfad: Hochpass + Tiefpass mit Resonanz, ein/aus
- Uebergangsmodus bei Delay-Zeit-Aenderung: Repitch / Fade / Jump
- Dry/Wet, Output-Gain, Stereo Width

Modulations-Sektion:
- LFO-Wellenformen: Sinus, Dreieck, Saw up, Saw down, Rechteck (kein Noise)
- Rate in Hz oder tempo-synchron, Phase-Offset L/R, x4-Schalter fuer die Delay-Modulation
- Modulationstiefe auf Delay-Zeit und auf Filter-Cutoff
- Envelope-Mix: Huellkurvenfolger statt bzw. zusaetzlich zum LFO

NICHT bauen: Reverb, Noise, Wobble, Gate/Ducking (Character-Tab).

## Routing (Operator-Prinzip)
- 8-11 feste Algorithmen fuer die 4 Einheiten: von voll seriell (A->B->C->D) ueber gemischte Topologien (z. B. A->B und C->D parallel; A speist B und C parallel, beide -> D) bis voll parallel.
- Wie bei Operator gehen die Einheiten der untersten Reihe auf den Ausgang. Im UI wird jeder Algorithmus als kleines Blockdiagramm gezeigt und per Klick gewaehlt.
- Algorithmuswechsel waehrend der Wiedergabe ohne Klicks (kurzes Crossfade).
- Globaler Dry/Wet und Output hinter dem Routing.

## 3D-Platzierung (das Kernfeature)
- Jede Einheit hat eine Position X (links/rechts), Y (vorne/hinten = Entfernung) und Z (Hoehe). Das sind 3 automatisierbare Parameter pro Einheit.
- UI: grosses Pad in Draufsicht, Hoerer-Symbol unten mittig, 4 farbige ziehbare Punkte A-D. Z per Shift-Drag oder Mausrad, dargestellt ueber Punktgroesse/Schatten, zusaetzlich Zahlenfelder.
- Psychoakustische Umsetzung, NICHT ueber Kompression:
  - X: Equal-Power-Panning plus kleine interaurale Laufzeitdifferenz (bis ca. 0,6 ms) und leichter Hoehenverlust auf dem abgewandten Kanal.
  - Y (Entfernung): Pegelabfall ca. -6 dB pro Abstandsverdopplung (mit Mindestabstand); Tiefpass fuer Luftabsorption, je weiter, desto dunkler; zusaetzliche Laufzeit Abstand/343 m/s (abschaltbar, damit Sync-Delays exakt bleiben); Stereobreite nimmt mit der Entfernung ab; optional 2-3 sehr kurze fruehe Reflexionen (kein Hall), deren Anteil mit der Entfernung steigt.
  - Z (Hoehe): dezente spektrale Faerbung (Peak/Notch im Bereich 6-10 kHz). Muss nicht perfekt sein.
  - Alle Positionsparameter glaetten, keine Zipper-Geraeusche.
- Optionaler Modus "Binaural (Kopfhoerer)" nur, wenn alles andere fertig und getestet ist.

## Technische Anforderungen
- C++17 oder neuer, JUCE ueber CMake (FetchContent oder Git-Submodul), AudioProcessorValueTreeState fuer alle Parameter, State speichern/laden (Presets im Host funktionieren).
- Audio-Thread realtime-safe: keine Allokationen, Locks oder Logging in processBlock. Delay-Buffer vorab allozieren (max. 4 s pro Einheit plus Entfernungs-Delay). Denormals verhindern, alle Parameter glaetten.
- Funktioniert bei 44,1-192 kHz und beliebigen Blockgroessen, Mono- und Stereo-Bus, Latenz korrekt melden (sollte 0 sein).
- Tempo-Sync ueber den Host-Playhead.
- Eigenes, aufgeraeumtes UI mit JUCE-Komponenten, dunkles Theme, skalierbar. Oben die Routing-Auswahl und das 3D-Pad, darunter Tabs A-D mit Echo- und Modulationsseite.

## Arbeitsweise
1. Pruefe zuerst Betriebssystem (Windows) und vorhandene Tools (Compiler, CMake, Git). Installiere Fehlendes selbst ueber winget. Installationen sind erlaubt.
2. Arbeite nur in diesem Projektordner. Aendere, verschiebe oder loesche nichts ausserhalb davon. Ausnahme: Installationen ueber den Paketmanager. Das fertige .vst3 legst du in ./dist ab (nicht nach C:\Program Files, dafuer braeuchte es Admin-Rechte). Keine Systemeinstellungen aendern. Aendere nichts an nachtlauf.ps1 und PROMPT.md.
3. Lege zu Beginn PLAN.md mit den Meilensteinen an und fuehre PROGRESS.md laufend (siehe oben).
4. git init (falls noch nicht geschehen), nach jedem Meilenstein und jedem groesseren Teilschritt committen.
5. Bei Unklarheiten nicht stoppen: die sinnvollste Annahme treffen, in DECISIONS.md eintragen, weitermachen. Wenn ein Ansatz nach mehreren Versuchen nicht klappt, einen einfacheren waehlen und das notieren.
6. Teste selbst:
   - Unit-Tests fuer die DSP: Delay-Zeiten stimmen; Feedback unter 100 % klingt ab; keine NaN/Inf; keine Klicks bei Parameter- und Algorithmuswechsel; jeder Algorithmus liefert Signal.
   - Offline-Render: Impuls und Testsignal durch jede Einheit, jeden Algorithmus und mehrere 3D-Positionen schicken, Ergebnisse als WAV nach ./test-renders.
   - pluginval herunterladen und das VST3 mit Strictness 5 oder hoeher validieren, bis es besteht.
7. Hoer nicht auf, bevor alle Meilensteine erledigt sind:
   - M1 Projekt baut, leeres VST3 laedt, pluginval besteht
   - M2 Eine Delay-Einheit mit allen Echo-Funktionen
   - M3 Modulation
   - M4 Vier Einheiten + Routing-Algorithmen
   - M5 3D-Platzierung
   - M6 UI komplett
   - M7 Alle Tests und pluginval gruen, Release-Build. Dazu README.md (Bauen, Installieren, Bedienung, Parameterliste) und SUMMARY.md: was fertig ist, was nicht, bekannte Probleme, was ich morgen zuerst testen soll. Danach DONE anlegen.
