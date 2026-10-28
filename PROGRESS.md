# Progress

Stand: ALLE Meilensteine M1-M7 erledigt (siehe SUMMARY.md). Nichts mehr offen ausser dem optionalen Binaural-Modus.

Befehle (alles Grosse liegt auf Q:):
- Configure: cmake -S . -B Q:/QuadEcho/build -G "Visual Studio 18 2026" -A x64
- Build:     cmake --build Q:/QuadEcho/build --config Release --parallel --target QuadEcho_VST3 QuadEcho_Standalone > logs/build.log
- Tests:     cmake --build ... --target QuadEchoTests ; Q:/QuadEcho/build/QuadEchoTests_artefacts/Release/QuadEchoTests.exe test-renders
- pluginval: Q:/QuadEcho/tools/pluginval.exe --strictness-level 10 --validate-in-process --validate "dist/QUAD ECHO.vst3"
- Git Bash: MSBuild-Schalter mit / vermeiden (wird als Pfad gedeutet), --parallel nutzen.
