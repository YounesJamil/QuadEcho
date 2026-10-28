#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DspTypes.h"

namespace qe
{
// Reihenfolge der Parameter je Einheit (ID = "a_" .. "d_" + Name)
enum P
{
    pOn, pMute, pSolo, pSync, pLink, pTimeL, pTimeR, pDivL, pDivR, pMode, pOffset,
    pFb, pFbInv, pInGain, pInDry, pPosDry, pHpOn, pLpOn, pHp, pLp, pRes, pTrans,
    pMix, pOut, pWidth,
    pLfoOn, pWave, pRate, pLfoSync, pLfoDiv, pPhase, pFPhase, pX4, pDDepth, pFDepth, pEnv,
    pPosX, pPosY, pPosZ, pFreeze, pSyncMode, pSixL, pSixR, pOffsetL, pHpRes, pSyncModeR, pHpBoost, pLpBoost,
    pCount
};

// pMix ("mix") ist seit Zustandsversion 2 der Echo-Pegel der Einheit (vorher Dry/Wet je Einheit).
// pRes ("res") ist seit Version 4 die LP-Resonanz, pHpRes die HP-Resonanz; pOffset ("offset") wirkt auf R.
inline const char* const kParamNames[pCount] = {
    "on", "mute", "solo", "sync", "link", "timeL", "timeR", "divL", "divR", "mode", "offset",
    "fb", "fbInv", "inGain", "inDry", "posDry", "hpOn", "lpOn", "hp", "lp", "res", "trans",
    "mix", "out", "width",
    "lfoOn", "wave", "rate", "lfoSync", "lfoDiv", "phase", "fPhase", "x4", "dDepth", "fDepth", "env",
    "posX", "posY", "posZ", "freeze", "syncMode", "sixL", "sixR", "offsetL", "hpRes", "syncModeR", "hpBoost", "lpBoost" };

inline const char* const kUnitLetters[kNumUnits] = { "a", "b", "c", "d" };

inline juce::String paramId (int unit, int p)
{
    return juce::String (kUnitLetters[unit]) + "_" + kParamNames[p];
}

// Globaler Filter (gf_*), Noise (nz_*) und Ducking (dk_*), im Expert-Bereich
enum G
{
    gfHpOn, gfLpOn, gfHp, gfLp, gfRes, gfInclDry, gfSkipFirst, gfLfoOn, gfWave, gfRate, gfSync, gfDiv, gfDepth, gfPhase,
    nzOn, nzLevel, nzColor, nzCrackle, nzWidth, nzInto, nzHpOn, nzLpOn, nzHp, nzLp, nzRes,
    nzLfoOn, nzWave, nzRate, nzSync, nzDiv, nzDepthF, nzDepthL, nzPhase, nzEnv, nzPosX, nzPosY, nzPosZ,
    dkOn, dkThreshold, dkAmount, dkAttack, dkRelease,
    gfHpRes, nzHpRes,
    gfHpBoost, gfLpBoost, nzHpBoost, nzLpBoost,
    gCount
};

inline const char* const kGlobalIds[gCount] = {
    "gf_hpOn", "gf_lpOn", "gf_hp", "gf_lp", "gf_res", "gf_inclDry", "gf_skipFirst", "gf_lfoOn", "gf_wave", "gf_rate", "gf_sync", "gf_div", "gf_depth", "gf_phase",
    "nz_on", "nz_level", "nz_color", "nz_crackle", "nz_width", "nz_into", "nz_hpOn", "nz_lpOn", "nz_hp", "nz_lp", "nz_res",
    "nz_lfoOn", "nz_wave", "nz_rate", "nz_sync", "nz_div", "nz_depthF", "nz_depthL", "nz_phase", "nz_env", "nz_posX", "nz_posY", "nz_posZ",
    "dk_on", "dk_threshold", "dk_amount", "dk_attack", "dk_release",
    "gf_hpRes", "nz_hpRes",
    "gf_hpBoost", "gf_lpBoost", "nz_hpBoost", "nz_lpBoost" };

// Globale Parameter
inline const char* const kIdAlgo = "algo";
inline const char* const kIdGlobalMix = "gMix";
inline const char* const kIdGlobalOut = "gOut";
inline const char* const kIdDistDelay = "distDelay";
inline const char* const kIdEarly = "early";
inline const char* const kIdBinaural = "binaural";
inline const char* const kIdDoppler = "doppler";
inline const char* const kIdGlobalFb = "gFb";
inline const char* const kIdLimiter = "limiter";
inline const char* const kIdLimitDry = "limitDry";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Liest alle Parameter ohne Allokation (Audio-Thread-tauglich)
class ParamReader
{
public:
    explicit ParamReader (juce::AudioProcessorValueTreeState& apvts);
    void read (EngineSettings& s) const;
    void readUnit (int u, EchoSettings& c, PosSettings& pos) const;

private:
    std::array<std::array<std::atomic<float>*, pCount>, kNumUnits> unit {};
    std::atomic<float>* algo = nullptr;
    std::atomic<float>* gMix = nullptr;
    std::atomic<float>* gOut = nullptr;
    std::atomic<float>* distDelay = nullptr;
    std::atomic<float>* early = nullptr;
    std::atomic<float>* binaural = nullptr;
    std::atomic<float>* doppler = nullptr;
    std::atomic<float>* gFb = nullptr;
    std::atomic<float>* limiter = nullptr;
    std::atomic<float>* limitDry = nullptr;
    std::array<std::atomic<float>*, gCount> glob {};
};
} // namespace qe
