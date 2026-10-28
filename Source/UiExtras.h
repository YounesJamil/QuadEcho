#pragma once
#include "UiCommon.h"

namespace ui
{
// ---- Tooltips (Englisch, wie die Beschriftungen) ----
inline juce::String tooltipForUnitParam (int p)
{
    using namespace qe;
    switch (p)
    {
        case pOn:      return "Unit on/off. Off = the signal passes through unchanged (bypass).";
        case pMute:    return "Silences this unit everywhere: at the output and for following units.";
        case pSolo:    return "Hear only the soloed unit(s), at their position.";
        case pSync:    return "Delay times follow the host tempo (note values).";
        case pLink:    return "Off (default): single delay, the right time follows the left (greyed out). On: the right side is free, separate times for left and right.";
        case pTimeL:
        case pTimeR:   return "Delay time in milliseconds (max. 4 s).";
        case pDivL:
        case pDivR:    return "Delay time as a note value of the chosen sync mode (Notes, Triplet, Dotted).";
        case pSyncMode:
        case pSyncModeR: return "Sync mode of this side (like Ableton Echo): Notes, Triplet, Dotted, or 16th (time as a number of 16th notes). Switching keeps the base note (1/8 -> 1/8T -> 1/8.).";
        case pSixL:
        case pSixR:    return "Delay time as a number of 16th notes (1-16).";
        case pMode:    return "Stereo: left and right independent. Ping Pong: the echo bounces left-right. Mid/Side: mid and side are delayed separately.";
        case pOffset:  return "Shifts the right side by a percentage of its time (swing).";
        case pFb:      return "How much of the echo is fed back. Above 100% it grows until the soft saturation limits it.";
        case pFbInv:   return "Inverts the phase of the feedback.";
        case pInGain:  return "Level into the delay. Above 0 dB it adds warm analog-style saturation (more gain = more drive). Affects the echoes only, unless 'Gain on dry' is on.";
        case pInDry:   return "Also applies the input gain (and the drive) to this unit's share of the original.";
        case pPosDry:  return "Also places this unit's share of the original at its position (not only the echoes).";
        case pHpOn:    return "Highpass in the feedback path: the repeats get slimmer (first echo untouched).";
        case pLpOn:    return "Lowpass in the feedback path: the repeats get darker (first echo untouched).";
        case pHp:      return "Highpass cutoff in the feedback path.";
        case pLp:      return "Lowpass cutoff in the feedback path.";
        case pRes:     return "Resonance of both filters. High values can ring.";
        case pTrans:   return "What happens when the delay time changes: Repitch = tape-like pitch glide, Fade = smooth crossfade, Jump = immediate (with a tiny crossfade).";
        case pMix:     return "Echo level of this unit. The signal passing through stays untouched (like a pedal). How much of the original you hear at the output is set by Global Dry/Wet.";
        case pFreeze:  return "Holds the echoes in an endless loop: the input closes, the feedback is exactly 100 %, filters and saturation are bypassed. Time changes wait until freeze is off.";
        case pOut:     return "Output level of this unit (-24 to +12 dB). To switch a unit off use UNIT ON (bypass) or M (mute).";
        case pWidth:   return "Stereo width of the echoes.";
        case pLfoOn:   return "LFO on/off as modulation source. Off: only the envelope (if Envelope mix > 0) modulates.";
        case pWave:    return "LFO shape (no noise).";
        case pRate:    return "LFO rate in Hz.";
        case pLfoSync: return "LFO rate follows the host tempo.";
        case pLfoDiv:  return "LFO rate as a note value.";
        case pPhase:   return "Phase offset of the right-channel LFO (stereo movement).";
        case pFPhase:  return "Phase offset of the filter modulation relative to the delay modulation (90 deg = the filter moves a quarter cycle later).";
        case pX4:      return "Multiplies the delay-time modulation by 4.";
        case pDDepth:  return "How strongly the modulation shifts the delay time (up to 10 ms, x4: 40 ms).";
        case pFDepth:  return "How strongly the modulation moves the filter cutoff (up to +-2 octaves). Needs HP or LP on.";
        case pEnv:     return "Modulation source: 0% = LFO only, 100% = loudness of this unit's input signal (envelope follower). Works only with Delay depth or Filter depth above 0.";
        default:       return {};
    }
}

inline juce::String tooltipForGlobal (const juce::String& id)
{
    if (id == qe::kIdGlobalMix)  return "The only control for the original (equal power): 0% = original only, 50% = equal, 100% = echoes only (e.g. on a send).";
    if (id == qe::kIdGlobalOut)  return "Master output level, behind the routing.";
    if (id == qe::kIdGlobalFb)   return "Scales the feedback of all four units together (100% = as set in the units). Pull down to let the echoes die out.";
    if (id == qe::kIdDoppler)    return "Pitch change when you move a point. Only works with DIST. DELAY on. 0 = none, 100% = realistic, 200% = exaggerated.";
    if (id == qe::kIdBinaural)   return "Headphone mode: head model with front/back and up/down cues. Only for headphones.";
    if (id == qe::kIdDistDelay)  return "Adds travel time (distance / 343 m/s) to placed units. Strengthens distance, but shifts echoes slightly off the tempo grid.";
    if (id == qe::kIdEarly)      return "Three very short echoes (5-13 ms) that grow with distance. Not a reverb.";
    if (id == qe::kIdLimiter)    return "Soft limiter at the very end: keeps peaks below 0 dBFS.";
    return {};
}

inline juce::String tooltipForExpert (const juce::String& id)
{
    using namespace qe;
    auto is = [&id] (int g) { return id == kGlobalIds[g]; };
    if (is (gfHpOn) || is (nzHpOn)) return "Highpass on/off.";
    if (is (gfLpOn) || is (nzLpOn)) return "Lowpass on/off.";
    if (is (gfHp) || is (nzHp)) return "Highpass cutoff.";
    if (is (gfLp) || is (nzLp)) return "Lowpass cutoff.";
    if (is (gfRes) || is (nzRes)) return "Resonance of the filters. High values can ring.";
    if (is (gfInclDry)) return "Also filter the original (dry) signal. Off: only the echoes are filtered (and the noise).";
    if (is (gfSkipFirst)) return "The first echo of each unit stays unfiltered; only the repeats (2nd, 3rd, ...) are filtered. Noise and the dry signal are not affected by this switch.";
    if (is (gfLfoOn) || is (nzLfoOn)) return "LFO on/off as modulation source.";
    if (is (gfWave) || is (nzWave)) return "LFO shape.";
    if (is (gfRate) || is (nzRate)) return "LFO rate in Hz.";
    if (is (gfSync) || is (nzSync)) return "LFO rate follows the host tempo.";
    if (is (gfDiv) || is (nzDiv)) return "LFO rate as a note value.";
    if (is (gfDepth)) return "How strongly the LFO moves the global filter cutoff (up to +-2 octaves).";
    if (is (gfPhase)) return "Offsets the movement of the right filter against the left one (stereo motion). 0 = both sides move together.";
    if (is (nzPhase)) return "Phase offset of the level modulation against the filter modulation.";
    if (is (nzOn)) return "Noise source on/off (tape hiss and crackle).";
    if (is (nzLevel)) return "Noise level. 100% is intense.";
    if (is (nzColor)) return "Noise color: dark and heavy (brown) - pink - bright (white).";
    if (is (nzCrackle)) return "Crackle: short random clicks, like old tape or vinyl.";
    if (is (nzWidth)) return "Stereo width of the noise (0 = mono).";
    if (is (nzInto)) return "0 = noise goes straight to the output. 100% = noise goes into the delay units instead, so it is echoed and saturated in the feedback loop.";
    if (is (nzDepthF)) return "How strongly the modulation moves the noise filter cutoff (up to +-2 octaves).";
    if (is (nzDepthL)) return "How strongly the modulation pulses the noise level.";
    if (is (nzEnv)) return "Modulation source: 0% = LFO, 100% = loudness of the input signal (the noise follows your playing).";
    if (is (nzPosX) || is (nzPosY) || is (nzPosZ)) return "Position of the noise in the room.";
    if (is (dkOn)) return "Ducking on/off: the echoes get quieter while you play and come back in the pauses. The original and the noise are not ducked.";
    if (is (dkThreshold)) return "Input level from which the echoes are lowered (soft knee, +-3 dB).";
    if (is (dkAmount)) return "How far the echoes are lowered while you play.";
    if (is (dkAttack)) return "How fast the echoes dip when you start playing.";
    if (is (dkRelease)) return "How fast the echoes come back after you stop.";
    return {};
}

// Eigene Farbe fuer alles, was zur Modulation gehoert (LFO, Tiefe, Huellkurve)
inline const Colour kModColour { 0xffe6879f };

// ---- Pegel ----
inline float dbToGainUi (float db) { return std::pow (10.0f, db * 0.05f); }

inline float meterPos (float peak)
{
    const float db = 20.0f * std::log10 (std::max (peak, 1e-5f));
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

// Horizontaler Spitzenpegel (liest lockfrei aus der Engine)
class PeakMeter : public juce::Component, private juce::Timer
{
public:
    PeakMeter (std::function<float()> getPeak, Colour accent = kAccent) : getter (std::move (getPeak)), col (accent)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (kBg);
        g.fillRoundedRectangle (b, 2.0f);
        const float pos = meterPos (level);
        auto fill = b.withWidth (b.getWidth() * pos);
        g.setColour (level > 0.97f ? juce::Colour (0xffff4d4d) : (pos > 0.85f ? juce::Colour (0xffffc857) : col));
        g.fillRoundedRectangle (fill, 2.0f);
        g.setColour (kLine);
        for (float db : { -24.0f, -12.0f, -6.0f })   // Skalenmarken
            g.drawVerticalLine ((int) (b.getX() + b.getWidth() * meterPos (dbToGainUi (db))), b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        const float pk = getter();
        const float next = std::max (pk, level * 0.86f);
        if (std::abs (next - level) > 1e-4f) { level = next; repaint(); }
    }
    std::function<float()> getter;
    Colour col;
    float level = 0.0f;
};

// Karte (Gruppe von Reglern) mit Titel: neutraler Grund fuer alle Karten, die Akzentfarbe nur als
// kleine Marke vor dem Titel (die Farbe tragen die Regler selbst)
inline void drawCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, Colour accent)
{
    auto b = r.toFloat();
    g.setColour (kPanel.withAlpha (0.55f));       // Karten fast glatt: deckt die Koernung des Panels teilweise ab
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (kText.withAlpha (0.035f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (kText.withAlpha (0.06f));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    drawBevel (g, b, 8.0f, 0.7f);
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) r.getX() + 10.0f, (float) r.getY() + 7.0f, 3.0f, 9.0f), 1.5f);
    g.setColour (kDim);
    g.setFont (ui::uiFont (10.5f, true));
    g.drawText (title, r.getX() + 18, r.getY() + 4, r.getWidth() - 28, 14, juce::Justification::centredLeft);
}
} // namespace ui
