// Renders the editor to a PNG without a host (docs/screenshot, UI checks).
//   sbf_snapshot out.png [scale]
#include <juce_gui_extra/juce_gui_extra.h>
#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("sbf_ui.png"));
    const float scale = argc > 2 ? (float) juce::String (argv[2]).getDoubleValue() : 2.0f;
    SbfProcessor proc;
    proc.prepareToPlay (48000.0, 64);
    // a few parameters off their defaults so the controls show state
    proc.apvts.getParameter (SbfProcessor::kFeedback)->setValueNotifyingHost (0.6f);
    proc.apvts.getParameter (SbfProcessor::kChBMod)->setValueNotifyingHost (1.0f);
    proc.engine.meters.inSignal.store (true);
    proc.engine.meters.ledPos.store (true);
    sbfui::Panel panel (proc);
    const int w = juce::roundToInt (sbfui::Panel::W * scale), h = juce::roundToInt (sbfui::Panel::H * scale);
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        panel.paintEntireComponent (g, true);
    }
    juce::PNGImageFormat png;
    juce::FileOutputStream fo (out);
    if (fo.openedOk()) png.writeImageToStream (img, fo);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), w, h);
    return 0;
}
