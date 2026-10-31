#pragma once
#include "UiControls.h"
#include "Widgets.h"

// Live-Anzeigen des Ducking (lockfrei aus der Engine): Eingangspegel mit dem Threshold als Marke, darunter die Absenkung
class DuckMeter : public juce::Component, private juce::Timer
{
public:
    explicit DuckMeter (QuadEchoProcessor& p)
        : proc (p), threshold (p.apvts.getRawParameterValue (qe::kGlobalIds[qe::dkThreshold]))
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float inDb = 20.0f * std::log10 (std::max (inShown, 1.0e-6f));
        // INPUT: -60 .. 0 dB (Bereich des Thresholds), Marke = Threshold
        row (g, b.removeFromTop (b.getHeight() * 0.5f), "INPUT", inDb > -59.9f ? juce::String (inDb, 1) + " dB" : juce::String ("-inf dB"),
             (inDb + 60.0f) / 60.0f, inDb > threshold->load() ? ui::kText : ui::kDim, threshold->load(), -60.0f);
        // ECHO REDUCTION: 0 .. -24 dB
        row (g, b, "ECHO REDUCTION", (redShown > 0.05f ? "-" : "") + juce::String (redShown, 1) + " dB", redShown / 24.0f,
             redShown > 0.05f ? ui::kText : ui::kDim, std::nanf (""), 0.0f);
    }

private:
    // Kopfzeile, Balken (Anteil frac), Skala in 6-dB-Schritten ab lo (lo < 0: -60 .. 0, sonst 0 .. -24); mark = Marke in dB
    void row (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& value, float frac,
              juce::Colour valueCol, float mark, float lo)
    {
        g.setFont (ui::uiFont (10.5f, true));
        g.setColour (ui::kDim);
        auto head = r.removeFromTop (15.0f);
        g.drawText (title, head, juce::Justification::centredLeft);
        g.setColour (valueCol);
        g.drawText (value, head, juce::Justification::centredRight);

        auto bar = r.removeFromTop (10.0f);
        g.setColour (ui::kBg);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (ui::kAccent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, frac)), 3.0f);
        g.setColour (ui::kLine);
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);
        const float range = lo < 0.0f ? -lo : 24.0f;
        if (! std::isnan (mark))
        {
            const float x = bar.getX() + bar.getWidth() * (mark - lo) / range;
            g.setColour (ui::kText);
            g.fillRect (juce::Rectangle<float> (x - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f));
        }

        g.setFont (ui::uiFont (10.0f));
        g.setColour (ui::kDim);
        const int step = lo < 0.0f ? 12 : 6;
        for (int k = 0; k <= (int) range; k += step)
        {
            const float x = bar.getX() + bar.getWidth() * (float) k / range;
            g.drawVerticalLine ((int) x, bar.getBottom(), bar.getBottom() + 3.0f);
            const int db = lo < 0.0f ? (int) lo + k : -k;
            const bool first = k == 0, last = k == (int) range;   // Randbeschriftung nach innen ausrichten
            const float lx = first ? x : (last ? x - 30.0f : x - 15.0f);
            g.drawText (juce::String (db), juce::Rectangle<float> (lx, bar.getBottom() + 3.0f, 30.0f, 11.0f),
                        first ? juce::Justification::centredLeft : (last ? juce::Justification::centredRight : juce::Justification::centred));
        }
    }

    void timerCallback() override
    {
        auto follow = [] (float now, float shown) { return now > shown ? now : shown + (now - shown) * 0.3f; };   // steigt sofort, faellt weich
        const float red = follow (proc.getEngine().takeDuckReduction(), redShown);
        const float in = follow (proc.getEngine().takeInputPeak(), inShown);
        if (std::abs (red - redShown) > 0.01f || std::abs (in - inShown) > 1.0e-5f || threshold->load() != lastThr)
        {
            redShown = red; inShown = in; lastThr = threshold->load();
            repaint();
        }
    }
    QuadEchoProcessor& proc;
    std::atomic<float>* threshold;
    float redShown = 0.0f, inShown = 0.0f, lastThr = 0.0f;
};

