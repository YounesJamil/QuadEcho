// Gemeinsame Teile von Harness.cpp und Audit.cpp: Baseline-Faelle, Anregungen, Hilfen fuer Dateien und Zeitmessung.
#pragma once
#include "TestUtil.h"

namespace harness
{
using namespace qetest;

constexpr double kRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
constexpr int kBaselineBlock = 512;
constexpr double kBaselineSeconds = 1.5;
inline const char* const kStimNames[] = { "impulse", "sweep", "noise" };
inline const char* const kModeNames[] = { "stereo", "pingpong", "midside" };
inline const char* const kTransNames[] = { "repitch", "fade", "jump" };

struct Case
{
    std::string name;
    EngineSettings s;
    Mutator mut;
    double bpm = 120.0;
};

Sig stimulus (int kind, double sr);
EngineSettings algoBase (int algo, int mode);
std::vector<Case> baselineCases (double sr);
std::string renderName (const Case& c, int stim, double sr);
Sig renderCase (const Case& c, int stim, double sr);
EngineSettings worstCase();
uint64_t threadCycles();
double dbfs (double x);
void writeF32 (const juce::File& f, const Sig& s);
juce::var echoToVar (const EchoSettings& c);

// Audit-Messungen (Audit.cpp): QuadEchoTests --harness audit <name|all> [ausgabeordner]
int auditMain (const juce::String& which, const juce::File& outDir);
} // namespace harness
