# QUAD ECHO (JUCE-VST3/AU-Plugin, Delay mit vier Einheiten)

Der Nutzer schreibt Deutsch, knapp und umgangssprachlich: auf Deutsch antworten. Sein Nutzungskontingent ist knapp: erst lesen und schätzen, nichts bauen, bis er es sagt; nur die Dateien lesen, die der Punkt braucht; keine Subagenten für Kleinkram.

## Dokumente
- [IDEAS.md](IDEAS.md): der aktuelle Plan (Punkte 1 bis 9, Regel "alles automatisierbar", Später, Bekannte Grenzen) mit Vorgehen je Punkt.
- [SPEC.md](SPEC.md): freigegebene Grenzwerte für CPU, Genauigkeit und Determinismus. Jede DSP-Änderung muss sie einhalten.
- [DECISIONS.md](DECISIONS.md): Designentscheidungen, nicht ohne Grund umwerfen.
- [README.md](README.md): Bedienung, Parameter, Bauen, Tests. [AUDIT.md](AUDIT.md): Messbericht des DSP-Audits. [INSTALL.md](INSTALL.md): Installation für Endnutzer.

## Regeln
- **Ein UI-Stil:** keine Sonder-Looks. Neue Knöpfe und Regler im bestehenden Karten-Stil bauen (`Source/UiControls.h`, `Source/UnitPanel.h`, `Source/RoutingSelector.h`).
- **Alles automatisierbar:** neue Einstellungen als Host-Parameter, diskrete Werte als Stufen-Parameter (Ableton zeigt sie als Stufen).
- **Alte Presets müssen gleich klingen:** Parameteränderungen in `Source/StateMigration.h` berücksichtigen, Standardwerte neutral wählen.
- **Audio-Thread:** kein Speicher anfordern, keine Locks, keine Datei- oder Konsolenzugriffe (ein Test prüft das mit zählendem `operator new`).
- **Nach jedem Build `dist/` aktualisieren:** VST3 und Standalone-EXE, Zeitstempel prüfen.
- **Großes liegt auf Q:** JUCE, Build und Tools in `Q:\QuadEcho`. Im Projektordner nur Quellcode, Doku, `dist/`, Tests.
- **Git:** Repo github.com/YounesJamil/QuadEcho (privat), Commits als YounesJamil <younes.lammaghi@gmail.com>. Nur committen, wenn der Nutzer es verlangt.