// Live-Modulation im Expert-Bereich (Global Filter oder Noise): kompakt, gleicher Stil wie die Anzeige der Einheiten
class ExpertModDisplay : public juce::Component, private juce::Timer
{
public:
    ExpertModDisplay (QuadEchoProcessor& p, bool noiseSide) : proc (p), reader (p.apvts), noise (noiseSide)
    {
        setInterceptsMouseClicks (false, false);
        envHist.fill (0.0f);
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override
    {
        qe::EngineSettings s;
        reader.read (s);
        const auto& eng = proc.getEngine();
        const bool lfoOn = noise ? s.noise.lfoOn : s.gfilter.lfoOn;
        const int wave = noise ? s.noise.wave : s.gfilter.wave;
        const float envMix = noise ? s.noise.envMix : s.gfilter.envMix;
        const double pos = noise ? eng.noiseSource().getLfoPos() : eng.globalFilter().getLfoPos();
        const float rate = noise ? eng.noiseSource().getLfoRateHz() : eng.globalFilter().getLfoRateHz();
        const bool filterOn = noise ? (s.noise.hpOn || s.noise.lpOn) : (s.gfilter.hpOn || s.gfilter.lpOn);
        const float fDepth = noise ? s.noise.depthF : s.gfilter.depth;
        constexpr float window = 4.0f;

        auto full = getLocalBounds().toFloat();
        g.setColour (ui::kBg.withAlpha (0.6f));
        g.fillRoundedRectangle (full, 8.0f);
        g.setColour (ui::kText.withAlpha (0.09f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (ui::kModColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 6.0f, 3.0f, 9.0f), 1.5f);
        g.setFont (ui::uiFont (10.5f, true));
        g.setColour (ui::kDim);
        g.drawText ("LIVE MODULATION", 18, 3, 200, 14, juce::Justification::centredLeft);
        g.setFont (ui::uiFont (10.5f));
        g.drawText ("last 4 s, right = now", 130, 3, 200, 14, juce::Justification::centredLeft);

        auto lfoAt = [&] (float tau, double off) { return lfoOn ? qe::lfoWave (wave, pos + off - (double) (rate * tau)) : 0.0f; };
        auto envAt = [&] (float tau)
        {
            const int back = juce::jlimit (0, kHist - 1, (int) std::round (tau * 60.0f));
            return envHist[(size_t) ((head - 1 - back + 2 * kHist) % kHist)];
        };
        auto mixAt = [&] (float tau) { return lfoAt (tau, 0.0) * (1.0f - envMix) + envAt (tau) * envMix; };
        const double panPos = eng.noiseSource().getPanPos();
        const float panRate = eng.noiseSource().getPanRateHz();
        auto panAt = [&] (float tau) { return s.noise.panAmt * (float) std::sin (6.283185307179586 * (panPos - (double) (panRate * tau))); };
        auto levelAt = [&] (float tau)   // Pegel-Faktor wie in NoiseSource (unipolar, Phase gegen den Filter)
        {
            const float u = (lfoOn ? (lfoAt (tau, s.noise.phaseDeg / 360.0) + 1.0f) * 0.5f : 1.0f) * (1.0f - envMix) + envAt (tau) * envMix;
            return 1.0f - s.noise.depthL * (1.0f - u);
        };
        auto signedText = [] (float v, int dec, const char* unitText)
        {
            if (std::abs (v) < 0.005f) v = 0.0f;   // kein "-0.00"
            return (v >= 0.0f ? "+" : "") + juce::String (v, dec) + unitText;
        };

        struct Lane { const char* name; juce::Colour col; bool bipolar; bool active; juce::String now; std::function<float (float)> value; };
        const juce::Colour filterC (0xffbb9cd6);
        std::vector<Lane> lanes;
        lanes.push_back ({ "LFO", ui::kModColour, true, lfoOn && envMix < 0.999f, juce::String (lfoAt (0, 0.0), 2), [&] (float t) { return lfoAt (t, 0.0); } });
        lanes.push_back ({ "ENVELOPE", ui::kDryColour, false, envMix > 0.001f, juce::String ((int) std::round (envAt (0) * envMix * 100.0f)) + " %",
                           [&] (float t) { return envAt (t) * envMix; } });
        lanes.push_back ({ "FILTER", filterC, true, fDepth > 0.001f && filterOn, signedText (mixAt (0) * fDepth * 2.0f, 2, " oct"),
                           [&] (float t) { return mixAt (t) * fDepth; } });
        if (noise)
        {
            lanes.push_back ({ "LEVEL", ui::kNoiseColour, false, s.noise.depthL > 0.001f, juce::String ((int) std::round (levelAt (0) * 100.0f)) + " %", levelAt });
            lanes.push_back ({ "AUTO-PAN", ui::kNoiseColour, true, s.noise.panAmt > 0.001f, signedText (panAt (0), 2, ""), panAt });
        }

        // Noise: zwei Spalten (3 + 2 Lanes), damit die Lanes so hoch bleiben wie beim Global Filter
        auto area = getLocalBounds().reduced (10, 0).withTrimmedTop (18).withTrimmedBottom (4);
        const int cols = noise ? 2 : 1, rows = ((int) lanes.size() + cols - 1) / cols;
        const int colW = (area.getWidth() - (cols - 1) * 16) / cols, laneH = area.getHeight() / rows;
        for (size_t k = 0; k < lanes.size(); ++k)
        {
            const auto& lane = lanes[k];
            const int c = (int) k / rows, row = (int) k % rows;
            auto r0 = juce::Rectangle<int> (area.getX() + c * (colW + 16), area.getY() + row * laneH, colW, laneH).reduced (0, 1);
            const float alpha = lane.active ? 1.0f : 0.3f;
            g.setFont (ui::uiFont (10.5f, true));
            g.setColour (alpha < 1.0f ? ui::kDim.withAlpha (0.5f) : ui::kDim);
            g.drawText (lane.name, r0.removeFromLeft (84), juce::Justification::centredLeft);
            g.setColour (lane.col.withAlpha (alpha));
            g.drawText (lane.now, r0.removeFromRight (84), juce::Justification::centredRight);
            auto r = r0.toFloat().reduced (4.0f, 0.0f);
            g.setColour (ui::kPanelHi.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 2.0f);
            if (lane.bipolar) { g.setColour (ui::kLine); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight()); }
            juce::Path path;
            const int w = (int) r.getWidth();
            for (int px = 0; px <= w; px += 2)
            {
                const float v = juce::jlimit (-1.0f, 1.0f, lane.value (window * (1.0f - (float) px / (float) w)));
                const float y = lane.bipolar ? r.getCentreY() - v * (r.getHeight() * 0.5f - 1.0f)
                                             : r.getBottom() - 1.0f - std::max (v, 0.0f) * (r.getHeight() - 2.0f);
                if (px == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + (float) px, y);
            }
            g.setColour (lane.col.withAlpha (alpha));
            g.strokePath (path, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

private:
    void timerCallback() override
    {
        envHist[(size_t) head] = noise ? proc.getEngine().noiseSource().getEnvValue() : proc.getEngine().globalFilter().getEnvValue();
        head = (head + 1) % kHist;
        if (isShowing()) repaint();
    }

    static constexpr int kHist = 300;   // 5 s bei 60 Hz
    QuadEchoProcessor& proc;
    qe::ParamReader reader;
    bool noise;
    std::array<float, kHist> envHist;
    int head = 0;
};

// Expert-Bereich (klappt unten aus): gleicher Stil wie das Einheiten-Panel, Blau als Kennfarbe.
// Seiten: Globaler Filter, Noise und Ducking. Kompakt gehalten (kleine Regler, ein bis zwei Reihen).
// Filter als EQ-Kurven, LFO-Formen als Symbole.
class ExpertPanel : public juce::Component
{
public:
    static constexpr int kLiveH = 84;                      // Live-Modulation (Global Filter, Noise)
    static constexpr int kHeight = 264 + kLiveH + 6;       // Noise-Seite (hoechste)
    static constexpr int kGfHeight = 180 + kLiveH + 6;     // Filter-Seite
    static constexpr int kFilterHeight = 180;              // Ducking-Seite
    static constexpr int kPages = 4;
    std::function<void()> onHeightChanged;
    int preferredHeight() const { return page == 1 ? kHeight : (page == 0 ? kGfHeight : kFilterHeight); }

    explicit ExpertPanel (QuadEchoProcessor& p)
        : proc (p), duckMeter (p), gfLive (p, false), nzLive (p, true),
          gfGraph (p.apvts, { qe::kGlobalIds[qe::gfHpOn], qe::kGlobalIds[qe::gfLpOn], qe::kGlobalIds[qe::gfHp], qe::kGlobalIds[qe::gfLp],
                              qe::kGlobalIds[qe::gfHpRes], qe::kGlobalIds[qe::gfRes], qe::kGlobalIds[qe::gfHpBoost], qe::kGlobalIds[qe::gfLpBoost] , qe::kGlobalIds[qe::gfBellOn], qe::kGlobalIds[qe::gfBellHz], qe::kGlobalIds[qe::gfBellGain], qe::kGlobalIds[qe::gfBellQ]},
                   juce::Colour (0xffbb9cd6)),
          nzGraph (p.apvts, { qe::kGlobalIds[qe::nzHpOn], qe::kGlobalIds[qe::nzLpOn], qe::kGlobalIds[qe::nzHp], qe::kGlobalIds[qe::nzLp],
                              qe::kGlobalIds[qe::nzHpRes], qe::kGlobalIds[qe::nzRes], qe::kGlobalIds[qe::nzHpBoost], qe::kGlobalIds[qe::nzLpBoost] , qe::kGlobalIds[qe::nzBellOn], qe::kGlobalIds[qe::nzBellHz], qe::kGlobalIds[qe::nzBellGain], qe::kGlobalIds[qe::nzBellQ]},
                   juce::Colour (0xffbb9cd6))
    {
        using namespace qe;
        auto& apvts = proc.apvts;
        const juce::Colour tFilter (0xffbb9cd6);

        auto K = [&] (int g, const juce::String& title, std::function<juce::String (double)> fmt,
                      std::function<double (const juce::String&)> parse, juce::Colour col)
        {
            knobs[(size_t) g] = std::make_unique<ui::Knob> (apvts, kGlobalIds[g], title, col, fmt, parse);
            knobs[(size_t) g]->slider.setTooltip (ui::tooltipForExpert (kGlobalIds[g]));
            knobs[(size_t) g]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
            addChildComponent (*knobs[(size_t) g]);
        };
        auto T = [&] (int g, const juce::String& text, juce::Colour col, bool dry = false, bool noiseIcon = false)
        {
            toggles[(size_t) g] = std::make_unique<ui::Toggle> (apvts, kGlobalIds[g], text, col, dry, noiseIcon);
            toggles[(size_t) g]->setTooltip (ui::tooltipForExpert (kGlobalIds[g]));
            addChildComponent (*toggles[(size_t) g]);
        };
        auto W = [&] (int g)
        {
            waves[(size_t) g] = ui::makeWaveChoice (apvts, kGlobalIds[g], ui::kModColour);
            addChildComponent (*waves[(size_t) g]);
        };
        const juce::String phi = juce::String::fromUTF8 ("\xcf\x86");
        const auto mod = ui::kModColour;

        // Seite 3: Stereo und analoger Chorus
        {
            const auto sc = ui::kAccent;
            K (stWidth, "Width", ui::fmtPct, ui::parsePct, sc);
            K (stEnhance, "Enhance", ui::fmtPct, ui::parsePct, sc);
            T (stInclDry, "INCLUDE DRY", sc, true);
            T (chOn, "CHORUS ON", mod);
            K (chMix, "Mix", ui::fmtPct, ui::parsePct, mod);
            auto TC = [&] (int g, juce::StringArray labels, juce::StringArray tips)
            {
                std::vector<ui::IconChoice::Icon> list;
                for (const auto& t : labels)
                    list.push_back ([t] (juce::Graphics& gr, juce::Rectangle<float> r, juce::Colour c)
                                    { gr.setColour (c); gr.setFont (ui::uiFont (11.0f, true)); gr.drawText (t, r, juce::Justification::centred); });
                waves[(size_t) g] = std::make_unique<ui::IconChoice> (apvts, kGlobalIds[g], std::move (list), tips, mod);
                addChildComponent (*waves[(size_t) g]);
            };
            TC (chMode, { "I", "II", "I + II" }, { "Mode I: slow and wide", "Mode II: a bit faster, more movement", "Mode I + II: fast, shallow shimmer" });
            TC (chPlace, { "ON ECHOES", "IN LOOP" }, { "On the sum of the echoes (once, with a touch of bucket-brigade hiss)",
                                                       "Inside the feedback loop of every unit: each repeat is chorused again" });
        }

        // Seite 0: globaler Filter
        addChildComponent (gfGraph);
        T (gfInclDry, "INCLUDE DRY", tFilter, true);
        T (gfSkipFirst, "SKIP 1ST ECHO", tFilter);
        T (gfLfoOn, "LFO ON", mod);
        W (gfWave);
        T (gfSync, "SYNC", mod);
        K (gfRate, "Rate", ui::fmtHz, ui::parseNum, mod);
        K (gfDiv, "Rate (sync)", ui::fmtDiv, ui::parseDiv, mod);
        K (gfDepth, "Depth", ui::fmtPct, ui::parsePct, mod);
        K (gfPhase, phi + " Phase", ui::fmtDeg, ui::parseNum, mod);
        K (gfEnv, "Envelope", ui::fmtPct, ui::parsePct, mod);

        // Seite 1: Noise
        const auto nc = ui::kNoiseColour;
        T (nzOn, "NOISE ON", nc, false, true);
        K (nzLevel, "Level", ui::fmtPct, ui::parsePct, nc);
        K (nzColor, "Color", [] (double v) { return v < 0.17 ? juce::String ("brown") : v < 0.4 ? juce::String ("dark pink") : v < 0.6 ? juce::String ("pink") : v < 0.85 ? juce::String ("light") : juce::String ("white"); },
          [] (const juce::String& t) { return t.getDoubleValue(); }, nc);
        K (nzCrackle, "Crackle", ui::fmtPct, ui::parsePct, nc);
        K (nzWidth, "Width", ui::fmtPct, ui::parsePct, nc);
        K (nzInto, "Into echoes", ui::fmtPct, ui::parsePct, nc);
        addChildComponent (nzGraph);
        T (nzLfoOn, "LFO ON", mod);
        W (nzWave);
        T (nzSync, "SYNC", mod);
        K (nzRate, "Rate", ui::fmtHz, ui::parseNum, mod);
        K (nzDiv, "Rate (sync)", ui::fmtDiv, ui::parseDiv, mod);
        K (nzDepthF, "Filter depth", ui::fmtPct, ui::parsePct, mod);
        K (nzDepthL, "Level depth", ui::fmtPct, ui::parsePct, mod);
        K (nzPhase, phi + " Level", ui::fmtDeg, ui::parseNum, mod);
        K (nzEnv, "Envelope", ui::fmtPct, ui::parsePct, mod);
        K (nzPanAmt, "Amount", ui::fmtPct, ui::parsePct, ui::kNoiseColour);
        K (nzPanRate, "Frequency", ui::fmtHz, ui::parseNum, ui::kNoiseColour);
        K (nzPanDiv, "Freq (sync)", ui::fmtDiv, ui::parseDiv, ui::kNoiseColour);
        T (nzPanSync, "SYNC", ui::kNoiseColour);

        // Seite 2: Ducking
        const auto dc = ui::kAccent;
        T (dkOn, "DUCKING ON", dc);
        K (dkThreshold, "Threshold", ui::fmtDb, ui::parseNum, dc);
        K (dkAmount, "Amount", [] (double v) { return "-" + juce::String (v, 1) + " dB"; },
          [] (const juce::String& t) { return std::abs (t.getDoubleValue()); }, dc);
        K (dkAttack, "Attack", ui::fmtMs, ui::parseNum, dc);
        K (dkRelease, "Release", ui::fmtMs, ui::parseNum, dc);
        addChildComponent (duckMeter);
        addChildComponent (gfLive);
        addChildComponent (nzLive);

        static const char* pageNames[kPages] = { "GLOBAL FILTER", "NOISE", "DUCKING", "STEREO" };
        for (int i = 0; i < kPages; ++i)
        {
            auto& b = tabs[(size_t) i];
            b.setButtonText (pageNames[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (303);
            b.setColour (juce::TextButton::buttonOnColourId, kExpertColour);
            b.onClick = [this, i] { setPage (i); };
            addAndMakeVisible (b);
        }
        page = juce::jlimit (0, kPages - 1, (int) apvts.state.getProperty ("uiExpertPage", 0));
        tabs[(size_t) page].setToggleState (true, juce::dontSendNotification);

        for (int g : { gfSync, gfLfoOn, nzSync, nzLfoOn, dkOn, nzPanSync, chOn, gfHpOn, gfLpOn, gfBellOn, nzOn, stWidth, stEnhance })
        {
            watchers.push_back (std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (kGlobalIds[g]), [this] (float) { updateVisibility(); }));
            watchers.back()->sendInitialUpdate();
        }
        updateVisibility();
    }

    void setPage (int p)
    {
        page = p;
        proc.apvts.state.setProperty ("uiExpertPage", p, nullptr);
        updateVisibility(); resized(); repaint();
        if (onHeightChanged) onHeightChanged();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // gleicher Grund wie das Einheiten-Panel; Blau (wie der EXPERT-Knopf) nur als Kennfarbe oben und im Titel
        ui::drawPanel (g, b, 10.0f);
        g.setColour (kExpertColour.withAlpha (0.8f));
        g.fillRoundedRectangle (b.withHeight (3.0f), 1.5f);

        ui::drawGearIcon (g, { 22.0f, 16.0f }, kExpertColour);
        g.setFont (ui::uiFont (11.0f, true));
        g.setColour (ui::kDim);
        g.drawText ("EXPERT", 36, 8, 64, 16, juce::Justification::centredLeft);

        const juce::Colour tFilter (0xffbb9cd6);
        auto card = [&] (const juce::String& t, juce::Rectangle<int> r, juce::Colour tint) { ui::drawCard (g, r, t, tint); };
        if (page == 0)
        {
            card ("FILTER (ON ALL ECHOES)", cardA(), tFilter);
            card ("MODULATION", cardB(), ui::kModColour);
            g.setColour (ui::kDim);
            g.setFont (ui::uiFont (11.0f));
            g.drawText ("Sits behind all units and filters every echo once, including the first. The original stays untouched unless INCLUDE DRY is on.",
                        juce::Rectangle<int> (20, kRowY + kCardH + 6 + kLiveH + 6, 920, 32), juce::Justification::topLeft);
        }
        else if (page == 1)
        {
            card ("NOISE SOURCE", sourceCard(), ui::kNoiseColour);
            card ("AUTO-PAN", panCard(), ui::kNoiseColour);
            card ("NOISE FILTER", filterCard(), tFilter);
            card ("NOISE MODULATION", modCard(), ui::kModColour);
        }
        else if (page == 3)
        {
            card ("STEREO", stereoCard(), ui::kAccent);
            card ("ANALOG CHORUS", chorusCard(), ui::kModColour);
            g.setColour (ui::kDim);
            g.setFont (ui::uiFont (11.0f));
            g.drawText ("Stereo works on the echoes; INCLUDE DRY adds the original. Enhance stays mono-compatible. The chorus sits on the echoes or inside every loop.",
                        juce::Rectangle<int> (20, kRowY + kCardH + 8, 920, 32), juce::Justification::topLeft);
        }
        else
        {
            card ("DUCKING", duckCard(), ui::kAccent);
            g.setColour (ui::kDim);
            g.setFont (ui::uiFont (11.0f));
            g.drawText ("The echoes dip while the input is above the threshold and come back in the pauses (vocals, solos). The original and the noise stay untouched.",
                        juce::Rectangle<int> (20, kRowY + kCardH + 8, 920, 32), juce::Justification::topLeft);
        }
    }

    void resized() override
    {
        using namespace qe;
        auto at = [] (juce::Component* c, juce::Rectangle<int> r) { if (c != nullptr) c->setBounds (r); };
        for (int i = 0; i < kPages; ++i)
            tabs[(size_t) i].setBounds (100 + i * 118, 5, 112, 22);

        // kleine Regler: 80 breit, 84 hoch
        auto kb = [] (juce::Rectangle<int> card, int x, int w = 80) { return juce::Rectangle<int> (card.getX() + x, card.getY() + 16, w, 86); };
        auto tg = [] (juce::Rectangle<int> card, int x, int row, int w = 100) { return juce::Rectangle<int> (card.getX() + x, card.getY() + 20 + row * 34, w, 28); };
        // LFO-Block: Formen als Symbole (zwei Reihen), darunter LFO an und Sync
        auto lfoBlock = [&] (juce::Rectangle<int> card, int wave, int lfoOn, int sync, int width)
        {
            at (waves[(size_t) wave].get(), juce::Rectangle<int> (card.getX() + 8, card.getY() + 18, width, 44));
            const int half = (width - 4) / 2;
            at (toggles[(size_t) lfoOn].get(), juce::Rectangle<int> (card.getX() + 6, card.getY() + 66, half + 4, 28));
            at (toggles[(size_t) sync].get(), juce::Rectangle<int> (card.getX() + 10 + half, card.getY() + 66, half + 2, 28));
        };

        // ---- globaler Filter ----
        const auto a = cardA(), b = cardB();
        gfGraph.setBounds (a.getX() + 8, a.getY() + 18, 356, 82);
        at (toggles[gfInclDry].get(), tg (a, 372, 0, 106));
        at (toggles[gfSkipFirst].get(), tg (a, 372, 1, 106));
        lfoBlock (b, gfWave, gfLfoOn, gfSync, 126);
        gfLive.setBounds (12, kRowY + kCardH + 6, 936, kLiveH);
        at (knobs[gfRate].get(), kb (b, 140, 74));  at (knobs[gfDiv].get(), kb (b, 140, 74));
        at (knobs[gfDepth].get(), kb (b, 216, 74));
        at (knobs[gfPhase].get(), kb (b, 292, 74));
        at (knobs[gfEnv].get(), kb (b, 368, 74));

        // ---- Noise ----
        const auto s = sourceCard();
        at (toggles[nzOn].get(), juce::Rectangle<int> (s.getX() + 12, s.getY() + 36, 124, 34));
        const int order[5] = { nzLevel, nzColor, nzCrackle, nzWidth, nzInto };
        for (int i = 0; i < 5; ++i)
            at (knobs[(size_t) order[i]].get(), kb (s, 152 + i * 100, 84));
        const auto pc = panCard();
        at (knobs[nzPanAmt].get(), kb (pc, 6, 76));
        at (knobs[nzPanRate].get(), kb (pc, 82, 76));  at (knobs[nzPanDiv].get(), kb (pc, 82, 76));
        at (toggles[nzPanSync].get(), juce::Rectangle<int> (pc.getX() + 164, pc.getY() + 42, 70, 28));

        const auto f = filterCard(), m = modCard();
        nzGraph.setBounds (f.getX() + 8, f.getY() + 18, f.getWidth() - 16, 82);
        nzLive.setBounds (12, f.getBottom() + 6, 936, kLiveH);
        lfoBlock (m, nzWave, nzLfoOn, nzSync, 116);
        at (knobs[nzRate].get(), kb (m, 130, 78));  at (knobs[nzDiv].get(), kb (m, 130, 78));
        at (knobs[nzDepthF].get(), kb (m, 208, 78));
        at (knobs[nzDepthL].get(), kb (m, 286, 78));
        at (knobs[nzPhase].get(), kb (m, 364, 78));
        at (knobs[nzEnv].get(), kb (m, 442, 78));

        // ---- Ducking ----
        const auto d = duckCard();
        at (toggles[dkOn].get(), juce::Rectangle<int> (d.getX() + 12, d.getY() + 36, 136, 34));
        const int duckOrder[4] = { dkThreshold, dkAmount, dkAttack, dkRelease };
        for (int i = 0; i < 4; ++i)
            at (knobs[(size_t) duckOrder[i]].get(), kb (d, 170 + i * 100, 84));
        duckMeter.setBounds (d.getX() + 600, d.getY() + 20, 310, 80);

        // ---- Stereo und Chorus ----
        const auto st = stereoCard(), ch = chorusCard();
        at (knobs[stWidth].get(), kb (st, 12, 84));
        at (knobs[stEnhance].get(), kb (st, 104, 84));
        at (toggles[stInclDry].get(), juce::Rectangle<int> (st.getX() + 200, st.getY() + 36, 136, 34));
        at (toggles[chOn].get(), juce::Rectangle<int> (ch.getX() + 12, ch.getY() + 22, 130, 30));
        at (waves[chMode].get(), juce::Rectangle<int> (ch.getX() + 12, ch.getY() + 60, 130, 30));
        at (knobs[chMix].get(), kb (ch, 156, 84));
        at (waves[chPlace].get(), juce::Rectangle<int> (ch.getX() + 252, ch.getY() + 40, 220, 30));
    }

private:
    static constexpr int kRowY = 34, kCardH = 106;
    inline static const juce::Colour kExpertColour { 0xff5f90c4 };   // Kennfarbe (wie der EXPERT-Knopf)

    juce::Rectangle<int> cardA() const { return { 12, kRowY, 482, kCardH }; }
    juce::Rectangle<int> cardB() const { return { 502, kRowY, 446, kCardH }; }
    juce::Rectangle<int> sourceCard() const { return { 12, kRowY, 686, kCardH }; }
    juce::Rectangle<int> panCard() const { return { 706, kRowY, 242, kCardH }; }
    juce::Rectangle<int> filterCard() const { return { 12, kRowY + kCardH + 6, 392, kCardH }; }
    juce::Rectangle<int> modCard() const { return { 412, kRowY + kCardH + 6, 536, kCardH }; }
    juce::Rectangle<int> duckCard() const { return { 12, kRowY, 936, kCardH }; }
    juce::Rectangle<int> stereoCard() const { return { 12, kRowY, 352, kCardH }; }
    juce::Rectangle<int> chorusCard() const { return { 372, kRowY, 576, kCardH }; }

    void updateVisibility()
    {
        using namespace qe;
        auto val = [this] (int g) { return ui::currentValue (proc.apvts, qe::kGlobalIds[g]) > 0.5f; };   // aktuell, siehe currentValue
        auto show = [] (juce::Component* c, bool v) { if (c != nullptr) c->setVisible (v); };
        auto dim = [] (juce::Component* c, bool on) { if (c != nullptr) { c->setEnabled (on); c->setAlpha (on ? 1.0f : 0.5f); } };

        const bool p0 = page == 0, p1 = page == 1, p2 = page == 2, p3 = page == 3;
        gfGraph.setVisible (p0);
        gfLive.setVisible (p0);
        nzLive.setVisible (p1);
        for (int g : { gfDepth, gfPhase, gfEnv }) show (knobs[(size_t) g].get(), p0);
        for (int g : { gfInclDry, gfSkipFirst, gfLfoOn, gfSync }) show (toggles[(size_t) g].get(), p0);
        show (waves[gfWave].get(), p0);
        show (knobs[gfRate].get(), p0 && ! val (gfSync));
        show (knobs[gfDiv].get(), p0 && val (gfSync));

        nzGraph.setVisible (p1);
        for (int g : { nzLevel, nzColor, nzCrackle, nzWidth, nzInto, nzDepthF, nzDepthL, nzPhase, nzEnv }) show (knobs[(size_t) g].get(), p1);
        for (int g : { nzOn, nzLfoOn, nzSync, nzPanSync }) show (toggles[(size_t) g].get(), p1);
        show (knobs[nzPanAmt].get(), p1);
        show (knobs[nzPanRate].get(), p1 && ! val (nzPanSync));
        show (knobs[nzPanDiv].get(), p1 && val (nzPanSync));
        show (waves[nzWave].get(), p1);
        show (knobs[nzRate].get(), p1 && ! val (nzSync));
        show (knobs[nzDiv].get(), p1 && val (nzSync));

        for (auto* c : std::initializer_list<juce::Component*> { waves[gfWave].get(), toggles[gfSync].get(), knobs[gfRate].get(), knobs[gfDiv].get(), knobs[gfDepth].get(), knobs[gfPhase].get() })
            dim (c, val (gfLfoOn));
        for (auto* c : std::initializer_list<juce::Component*> { waves[nzWave].get(), toggles[nzSync].get(), knobs[nzRate].get(), knobs[nzDiv].get(), knobs[nzPhase].get() })
            dim (c, val (nzLfoOn));

        show (toggles[dkOn].get(), p2);
        for (int g : { dkThreshold, dkAmount, dkAttack, dkRelease })
        {
            show (knobs[(size_t) g].get(), p2);
            dim (knobs[(size_t) g].get(), val (dkOn));
        }
        duckMeter.setVisible (p2);

        for (int g : { stWidth, stEnhance, chMix }) show (knobs[(size_t) g].get(), p3);
        for (int g : { stInclDry, chOn }) show (toggles[(size_t) g].get(), p3);
        for (int g : { chMode, chPlace }) show (waves[(size_t) g].get(), p3);
        for (auto* c : std::initializer_list<juce::Component*> { waves[chMode].get(), waves[chPlace].get(), knobs[chMix].get() })
            dim (c, val (chOn));

        // Abdunkeln, was gerade nichts bewirkt: Modulation des globalen Filters ohne HP/LP, Schalter ohne Filter,
        // Noise-Regler bei Noise aus, Include dry bei neutralem Stereo
        const bool gfF = val (gfHpOn) || val (gfLpOn);
        if (! gfF)
            for (auto* c : std::initializer_list<juce::Component*> { waves[gfWave].get(), toggles[gfLfoOn].get(), toggles[gfSync].get(), knobs[gfRate].get(),
                                                                      knobs[gfDiv].get(), knobs[gfDepth].get(), knobs[gfPhase].get(), knobs[gfEnv].get() })
                dim (c, false);
        for (int g : { gfInclDry, gfSkipFirst }) dim (toggles[(size_t) g].get(), gfF || val (gfBellOn));
        if (! val (nzOn))
        {
            for (int g : { nzLevel, nzColor, nzCrackle, nzWidth, nzInto, nzDepthF, nzDepthL, nzPhase, nzEnv, nzRate, nzDiv, nzPanAmt, nzPanRate, nzPanDiv })
                dim (knobs[(size_t) g].get(), false);
            for (int g : { nzLfoOn, nzSync, nzPanSync }) dim (toggles[(size_t) g].get(), false);
            dim (waves[nzWave].get(), false);
            dim (&nzGraph, false);
        }
        else dim (&nzGraph, true);
        const bool stereoUsed = std::abs (ui::currentValue (proc.apvts, kGlobalIds[stWidth]) - 1.0f) > 0.001f
                                || ui::currentValue (proc.apvts, kGlobalIds[stEnhance]) > 0.001f;
        dim (toggles[stInclDry].get(), stereoUsed);
    }

    QuadEchoProcessor& proc;
    int page = 0;
    std::array<std::unique_ptr<ui::Knob>, qe::gCount> knobs;
    std::array<std::unique_ptr<ui::Toggle>, qe::gCount> toggles;
    std::array<std::unique_ptr<ui::IconChoice>, qe::gCount> waves;
    std::array<juce::TextButton, kPages> tabs;
    DuckMeter duckMeter;
    ExpertModDisplay gfLive, nzLive;
    ui::FilterGraph gfGraph, nzGraph;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> watchers;
};
