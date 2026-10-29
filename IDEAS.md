# Ideen (noch nicht umgesetzt)

Stand: 2026-10-29. Nur notiert, nichts davon ist gebaut.

**Das ist ein fester Plan, keine lose Ideensammlung.** Umzusetzen sind Abschnitt 1, Abschnitt 2 und die Punkte 3 bis 9 unten, alle unter der Regel "alles automatisierbar". Die offenen Punkte in den Abschnitten sind nur noch Detailfragen.

## Regel für alles Neue: alles automatisierbar
- Jede neue Einstellung (Phase-Modus, Muster, Zufalls-Seed usw.) muss ein echter Host-Parameter sein, den man in Ableton automatisieren kann. Keine Werte, die nur in der UI oder im Plugin-Zustand leben.
- **Stufenwerte müssen in Ableton als Stufen erscheinen.** Das gilt für alles Diskrete: synchrone Delay-Zeit (Notenwert, Triolen, punktiert, 16tel-Anzahl), Wellenform, Sync-Modus, Algorithmus, Muster-Preset und Schalter. Diese Parameter sind Choice- oder Int-Parameter mit fester Stufenzahl (nicht kontinuierliche Floats), damit die Automationskurve in Ableton einrastet und die Anzeige den Namen der Stufe zeigt.
- Prüfen, ob die vorhandenen Parameter das schon erfüllen (z. B. `divL`/`divR`, `sixL`/`sixR`, `wave`, `algorithm`) und was noch nicht (Quelle: `Source/Params.cpp`). Lücken schließen.

## 3. Link-LED an Single Delay koppeln
- Die Link-LED soll leuchten, wenn Single Delay aktiv ist (L und R verknüpft, `link` in `EchoSettings`). Der Nutzer sagt "nicht wie es jetzt ist": Das jetzige Verhalten ist also anders oder umgekehrt.
- Zuerst im Code prüfen, wann die LED jetzt leuchtet, und die Bedingung entsprechend ändern. Bei Unklarheit den Nutzer fragen.

## 4. STABLE-Modus auf jedem Filter
- Bisher gibt es STABLE als Schalter je Einheit (`stableFb`, Commit "Klang d"), der die Filterspitze in der Schleife ausgleicht.
- Gewünscht: STABLE an **jedem** Filter, den es gibt. Dazu gehören je Einheit der HP und der LP sowie der globale Filter (`GlobalFilter`, HP und LP). Prüfen, ob auch andere Filter dazukommen (z. B. in der Noise-Quelle).
- Je Filter ein eigener, automatisierbarer Schalter im bestehenden Stil. Alte Presets: Standard so wählen, dass der Klang gleich bleibt.

## 5. Ducking: Threshold mit Eingangs-Anzeige
- Beim Ducking-Threshold soll der Eingangspegel angezeigt werden (Pegelbalken im oder am Threshold-Regler), damit man die Schwelle gegen das echte Signal einstellt.
- Anzeige aus dem Audio-Thread nur über einen atomaren Wert (kein Speicher, keine Locks im Audio-Thread). Im Karten-Stil bauen, kein Sonder-Look.

## 6. Kleine Kurvenanzeige auch im Expert-Modus
- Die kleine Kurvenanzeige (Vorschau der Kurve, z. B. LFO-Form, Hüllkurve) soll auch im Expert-Modus vorhanden sein, nicht nur im normalen Modus.
- Unklar, welche Anzeige genau gemeint ist und in welchen Expert-Bereichen sie fehlt (Global Filter, Noise, Ducking?). Zuerst im Code und in der UI nachsehen, wo es sie schon gibt und wo nicht; bei Unklarheit den Nutzer fragen.
- Gleicher Karten-Stil wie die vorhandene Anzeige, keine Sonderlösung.

## 7. Envelope auch für den globalen Filter
- Der globale Filter (`GlobalFilterSettings`, `Source/GlobalFilter.h/.cpp`) bekommt eine Hüllkurve als Modulationsquelle, so wie die Einheiten sie über `envMix` schon haben.
- Vorbild ist die Hüllkurve der Einheiten: Aufbau, Regler und Anzeige dort lesen und übernehmen. Eingangssignal der Hüllkurve und Zielgröße (Grenzfrequenz von HP/LP) klären.
- Alle neuen Regler automatisierbar, Standardwert so, dass bestehende Presets gleich klingen. Mit der kleinen Kurvenanzeige aus Punkt 6 kombinieren.

