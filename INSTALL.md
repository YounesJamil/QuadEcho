# QUAD ECHO installieren

QUAD ECHO von Younes Jamil. Delay-Plugin mit vier Einheiten, Routing und 3D-Platzierung.

## Windows

1. Den Ordner `QUAD ECHO.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren (Admin-Rechte nötig) oder nach `%LOCALAPPDATA%\Programs\Common\VST3\` (nur für dich, ohne Admin).
2. In der DAW nach neuen Plugins suchen lassen.
3. Ohne DAW: `QUAD ECHO.exe` starten (Standalone). Falls Windows „Der Computer wurde durch Windows geschützt" meldet: **Weitere Informationen** → **Trotzdem ausführen**. Das kommt, weil die Datei nicht signiert ist.

## macOS (Apple Silicon und Intel, ab macOS 11)

1. Kopieren:
   - `QUAD ECHO.vst3` nach `~/Library/Audio/Plug-Ins/VST3/` (Ableton, Reaper, Cubase, Bitwig, Studio One)
   - `QUAD ECHO.component` nach `~/Library/Audio/Plug-Ins/Components/` (Logic Pro, GarageBand)
   - `QUAD ECHO.app` nach `/Applications/` (Standalone)

   Im Finder: **Gehe zu** → **Gehe zum Ordner …** und den Pfad einfügen. Fehlt ein Ordner, einfach anlegen.

2. **Einmal die Sperre aufheben.** Das Plugin ist nicht bei Apple signiert. Darum blockiert macOS heruntergeladene Dateien („beschädigt", „Entwickler nicht verifiziert", oder die DAW lädt es nicht). Kaputt ist es nicht. Im **Terminal** einmal ausführen:

   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/QUAD\ ECHO.vst3 ~/Library/Audio/Plug-Ins/Components/QUAD\ ECHO.component /Applications/QUAD\ ECHO.app
   ```

3. DAW neu starten bzw. nach Plugins suchen lassen. Taucht das AU in Logic nicht auf: Mac neu starten oder im Terminal `killall -9 AudioComponentRegistrar` und Logic neu öffnen.

## Lizenzen und Danksagung

Die Oberfläche nutzt die eingebaute Schrift *Barlow Semi Condensed* (SIL Open Font License, siehe `OFL-Barlow.txt`).

Der Binaural-Modus nutzt HRTF-Messungen von Bill Gardner und Keith Martin, MIT Media Lab 1994 (KEMAR-Kunstkopf, http://sound.media.mit.edu/resources/KEMAR.html).
