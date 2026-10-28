# Entscheidungen

- JUCE 9.0.4 (neuester stabiler Tag), per FetchContent nach ./deps.
- Generator: Visual Studio (Ninja nicht installiert).
- Mono-Bus: intern Stereo, Ausgang wird heruntergemischt.
- Alles Grosse auf Q: (C: fast voll, Y: langsam): JUCE-Deps in Q:/QuadEcho/deps, Build in Q:/QuadEcho/build, pluginval in Q:/QuadEcho/tools. Im Projektordner (Y:) bleiben nur Quellcode, Doku, logs, dist, test-renders.
- Konfigurieren: cmake -S . -B Q:/QuadEcho/build -G "Visual Studio 18 2026" -A x64
- 10 Algorithmen nach Operator-Prinzip (Tabelle in Source/DspTypes.h). Mehrere Quellen einer Einheit werden gemittelt.
- Ein Sync-Schalter je Einheit (gilt fuer L und R). Link: R folgt L (Zeit und Division). Offset wirkt als +/-50 % auf die R-Zeit.
- Ping-Pong: Eingang mono auf Linie L, Kreuz-Feedback; erstes Echo links, zweites rechts bei tL+tR. Mid/Side: M mit Zeit L, S mit Zeit R.
- Dry/Wet Equal-Power (cos/sin). Soft-Saturation = tanh im Feedbackpfad. Resonanz-Q = 0.707 + res^2 * 4.
- Uebergaenge: Repitch glaettet die Zeit (Zeitkonstante 200 ms), Fade blendet 60 ms, Jump blendet 3 ms zwischen zwei Abgriffen (kein Klick).
- Modulationstiefe max. 10 ms (x4: 40 ms), Filter +/-2 Oktaven. Envelope-Follower (5/150 ms) in dB-Skala auf 0..1. Tiefen werden geglaettet.
- 3D: Raumplatzierung nur fuer Ausgangseinheiten (ein erster Entwurf mit Platzierung in jeder Kettenstufe summierte sich auf ca. -30 dB).
  Distanz 1 + 7*Y Meter plus lateral 1.5*X; Hoehe Z aendert die Distanz nicht, nur die Faerbung.
- Ausgangssumme: gewichtete Mittelung nur ueber Zweige mit aktiver Einheit; ohne aktive Einheit Dry durchreichen.
- Algorithmuswechsel: Ausgang 15 ms aus, Topologie wechseln, 15 ms ein (statt doppeltem DSP fuer echtes Crossfade).
- Binaural-Modus nicht gebaut (optional).
- Tests: eigene Konsolenanwendung QuadEchoTests (kein Test-Framework), schreibt WAVs nach ./test-renders (nicht im Git).
- Heredocs in Git Bash scheiterten an Sonderzeichen; Dateien mit dem Write-Werkzeug erzeugt.

## Runde 3 (2026-10-28)

- Dry/Wet neu: Jede Einheit gibt das durchlaufende Signal unveraendert weiter und legt die Echos mit dem Echo-Pegel (Parameter `mix`, 0..1 linear) dazu, wie ein Effektpedal. Das Original am Ausgang bestimmt nur noch der globale Dry/Wet (Equal-Power). Grund: Zwei Dry/Wet-Regler hintereinander verwirrten (100 % Global Wet liess trotzdem Original durch), ein Send-Betrieb ging nur mit Nachregeln jeder Einheit.
  Umsetzung: Neben dem Signal laeuft eine Spur des reinen Originals durch die Einheiten (nur deren Dry-Weg: Gain on Dry, Output, Mute, Mittelung). Echo = Ausgang - Original-Spur. So ist die Trennung in jedem Algorithmus exakt (linear) und in Ketten zaehlen fruehere Echos als Echos. Die Saettigung mit Gain on Dry ist nichtlinear; der Rest landet im Echo-Anteil.
  Standard: global 50 %, Echo-Pegel 100 %. Mit einer Einheit klingt das genau wie der alte Standard (beide -3 dB).
  Folge: Lange Ketten mit Echo-Pegel 100 % werden lauter als vorher (wie Pedale in Serie). Bewusst so gelassen, Echo-Pegel senken.