## 8. Bell-EQ per Doppelklick in jedem Filter
- In jedem Filter (HP und LP jeder Einheit, globaler Filter) lässt sich per Doppelklick auf die Filterkurve ein Bell-EQ-Band hinzufügen.
- Vorhanden ist schon `hpBoost`/`lpBoost` (Glocke an der Grenzfrequenz, siehe `EchoSettings`). Prüfen, ob das Vorbild oder nur ein Teil davon ist und wie die Filter-Anzeige aufgebaut ist (Doppelklick-Verhalten, Ziehen von Frequenz und Gain).
- Offene Punkte: Wie viele Bänder pro Filter (eines oder mehrere)? Doppelklick auf eine vorhandene Glocke zum Entfernen? Parameter je Band: Frequenz, Gain, Güte (Q).
- Alle Band-Parameter automatisierbar. Ein nicht hinzugefügtes Band muss neutral sein (Gain 0 oder ausgeschaltet), damit alte Presets gleich klingen. Zusammenspiel mit STABLE (Punkt 4) beachten: Ein Bell in der Feedback-Schleife kann sich aufschaukeln.

## 9. Auto-Pan in der Noise-Quelle
- Die Noise-Quelle (`Source/NoiseSource.h/.cpp`) bekommt einen Auto-Pan mit zwei Reglern: **Frequency** und **Amount**.
- Frequency optional tempo-gesynkt (Stufen-Parameter, siehe Regel "alles automatisierbar"), Amount 0 = aus bzw. neutral.
- Wellenform fest (Sinus) oder wählbar? Das ist noch offen; Vorschlag: zuerst Sinus.
- Beide Regler automatisierbar, im bestehenden Karten-Stil der Noise-Karte.

## Später (ungeplant, aus der früheren ROADMAP)
- **Design:** Bereiche einklappbar machen, Bedienelemente ausblenden, die gerade nichts bewirken.
- **Ducking mit externem Sidechain:** Echos von einer anderen Spur ducken, braucht einen zusätzlichen Eingangsbus.
- **Freeze global:** alle Einheiten mit einem Knopf.
- **Lautstärke langer Ketten:** automatische Normierung pro Stufe (ändert aber das Pedal-Verhalten).
- **Analog-Klang:** bisher einfache tanh-Kennlinien. Anderer Charakter (Röhre, Band) nur nach Beschreibung des Nutzers.
- **Getrennte Resonanz** für HP und LP im Feedback (bisher ein gemeinsamer Regler).
- **Binaural:** Brightness-Korrektur für helle Kopfhörer, später echte HRTF-Messdaten statt Kugel-Kopf-Modell, jeweils nach Hörtest.

## Bekannte Grenzen
- **Presets nicht als Host-Programme:** Als Programme angemeldet, scheitert pluginval ("Plugin state restoration" bei den Schaltern, Zusammenspiel von Programmwechsel und APVTS). Damit Strictness 10 besteht, laufen Presets nur über das Menü im Plugin.
- **Nicht in einem echten Host getestet und nichts gehört:** Alles ist messtechnisch geprüft (pluginval, Tests), das Klangurteil steht aus, besonders bei Binaural.
- **Mono-Bus:** intern Stereo, der Ausgang wird heruntergemischt. **Tempo ohne Host:** 120 BPM, Tail-Länge 10 s.
- Schätzwerte zum Nachjustieren in `Spatializer.cpp`: Entfernungsskala, Reflexionszeiten, Rück-Dämpfung, Kopfschatten.

## 1. Phase-Modus im Routing (Phasenmodulation zwischen den Einheiten)

**Richtung, in die es gehen soll.** Ein Knopf im Routing (Stil wie der bestehende RoutingSelector).

- Im Phase-Modus speist die Quelleinheit die nächste nicht mehr als Audio, sondern moduliert deren Delay-Zeit.
- Lesezeit(t) = Delay-Zeit + Tiefe x Quellsignal(t), pro Sample, mit Interpolation.
- Träger ist der normale Eingang der Zieleinheit, Modulator ist der Ausgang der Vorgängereinheit.
- Intensität = Tiefe x Pegel der Quelle x Frequenz des Trägers (Phasenverschiebung = 2 pi x f x Verschiebung).
- Serial (A>B>C>D) ergibt eine FM-artige Modulationskette, nur D ist am Ausgang hörbar.
- Offene Punkte:
  - Skalierung von `delayDepth` (ms oder %) für Audio-Rate prüfen.
  - Knopf global oder je Verbindung (Vorschlag: zuerst global).
  - Soll der LFO der Zieleinheit weiterlaufen (Vorschlag: ja, Quelle kommt dazu)?
  - Interpolation in der DelayUnit prüfen (evtl. schon durch Doppler vorhanden).
  - Untergrenze der Lesezeit (mindestens 1 Sample).
