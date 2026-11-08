// Stereo Bucket Flanger — editor. Original layout; no manufacturer artwork, names or logos.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SbfLook : public juce::LookAndFeel_V4
{
public:
    SbfLook()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff23262b));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe8a33d));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3f47));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xfff2f2ef));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe6e6e2));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, juce::Colour (0xffb9bcc2));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2f333a));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a505a));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6e6e2));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe8a33d));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2f333a));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e6e2));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffe8a33d));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff23262b));
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffe6e6e2));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a33d));
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff4a505a));
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight()), cx = r.getCentreX(), cy = r.getCentreY(), rad = d * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        g.setColour (findColour (juce::Slider::rotarySliderOutlineColourId));
        juce::Path arc; arc.addCentredArc (cx, cy, rad, rad, 0, a0, a1, true);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        juce::Path val; val.addCentredArc (cx, cy, rad, rad, 0, a0, ang, true);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff181a1e)); g.fillEllipse (cx - rad * 0.72f, cy - rad * 0.72f, rad * 1.44f, rad * 1.44f);
        g.setColour (juce::Colour (0xff3a3f47)); g.drawEllipse (cx - rad * 0.72f, cy - rad * 0.72f, rad * 1.44f, rad * 1.44f, 1.0f);
        juce::Path tick; tick.addRectangle (-1.5f, -rad * 0.7f, 3.0f, rad * 0.32f);
        g.setColour (findColour (juce::Slider::thumbColourId));
        g.fillPath (tick, juce::AffineTransform::rotation (ang).translated (cx, cy));
    }
};

class SbfEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SbfEditor (SbfProcessor& p) : AudioProcessorEditor (p), proc (p)
    {
        setLookAndFeel (&look);
        auto& ap = proc.apvts;
        auto knob = [&] (juce::Slider& s, juce::Label& l, const char* id, const char* name, std::unique_ptr<SA>& att)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
            addAndMakeVisible (s);
            l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
            l.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
            addAndMakeVisible (l);
            att = std::make_unique<SA> (ap, id, s);
        };
        knob (manual, lManual, SbfProcessor::kManual, "MANUAL", aManual);
        knob (rate, lRate, SbfProcessor::kRate, "RATE", aRate);
        knob (depth, lDepth, SbfProcessor::kDepth, "DEPTH", aDepth);
        knob (feedback, lFeedback, SbfProcessor::kFeedback, "FEEDBACK", aFeedback);
        knob (inFine, lInFine, SbfProcessor::kInFine, "IN TRIM", aInFine);
        knob (outFine, lOutFine, SbfProcessor::kOutFine, "OUT TRIM", aOutFine);
        knob (cv, lCv, SbfProcessor::kCv, "CV VALUE", aCv);
        manual.textFromValueFunction = [] (double v) { return juce::String (v, 2) + " ms"; };
        rate.textFromValueFunction = [] (double v) { return juce::String (v, 3) + " Hz"; };
        depth.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " %"; };
        feedback.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " %"; };
        inFine.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
        outFine.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };

        auto combo = [&] (juce::ComboBox& c, juce::Label& l, const char* id, const char* name, std::unique_ptr<CA>& att)
        {
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter (id))) c.addItemList (pc->choices, 1);
            addAndMakeVisible (c);
            l.setText (name, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
            l.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
            addAndMakeVisible (l);
            att = std::make_unique<CA> (ap, id, c);
        };
        combo (mode, lMode, SbfProcessor::kMode, "EFFECT MODE", aMode);
        combo (chbMod, lChbMod, SbfProcessor::kChBMod, "CH-B MOD", aChbMod);
        combo (phaseA, lPhaseA, SbfProcessor::kPhaseA, "CH-A PHASE", aPhaseA);
        combo (phaseB, lPhaseB, SbfProcessor::kPhaseB, "CH-B PHASE", aPhaseB);
        combo (cvMode, lCvMode, SbfProcessor::kCvMode, "EXT CV", aCvMode);
        combo (inPad, lInPad, SbfProcessor::kInPad, "INPUT LEVEL", aInPad);
        combo (outPad, lOutPad, SbfProcessor::kOutPad, "OUTPUT LEVEL", aOutPad);
        combo (sync, lSync, SbfProcessor::kSync, "RATE SYNC", aSync);

        noise.setButtonText ("Noise model"); addAndMakeVisible (noise); aNoise = std::make_unique<BA> (ap, SbfProcessor::kNoise, noise);
        bypass.setButtonText ("Bypass"); addAndMakeVisible (bypass); aBypass = std::make_unique<BA> (ap, SbfProcessor::kBypass, bypass);

        status.setFont (juce::Font (juce::FontOptions (11.0f))); status.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (status);
        setSize (760, 400);
        startTimerHz (30);
    }
    ~SbfEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff23262b));
        g.setColour (juce::Colour (0xff2b2f35));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 8.0f);
        g.setColour (juce::Colour (0xffe6e6e2));
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText ("STEREO BUCKET FLANGER", 22, 14, 400, 26, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8d9199));
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText ("two-channel 1024-stage BBD flanger / chorus model", 24, 38, 400, 16, juce::Justification::centredLeft);

        // LEDs: LFO -/+ peak, input signal / clip
        auto led = [&] (int x, int y, bool on, juce::Colour c, const juce::String& name)
        {
            g.setColour (on ? c : c.withAlpha (0.18f)); g.fillEllipse ((float) x, (float) y, 12.0f, 12.0f);
            g.setColour (juce::Colour (0xff8d9199)); g.setFont (juce::Font (juce::FontOptions (10.5f)));
            g.drawText (name, x - 14, y + 14, 40, 14, juce::Justification::centred);
        };
        const auto& m = proc.engine.meters;
        led (560, 20, m.ledNeg.load(), juce::Colour (0xffe8a33d), "-");
        led (590, 20, m.ledPos.load(), juce::Colour (0xffe8a33d), "+");
        g.setColour (juce::Colour (0xff8d9199)); g.drawText ("LFO", 548, 50, 66, 14, juce::Justification::centred);
        led (650, 20, m.inSignal.load(), juce::Colour (0xff4fcf6a), "SIG");
        led (690, 20, m.inClip.load(), juce::Colour (0xffe65a4f), "CLIP");
        g.drawText ("INPUT", 640, 50, 74, 14, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20).withTrimmedTop (56);
        auto row1 = area.removeFromTop (150);
        const int kw = 118;
        auto place = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) { l.setBounds (r.removeFromTop (18)); s.setBounds (r); };
        auto placeC = [&] (juce::ComboBox& c, juce::Label& l, juce::Rectangle<int> r) { l.setBounds (r.removeFromTop (16)); c.setBounds (r.removeFromTop (26).reduced (6, 0)); };
        auto modeBox = row1.removeFromLeft (150); placeC (mode, lMode, modeBox.withTrimmedTop (36));
        place (manual, lManual, row1.removeFromLeft (kw));
        place (rate, lRate, row1.removeFromLeft (kw));
        place (depth, lDepth, row1.removeFromLeft (kw));
        place (feedback, lFeedback, row1.removeFromLeft (kw));
        place (cv, lCv, row1.removeFromLeft (kw));
        area.removeFromTop (10);
        auto row2 = area.removeFromTop (48);
        const int cw = 120;
        placeC (chbMod, lChbMod, row2.removeFromLeft (cw));
        placeC (phaseA, lPhaseA, row2.removeFromLeft (cw));
        placeC (phaseB, lPhaseB, row2.removeFromLeft (cw));
        placeC (cvMode, lCvMode, row2.removeFromLeft (cw));
        placeC (sync, lSync, row2.removeFromLeft (cw));
        area.removeFromTop (6);
        auto row3 = area.removeFromTop (96);
        placeC (inPad, lInPad, row3.removeFromLeft (cw).withTrimmedTop (20));
        place (inFine, lInFine, row3.removeFromLeft (100));
        placeC (outPad, lOutPad, row3.removeFromLeft (cw).withTrimmedTop (20));
        place (outFine, lOutFine, row3.removeFromLeft (100));
        auto tog = row3.removeFromLeft (150).withTrimmedTop (20);
        noise.setBounds (tog.removeFromTop (24)); bypass.setBounds (tog.removeFromTop (24));
        status.setBounds (getLocalBounds().removeFromBottom (24).reduced (20, 4));
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override
    {
        const auto& m = proc.engine.meters;
        status.setText (juce::String::formatted ("latency %d smp  |  CPU %.1f %%  |  %s", proc.getLatencySamples(), 100.0f * m.cpuLoad.load(), proc.tunablesStatus.toRawUTF8()), juce::dontSendNotification);
        repaint (540, 10, 200, 60);
    }

    SbfProcessor& proc;
    SbfLook look;
    juce::Slider manual, rate, depth, feedback, inFine, outFine, cv;
    juce::Label lManual, lRate, lDepth, lFeedback, lInFine, lOutFine, lCv, lMode, lChbMod, lPhaseA, lPhaseB, lCvMode, lInPad, lOutPad, lSync, status;
    juce::ComboBox mode, chbMod, phaseA, phaseB, cvMode, inPad, outPad, sync;
    juce::ToggleButton noise, bypass;
    std::unique_ptr<SA> aManual, aRate, aDepth, aFeedback, aInFine, aOutFine, aCv;
    std::unique_ptr<CA> aMode, aChbMod, aPhaseA, aPhaseB, aCvMode, aInPad, aOutPad, aSync;
    std::unique_ptr<BA> aNoise, aBypass;
};
