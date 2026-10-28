// Rendert den Plugin-Editor ohne Fenster in eine PNG-Datei (fuer UI-Pruefungen).
// Aufruf: QuadEchoUiShot out.png [id=wert ...] [expert=0/1] [page=0/1] [expertPage=0/1/2] [unit=0..3] [width=1000] [audio=1]
// Beispiel: QuadEchoUiShot noise.png nz_on=1 expert=1 expertPage=1
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdlib>
#include <iostream>
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::cout << "Aufruf: QuadEchoUiShot out.png [id=wert ...]" << std::endl; return 2; }

    QuadEchoProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    int width = 1000;
    bool audio = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        const auto key = arg.upToFirstOccurrenceOf ("=", false, false);
        const auto val = arg.fromFirstOccurrenceOf ("=", false, false);
        if (key == "expert") proc.apvts.state.setProperty ("uiExpert", val.getIntValue() != 0, nullptr);
        else if (key == "page") proc.apvts.state.setProperty ("uiPage", val.getIntValue(), nullptr);
        else if (key == "expertPage") proc.apvts.state.setProperty ("uiExpertPage", val.getIntValue(), nullptr);
        else if (key == "unit") proc.apvts.state.setProperty ("uiUnit", val.getIntValue(), nullptr);
        else if (key == "width") width = val.getIntValue();
        else if (key == "audio") audio = val.getIntValue() != 0;
        else if (key == "link") {}    // wird nach dem Erzeugen des Editors ausgewertet
        else if (auto* p = proc.apvts.getParameter (key))
            p->setValueNotifyingHost (p->convertTo0to1 (val.getFloatValue()));
        else std::cout << "Unbekannter Parameter: " << key << std::endl;
    }

    if (audio)   // ein wenig Audio verarbeiten, damit Pegel, LFO-Phase und Huellkurve etwas anzeigen
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (7);
        for (int block = 0; block < 400; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, 0.3f * std::sin ((float) (block * 512 + i) * 0.06f) * (0.5f + 0.5f * std::sin ((float) block * 0.05f)));
            proc.processBlock (buf, midi);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (width, (int) std::round (width * (double) 1.0 * editor->getHeight() / (double) editor->getWidth()));

    // link=N: den sichtbaren Knopf "LINK" N-mal klicken und nach jedem Klick Parameter und Anzeige ausgeben
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]).startsWith ("link="))
        {
            const int clicks = juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false).getIntValue();
            std::function<juce::Button* (juce::Component&)> find = [&] (juce::Component& c) -> juce::Button*
            {
                for (auto* child : c.getChildren())
                {
                    if (auto* b = dynamic_cast<juce::Button*> (child))
                    {
                        bool visible = true;   // sichtbar bis zum Editor (isShowing gilt nur auf dem Bildschirm)
                        for (juce::Component* p = b; p != nullptr && p != editor.get(); p = p->getParentComponent())
                            visible = visible && p->isVisible();
                        if (b->getButtonText() == "LINK" && visible) return b;
                    }
                    if (auto* r = find (*child)) return r;
                }
                return nullptr;
            };
            if (auto* b = find (*editor))
                for (int k = 1; k <= clicks; ++k)
                {
                    b->onClick();   // wie ein Mausklick (der Knopf schaltet sich nicht selbst)
                    // sichtbarer Zeitregler der rechten Seite (Wert beginnt mit "R "): bedienbar oder ausgegraut?
                    std::function<juce::Slider* (juce::Component&)> findR = [&] (juce::Component& c) -> juce::Slider*
                    {
                        for (auto* child : c.getChildren())
                        {
                            if (auto* s = dynamic_cast<juce::Slider*> (child))
                                if (s->isVisible() && child->getParentComponent()->isVisible() && s->getTextFromValue (s->getValue()).startsWith ("R "))
                                    return s;
                            if (child->isVisible())
                                if (auto* r = findR (*child)) return r;
                        }
                        return nullptr;
                    };
                    auto* rKnob = findR (*editor);
                    std::cout << "Klick " << k << ": a_link = " << proc.apvts.getRawParameterValue ("a_link")->load()
                              << ", LINK leuchtet = " << (b->getToggleState() ? 1 : 0)
                              << ", rechter Regler bedienbar = " << (rKnob != nullptr && rKnob->isEnabled() ? 1 : 0) << std::endl;
                }
            else std::cout << "LINK nicht gefunden" << std::endl;
        }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
    {
        std::cout << "Konnte " << out.getFullPathName() << " nicht schreiben" << std::endl;
        return 1;
    }
    std::cout << out.getFullPathName() << "  " << image.getWidth() << " x " << image.getHeight() << std::endl;
    return 0;
}