- Rückkopplung zwischen Einheiten (z. B. D>A) ist ein eigenes, größeres Thema: braucht einen Block Latenz oder Rechnung pro Sample.

## 2. Rhythmus-Modus für die Modulation (Schritt-Muster im LFO)

- Neue Kurvenform im bestehenden LFO: Schritt-Muster (z. B. 8 oder 16 Stufen pro Takt), Delay-Zeit springt oder gleitet von Stufe zu Stufe.
- Tempo-gesynkt (Takte, Viertel, Triolen) oder frei in Hz.
- Erst Presets (Treppe, Zickzack, Synkopen, Zufall mit fester Seed) plus Zufalls-Taste; Zeichnen der Stufen per Maus später.
- Tiefe, Phase und Rate über die vorhandenen Regler.
- Verwandte Idee (verworfen als eigener Weg): Delay-Zeit der Quelleinheit als Takt der Modulation einer anderen. Ist mit der Modulation der Zieleinheit schon abbildbar.

---

# Anweisungen für eine neue Claude-Instanz

Der Nutzer schreibt Deutsch, knapp und umgangssprachlich. Antworte auf Deutsch. Er hat wenig Nutzungskontingent: erst nur lesen und schätzen, nichts bauen, bis er es sagt. Keine unnötigen Dateien lesen, keine Subagenten.

## Auftrag
Alle Punkte oben sind fester Plan. Wenn der Nutzer "Idee 1" sagt: Phase-Modus (Abschnitt 1). Bei "Idee 2": Rhythmus-Modus (Abschnitt 2). Bei den Nummern 3 bis 9: die jeweiligen Abschnitte. Vor dem Bauen die offenen Punkte kurz mit ihm klären (je eine Frage), dann umsetzen.

Bei jedem Punkt gilt die Regel "alles automatisierbar" (siehe oben): neue Parameter als Host-Parameter anlegen, diskrete Werte als Stufen-Parameter.

## Projektregeln
Stehen in [CLAUDE.md](CLAUDE.md) und gelten für jeden Punkt hier.

## Was schon geprüft ist (Stand 2026-10-28)
- Routing: `Source/DspTypes.h:153` (`kAlgorithms`, 10 Algorithmen, `sources` = Bitmaske der speisenden Einheiten, `outputs` = Ausgangseinheiten). Quellen haben immer einen kleineren Index als die gespeiste Einheit, keine Rückkopplung zwischen Einheiten.
- Engine: `Source/QuadEngine.cpp`, Schleife über die Einheiten ab Zeile ~208. Mehrere Quellen werden addiert und durch die Anzahl geteilt (Zeilen ~236-258). Verarbeitung blockweise (`kChunk`).
- Routing-UI: `Source/RoutingSelector.h`.
- Modulation pro Einheit existiert schon in `EchoSettings` (`Source/DspTypes.h:33-66`): `delayDepth`, `filterDepth`, `envMix`, `wave` (Sine, Triangle, Saw Up, Saw Down, Square, Random), `rateHz`, `lfoSync`, `lfoDiv`, `phaseDeg`, `lfoOn`, `filterPhaseDeg`. Parameter in `Source/Params.cpp` (~Zeile 151).
- Noch NICHT geprüft: `Source/DelayUnit.cpp` (Einheit und Bereich von `delayDepth`, vorhandene Interpolation, Glättung). Das zuerst lesen.

## Vorgehen Idee 1
1. `DelayUnit.cpp/.h` lesen, Skalierung von `delayDepth` und die Lesepfad-Interpolation klären.
2. Neuen Bool-Parameter für den Phase-Modus anlegen (Params, State, StateMigration, Standardwert aus = altes Verhalten).
3. Engine: bei Phase-Modus die Quelle nicht in `src` mischen, sondern als Modulationspuffer an die Zieleinheit geben. Die Zieleinheit liest das externe Eingangssignal als Träger.
4. DelayUnit: Modulationspuffer pro Sample auf die Lesestelle addieren, Untergrenze 1 Sample, Tiefe geglättet.
5. Knopf im RoutingSelector im Karten-Stil.
6. Tests (`tests/`) und Python-Referenz (`tests/ref/delayunit_ref.py`) anpassen, Echtzeit-Test (kein Speicher im Audio-Thread), Build, `dist/` aktualisieren.

## Vorgehen Idee 2
1. Neue Kurvenform "Steps" im LFO ergänzen (Wave-Enum, `kNumWaves`, Auswahl in `UiControls.h`, Icon).
2. Muster als Parameter oder Preset-Index plus Seed; tempo-gesynkt über `lfoSync`/`lfoDiv`.
3. Zufalls-Taste und kleine Anzeige im Karten-Stil.
4. State-Migration, Tests, Build, `dist/`.