- Zustandsversion `qeVersion` = 2 am gespeicherten Zustand. Aeltere Zustaende werden beim Laden umgerechnet (`Source/StateMigration.h`): Echo-Pegel = sin(m)/max sin(m), globaler Mix aus dem alten Verhaeltnis Original/Echo. Presets sind auf die neue Bedeutung umgestellt.
- Place Dry und Gain on Dry wirken auf den Anteil des Originals, der ueber die jeweilige Ausgangseinheit laeuft. Waehrend eines Algorithmuswechsels kommt das Original unplatziert direkt vom Eingang (keine Luecke im Original).
- Freeze je Einheit: Eingang zu, Feedback genau +-1 (Invert bleibt), Filter und Saettigung umgangen (50 ms Ueberblendung). Die Leseposition rastet per Fade auf eine ganze Sample-Zahl ein, sonst wuerde die Hermite-Interpolation den Loop bei jeder Runde dunkler machen. Modulationstiefe geht dabei auf 0. Zeitaenderungen warten bis Freeze aus.
- Ducking global im Expert-Bereich: Spitzenwert des Eingangs (Release 30 ms), weiches Knie +-3 dB um den Threshold, Absenkung in dB mit Attack/Release geglaettet. Wirkt nur auf die Echos (vor der Raumplatzierung), nicht auf Original und Noise. Kein externer Sidechain (waere ein zusaetzlicher Bus).
- UI: Pad quadratisch (Ringe = echte Kreise, gleicher Massstab wie im DSP). Positionstabelle kompakt mit Leiste ab der Mitte, Werte weiter eintippbar. Master-Karte (Mix, Output, Feedback Scale, Limiter) und Karte Position & Room (Zahlen, Binaural, Early Refl., Dist. Delay, Doppler). Doppler abgedunkelt ohne Dist. Delay. Karten neutral, Akzent nur als Marke. Chips schmaler, An/Aus als gefuellter Punkt/Ring, Pegel zeigt die Echos der Einheit.
- "Global Feedback" heisst jetzt "Feedback Scale" (ID `gFb` unveraendert).
- Entfernungskurve (Nutzerwunsch nach Hoertest auf Boxen): Radius -> Meter abschnittsweise linear: bis Radius 2/3 von 1 auf 1,41 m (hoechstens -3 dB, Luft-Tiefpass erst ab ca. 14 kHz), bis Radius 1 steil auf 4 m (-12 dB), bis zu den Ecken (1,41) auf ca. 5,6 m. Pegel, Tiefpass, Stereobreite, Reflexionen, Laufzeit und Doppler folgen alle dieser Entfernung (Spatializer.cpp, distanceMeters).
- Platzierung bei ausgeschalteter Ausgangseinheit (Nutzerwunsch): Eine Ausgangseinheit, die aus ist, reicht nur durch; ihre Quellen uebernehmen Position und Anteil, bei mehreren Quellen zu gleichen Teilen (placementOf in DspTypes.h, gemeinsam fuer DSP, Pad und Positionsfelder). Stumme Einheiten zaehlen fuer die Mittelung mit, klingen aber nicht. Ausgaenge ohne eingeschaltete Einheit im Zweig bleiben in der UI ziehbar (vorab platzieren).
- Drehregler: Shift = zehnmal feiner (2500 statt 250 Pixel fuer den ganzen Bereich), schrittweise aufsummiert, damit beim Umschalten mitten im Ziehen nichts springt (FineSlider in UiCommon.h).
- Expert-Bereich und Live-Modulation im Stil der uebrigen UI (Nutzerwunsch): warmer Grund und neutrale Karten statt Blau-Verlauf bzw. Neon-Oszilloskop; Blau nur als Kennfarbe des Expert-Bereichs, Kurven in den Funktionsfarben.
- Offset als +-Balken unter den Zeiten (Nutzerwunsch, wie Ableton Echo); Time-Karte schmaler (320), Feedback-Karte breiter. Einheiten-Panel 30 px hoeher (334), damit Sync-Menue, Zeitregler und Offset untereinander passen; Fenster-Grundhoehe 752.
- Sync-Modus je Einheit (Nutzerwunsch, wie Ableton Echo): Notes, Triplet, Dotted, 16th (Standard). Notes/Triplet/Dotted teilen sich divL/divR; gehoert der gespeicherte Notenwert zu einer anderen Familie, spielt der DSP dieselbe Grundnote im aktuellen Modus (divInMode). 16th nutzt eigene Parameter sixL/sixR (1-16). Zustandsversion 3: alte Zustaende bekommen den Modus passend zu ihrem Notenwert (klingen wie vorher).
- Einheitliche Maus-Bedienung (FineSlider): Ziehen relativ (Balken springen beim Klick nicht mehr), Einzelklick oeffnet nach der Doppelklick-Wartezeit ein Eingabefeld, Doppelklick = Standardwert. Die eingebaute JUCE-Texteingabe ist aus, weil sie Klick und Doppelklick je nach Reglerart unterschiedlich verwendet.
- Globaler Filter: die Phase hatte keinen Bezug (ein einzelner LFO). Jetzt "Phase R" wie bei den Einheiten: rechter Kanal mit eigenem Filter und versetzter Bewegung. ID gf_phase unveraendert, bei 0 Grad identisch zu vorher.
- Struktur (Nutzerwunsch "zu clean"): feine Koernung (gekachelte 128er-Textur, SharedResourcePointer, solange ein Editor offen ist), flache Praegung (Lichtsaum oben, dunkle Kante unten), Knopfkoerper mit Verlauf, Vignette und einfacher Schatten ohne Weichzeichner (Meter und Live-Kurven loesen haeufig Neuzeichnen des Hintergrunds aus).
- Single-Delay als Start (Nutzerwunsch): Link bleibt standardmaessig an, der Knopf heisst jetzt "SPLIT L/R" und zeigt das Gegenteil (leuchtet = Link aus, R frei). Kein neuer Parameter.
- Offset je Kanal (wie Ableton Echo): neuer Parameter offsetL, offset bleibt R; im Single-Delay wirken beide (Stereo-Swing). Darstellung als kleines Zahlenfeld unter dem Zeitregler (FineSlider mit "numberbox").
- Getrennte Resonanz fuer HP und LP (Nutzerwunsch, EQ-Kurve mit zwei Punkten): res = LP, hpRes neu (auch gf_/nz_). Zustandsversion 4 kopiert die alte gemeinsame Resonanz in beide.
- Filter als EQ-Kurve (FilterGraph): Betragsgang 2. Ordnung wie im DSP (Q = 0.707 + res^2 * 4), Punkt auf der Resonanzspitze (Hoehe = 20 log10 Q). Klick = an/aus erst nach der Doppelklick-Wartezeit, Doppelklick = Standard.
- Random-LFO (weich): Kosinus-Ueberblendung zwischen reproduzierbaren Zufallswerten je Zyklus (Hash des Zyklus). Dafuer laufen alle LFO-Positionen fortlaufend (double, nicht auf 0..1 umgebrochen); lfoWave bekommt die Position in Zyklen.
- Symbole statt Aufklapp-Menues (IconChoice): LFO-Formen, Sync-Modus, Channel Mode, Transition. Notenwerte (Delay-Zeit in Notes/Triplet/Dotted, LFO-Sync) als Drehregler (NoteDivKnob bzw. Knob auf dem Auswahl-Parameter).
- Echo-Vorschau: x-Achse in Takten des Host-Tempos (1/2/4/8 Takte, so dass die ersten vier Wiederholungen hineinpassen), Raster Takt/Viertel/16tel.
- Live-Modulation: Huellkurven-Spur skaliert mit Envelope mix (zeigt den tatsaechlich genutzten Anteil).
- Noise 12 dB leiser (100 % ca. -18 dBFS Effektivwert), sie war im Vergleich zum Rest zu laut.
- Koernung als Material: an der Ecke jeder Flaeche verankert, warme Toene, grob im Hintergrund, fein in Panels, Karten decken sie teilweise ab; Licht (oben hell, unten dunkel) liegt ueber der Koernung.
- Sync-Modus je Kanal (Nutzerwunsch, wie Ableton Echo): syncMode = L, syncModeR neu; kleine Symbole ueber jedem Zeitregler, die rechten im Single-Delay ausgegraut. Zustandsversion 5: R bekommt den Modus von L.
- Filter 18 dB/Oktave (Nutzerwunsch, vorher 12): SVF 2. Ordnung (Resonanz) plus TPT-Stufe 1. Ordnung bei derselben Frequenz, in Feedback-, globalem und Noise-Filter. Die Kurve rechnet dasselbe; der Punkt sitzt auf dem Pegel bei der Grenzfrequenz (Q in dB minus 3 dB). An/Aus ueber Kaestchen HP/LP statt Klick auf den Punkt.
- Mid/Side: Beschriftung M/S statt L/R in Zeitreglern (und Echo-Vorschau).
- Kanal-Symbole: Ping Pong = Ball zwischen zwei Seiten (gestrichelte Bahn), Mid/Side = Buchstaben M/S (eindeutiger als jedes Bild).
- Offset-Feld: kleine Pille, nur der Wert mit Vorzeichen ("+0%"), bei 0 gedimmt, sonst Einheitsfarbe.
- Hintergrund-Struktur: weichgezeichnetes Rauschen (dreimal 3x3, nahtlos) statt 2x2-Pixel (sah nach Minecraft aus).
- EQ-Kurve: Ziehen aendert nur die Frequenz, mit Alt nur die Guete Q (Nutzerwunsch); angezeigt wird Q statt Prozent.
- SPLIT L/R schaltet beim Klick direkt den Parameter um und zeigt ihn nur an (setClickingTogglesState aus). Vorher schaltete der Knopf sich selbst und leitete den Parameter daraus ab; das brauchte teils zwei Klicks. Pruefbar mit QuadEchoUiShot split=N.
- Werte-Schrift: Bahnschrift SemiBold (bei Windows 10/11 dabei) in warmem Creme fuer alle veraenderbaren Werte (createSliderTextBox), sonst Standardschrift.
- Koernung: feine Koernung in echter Bildschirmaufloesung (Kachel mit 1/Pixelskalierung gezeichnet), sonst wird bei Windows-Skalierung jedes Texturpixel zu einem sichtbaren Block. Hintergrund weicher (256er-Kachel, sechsmal weichgezeichnet, schwaecher).
- 16th-Modus zeigt nur die Anzahl (ganze Zahl), Zeit-Symbole groesser (92 x 24).
- LINK wieder als normaler Knopf (leuchtet = gekoppelt, Standard an). Der Umbau zu "SPLIT L/R" beruhte auf einem Missverstaendnis; der Nutzer meinte die wiederhergestellte alte Sitzung der Standalone-App.
- Filter-Boost (Nutzerwunsch: Punkt ueber die 0-dB-Linie ziehen): Glocke (RBJ-Peaking-EQ) an der modulierten Grenzfrequenz, gleiche Guete wie die Resonanz, 0 bis +18 dB, je HP und LP, in allen drei Filtern. Eigene Biquad-Struktur (PeakEq in DspTypes.h), weil JUCEs IIR-Koeffizienten im Audio-Thread allokieren. Ziehen hoch = Boost, Alt = Q.
- Resonanz und Boost werden geglaettet (ca. 30 ms, pro 16er-Block), sonst springt eine hohe Guete hoerbar. Der Resonanzbereich bleibt Q 0,71-4,7 (eine kurz eingebaute Erweiterung bis Q 16 war nicht gemeint und ist wieder raus).
- Schrift nirgends weiss (Nutzerwunsch): Texte in warmem Sand (kText 0xffd9c6a1), Werte etwas heller (kValue). Offset-Werte als einzige Schrift in pastelligem Tuerkisblau (kOffsetColour), bei 0 gedimmt.
- Eine Schrift fuer die ganze Oberflaeche (Nutzerwunsch "Standardschrift anpassen"): Bahnschrift, normal fuer Beschriftungen, fett fuer Knoepfe und Titel, SemiBold fuer Werte (ui::uiFont / ui::valueFont). Ausdruecklich gesetzt, weil JUCE die Standardschrift ueber den globalen LookAndFeel sucht.
- LINK-Knopf nach Nutzerwunsch: aus (Standard) = Single-Delay, R ausgegraut; an (leuchtet) = R frei. Der Parameter "link" behaelt seine Bedeutung (1 = R folgt L), der Knopf zeigt das Gegenteil an (LinkToggle).
- Fehlerursache fuer "invertiert" und "doppelt klicken": JUCE benachrichtigt die Beobachter eines Parameters in umgekehrter Reihenfolge; die Sichtbarkeits-Logik las die APVTS-Rohwerte bzw. Knopfzustaende, die zu diesem Zeitpunkt noch alt waren. Ausgrauen und Anzeigen hinkten so einen Klick hinterher. Jetzt liest die UI dort den Parameter selbst (ui::currentValue). QuadEchoUiShot link=N prueft nach jedem Klick Knopf und rechten Regler.
- Mac-Version (2026-10-28): JUCE baut AU nur auf dem Mac (FORMATS VST3 AU Standalone), macOS ab 11, Universal Binary per CMAKE_OSX_ARCHITECTURES. Gebaut wird per GitHub Actions (Repo github.com/YounesJamil/QuadEcho), ad hoc signiert; ohne Apple-Developer-Konto muss der Empfaenger einmal die Quarantaene entfernen (INSTALL.md). JUCE liegt lokal weiter auf Q:, sonst im Build-Ordner.
- Schrift: Bahnschrift gibt es nur unter Windows und darf nicht mitgeliefert werden. Stattdessen eingebaut: Barlow Semi Condensed (SIL OFL, Resources/Fonts, Lizenz liegt bei), um Faktor 1,18 hoeher gesetzt, weil die Buchstaben bei gleicher JUCE-Hoehe kleiner sind.
- Place Dry nur per Pegel (Nutzerbeobachtung: Original wird mit Place Dry deutlich leiser, bei 3 parallelen Einheiten wirkte es auf das ganze Original). Ursache: Der platzierte Anteil lief durch Entfernung, Laufzeit, Luftfilter und Reflexionen, der Rest direkt; beide zusammen ergeben einen Kammfilter (gemessen -2,1 dB mit Dist. Delay, hoerbar duenner). Jetzt: Equal-Power-Panning nach X der Einheit, Breite schrumpft zur Seite hin, kein Weg durch den Spatializer. Vorne/hinten und Hoehe wirken nur noch auf die Echos.
