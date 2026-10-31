// Referenz-Harness fuer das DSP-Audit. Ohne Plugin-Host, im Test-Binary.
//   QuadEchoTests --harness baseline-write <ordner> [manifest]   float32-WAVs + manifest.tsv (Baseline)
//   QuadEchoTests --harness baseline-check <ordner>              neu rendern, mit den WAVs in <ordner> vergleichen
//   QuadEchoTests --harness manifest-check <manifest>            neu rendern, nur Pruefsummen vergleichen
//   QuadEchoTests --harness refdump <ordner>                     Einzel-Einheit fuer das Python-Referenzmodell (tests/ref)
//   QuadEchoTests --harness bench [sekunden] [laeufe]            CPU-Zeit pro Block (p50/p99/max), Median ueber Laeufe
// Die Pruef-Befehle enden mit Exit-Code 1, sobald ein Render nicht bitgenau gleich ist.
#pragma once

int harnessMain (int argc, char** argv);
