// Stereo Bucket Flanger — editor.
//
// Original rack-panel design. Generic hardware cues only (rack ears, pointer knobs with scale rings,
// toggle switches, LEDs, engraved section labels); colour scheme, layout, typography and naming are
// deliberately unlike the reference hardware (spec §1/§6/§9: no panel artwork or trade dress).
// Drawn at a base size and scaled, so the window is freely resizable.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace sbfui
{
// ---- palette ---------------------------------------------------------------------------------
static const juce::Colour kPanel      (0xffd8d3c6);   // champagne anodised aluminium
static const juce::Colour kPanelDark  (0xffc9c3b4);
static const juce::Colour kPanelLight (0xffe6e2d8);
static const juce::Colour kEar        (0xff2b2e33);
static const juce::Colour kInk        (0xff262a30);   // engraved legends
static const juce::Colour kInk2       (0xff6a6f78);
static const juce::Colour kRule       (0xffaaa496);
static const juce::Colour kAccent     (0xff1f8a86);   // teal
static const juce::Colour kKnob       (0xff2a2d33);
static const juce::Colour kKnobEdge   (0xff15171a);
static const juce::Colour kPointer    (0xfff4f1ea);
static const juce::Colour kRecess     (0xff1b1e22);
static const juce::Colour kLcd        (0xffe7dfc8);
static const juce::Colour kAmber      (0xfff2a33a);
static const juce::Colour kGreen      (0xff3cc46c);
static const juce::Colour kRed        (0xffe4473c);

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font legend (float size) { return font (size, true).withExtraKerningFactor (0.08f); }

// ---- knob look -------------------------------------------------------------------------------
struct ModeKnob : juce::Slider { using Slider::Slider; };

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::Slider::textBoxTextColourId, kLcd);
        setColour (juce::Slider::textBoxBackgroundColourId, kRecess);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a3e45));
        setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.5f));
        setColour (juce::Label::textColourId, kInk);
        setColour (juce::ComboBox::backgroundColourId, kRecess);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3e45));
        setColour (juce::ComboBox::textColourId, kLcd);
        setColour (juce::ComboBox::arrowColourId, kAmber);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff23262b));
        setColour (juce::PopupMenu::textColourId, kLcd);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, kLcd);
        setColour (juce::TextEditor::highlightColourId, kAccent.withAlpha (0.5f));
        setColour (juce::CaretComponent::caretColourId, kLcd);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const bool mode = dynamic_cast<ModeKnob*> (&s) != nullptr;
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        const float cx = r.getCentreX(), cy = r.getCentreY();
        const float rOuter = d * 0.5f;                 // scale ring radius
        const float rBody  = d * 0.33f;                // knob body
        const float ang = a0 + pos * (a1 - a0);

        // scale ring: ticks (+ numerals for large knobs)
        const int nTicks = mode ? (int) std::lround (s.getMaximum() - s.getMinimum()) + 1 : 11;
        g.setColour (kInk);
        for (int i = 0; i < nTicks; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (nTicks - 1);
            const bool major = mode || (i % 5 == 0);
            const float len = major ? d * 0.05f : d * 0.03f;
            const float r1 = rBody + d * 0.045f;
            juce::Point<float> p1 (cx + std::sin (a) * r1, cy - std::cos (a) * r1);
            juce::Point<float> p2 (cx + std::sin (a) * (r1 + len), cy - std::cos (a) * (r1 + len));
            g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
            if (! mode && d >= 64 && major)
            {
                g.setFont (font (d * 0.085f, true));
                juce::Point<float> pt (cx + std::sin (a) * (rOuter - d * 0.04f), cy - std::cos (a) * (rOuter - d * 0.04f));
                g.drawText (juce::String (i), juce::Rectangle<float> (20, 12).withCentre (pt), juce::Justification::centred);
            }
        }
        // body: bevel + dark cap
        g.setColour (kKnobEdge);
        g.fillEllipse (cx - rBody, cy - rBody, 2 * rBody, 2 * rBody);
        juce::ColourGradient grad (kKnob.brighter (0.35f), cx - rBody * 0.5f, cy - rBody * 0.6f, kKnob.darker (0.3f), cx + rBody * 0.6f, cy + rBody * 0.8f, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - rBody * 0.93f, cy - rBody * 0.93f, 2 * rBody * 0.93f, 2 * rBody * 0.93f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (cx - rBody * 0.80f, cy - rBody * 0.80f, 2 * rBody * 0.80f, 2 * rBody * 0.80f, 1.0f);
        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-d * 0.018f, -rBody * 0.92f, d * 0.036f, rBody * 0.55f, d * 0.012f);
        g.setColour (kPointer);
        g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
        // active arc (teal) for continuous knobs
        if (! mode)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, rBody + d * 0.025f, rBody + d * 0.025f, 0.0f, a0, ang, true);
            g.setColour (kAccent);
            g.strokePath (arc, juce::PathStrokeType (d * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.5f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.5f); }
    juce::Font getPopupMenuFont() override { return font (12.5f); }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        juce::Path tri;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (tri);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
        l.setFont (getComboBoxFont (box));
    }
};

// ---- toggle switch bound to a 2-state parameter ------------------------------------------------
class ToggleSwitch : public juce::Component
{
public:
    ToggleSwitch (juce::RangedAudioParameter& p, juce::String top, juce::String bottom)
        : legendTop (std::move (top)), legendBottom (std::move (bottom)),
          att (p, [this] (float v) { state = v > 0.5f; repaint(); })
    {
        att.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (state ? 0.0f : 1.0f); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX();
        // legends
        g.setColour (kInk);
        g.setFont (font (9.5f, true));
        g.drawText (legendTop, r.removeFromTop (12.0f), juce::Justification::centred);
        g.drawText (legendBottom, r.removeFromBottom (12.0f), juce::Justification::centred);
        // bezel + slot
        auto bez = juce::Rectangle<float> (22.0f, r.getHeight() - 4.0f).withCentre (r.getCentre());
        g.setColour (kPanelDark.darker (0.15f));
        g.fillRoundedRectangle (bez, 4.0f);
        g.setColour (kInk2.withAlpha (0.5f));
        g.drawRoundedRectangle (bez, 4.0f, 1.0f);
        auto slot = juce::Rectangle<float> (8.0f, bez.getHeight() - 14.0f).withCentre (bez.getCentre());
        g.setColour (kRecess);
        g.fillRoundedRectangle (slot, 4.0f);
        // lever: points to the active legend (top = state 0)
        const float leverLen = slot.getHeight() * 0.62f;
        const float y0 = slot.getCentreY();
        const float y1 = state ? y0 + leverLen : y0 - leverLen;
        g.setColour (kKnob);
        g.drawLine (cx, y0, cx, y1, 5.0f);
        g.setColour (kAccent);
        g.fillEllipse (cx - 6.0f, y1 - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (cx - 3.0f, y1 - 4.5f, 4.0f, 3.0f);
    }
private:
    juce::String legendTop, legendBottom;
    bool state = false;
    juce::ParameterAttachment att;
};

// ---- illuminated push button bound to a bool parameter ----------------------------------------
class LitButton : public juce::Component
{
public:
    LitButton (juce::RangedAudioParameter& p, juce::String text, juce::Colour led)
        : label (std::move (text)), ledColour (led), att (p, [this] (float v) { on = v > 0.5f; repaint(); })
    {
        att.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kKnobEdge);
        g.fillRoundedRectangle (r, 4.0f);
        juce::ColourGradient grad (kKnob.brighter (0.25f), r.getX(), r.getY(), kKnob.darker (0.2f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
        auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() });
        g.setColour (on ? ledColour : ledColour.withAlpha (0.18f));
        g.fillEllipse (led);
        if (on) { g.setColour (ledColour.withAlpha (0.25f)); g.fillEllipse (led.expanded (3.0f)); }
        g.setColour (kLcd);
        g.setFont (font (11.0f, true).withExtraKerningFactor (0.06f));
        g.drawText (label, r.withTrimmedLeft (24.0f), juce::Justification::centredLeft);
    }
private:
    juce::String label; juce::Colour ledColour; bool on = false;
    juce::ParameterAttachment att;
};

// ---- the panel (base-size content; scaled by the editor) ---------------------------------------
class Panel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1180, H = 340;
    static constexpr int kEarW = 24;

    explicit Panel (SbfProcessor& p) : proc (p)
    {
        setLookAndFeel (&look);
        auto& ap = proc.apvts;
        auto param = [&] (const char* id) -> juce::RangedAudioParameter& { return *ap.getParameter (id); };

        auto knob = [&] (juce::Slider& s, const char* id, std::unique_ptr<SA>& att, int tbW)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, tbW, 16);
            s.setLookAndFeel (&look);
            addAndMakeVisible (s);
            att = std::make_unique<SA> (ap, id, s);
        };
        knob (manual,   SbfProcessor::kManual,   aManual, 72);
        knob (feedback, SbfProcessor::kFeedback, aFeedback, 56);
        knob (rate,     SbfProcessor::kRate,     aRate, 66);
        knob (depth,    SbfProcessor::kDepth,    aDepth, 56);
        knob (inFine,   SbfProcessor::kInFine,   aInFine, 56);
        knob (outFine,  SbfProcessor::kOutFine,  aOutFine, 56);
        knob (cv,       SbfProcessor::kCv,       aCv, 50);

        mode.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mode.setRotaryParameters (juce::MathConstants<float>::pi * 1.4f, juce::MathConstants<float>::pi * 2.6f, true);
        mode.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        mode.setLookAndFeel (&look);
        addAndMakeVisible (mode);
        aMode = std::make_unique<SA> (ap, SbfProcessor::kMode, mode);

        chbMod  = std::make_unique<ToggleSwitch> (param (SbfProcessor::kChBMod), "NORM", "INV");
        phaseA  = std::make_unique<ToggleSwitch> (param (SbfProcessor::kPhaseA), "NORM", "INV");
        phaseB  = std::make_unique<ToggleSwitch> (param (SbfProcessor::kPhaseB), "NORM", "INV");
        inPad   = std::make_unique<ToggleSwitch> (param (SbfProcessor::kInPad),  "0 dB", "-20 dB");
        outPad  = std::make_unique<ToggleSwitch> (param (SbfProcessor::kOutPad), "0 dB", "-20 dB");
        noise   = std::make_unique<ToggleSwitch> (param (SbfProcessor::kNoise),  "OFF", "ON");
        for (auto* t : { chbMod.get(), phaseA.get(), phaseB.get(), inPad.get(), outPad.get(), noise.get() }) addAndMakeVisible (t);
        bypass = std::make_unique<LitButton> (param (SbfProcessor::kBypass), "BYPASS", kRed);
        addAndMakeVisible (*bypass);

        auto combo = [&] (juce::ComboBox& c, const char* id, std::unique_ptr<CA>& att)
        {
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter (id))) c.addItemList (pc->choices, 1);
            c.setLookAndFeel (&look);
            addAndMakeVisible (c);
            att = std::make_unique<CA> (ap, id, c);
        };
        combo (cvMode, SbfProcessor::kCvMode, aCvMode);
        combo (sync, SbfProcessor::kSync, aSync);

        setSize (W, H);
        startTimerHz (30);
    }
    ~Panel() override
    {
        for (auto* c : getChildren()) c->setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    // ---- layout (base coordinates) ----
    struct Section { const char* title; int x0, x1; };
    static constexpr Section kSections[] = {
        { "INPUT", 40, 162 }, { "EFFECT MODE", 172, 312 }, { "DELAY", 322, 560 }, { "MODULATION", 570, 800 },
        { "PHASE", 810, 910 }, { "EXT CV", 920, 1040 }, { "OUTPUT", 1050, 1140 } };

    void resized() override
    {
        auto circle = [] (int cx, int cy, int d) { return juce::Rectangle<int> (d, d + 18).withCentre ({ cx, cy + 9 }); }; // + textbox
        manual.setBounds (circle (420, 172, 132));
        feedback.setBounds (circle (508, 156, 88));
        rate.setBounds (circle (632, 156, 88));
        depth.setBounds (circle (740, 156, 88));
        inFine.setBounds (circle (118, 150, 70));
        outFine.setBounds (circle (1095, 150, 70));
        cv.setBounds (circle (980, 212, 72));
        mode.setBounds (juce::Rectangle<int> (80, 80).withCentre ({ 242, 166 }));
        inPad->setBounds (54, 232, 44, 72);
        outPad->setBounds (1064, 232, 44, 72);
        noise->setBounds (1106, 232, 44, 72);
        chbMod->setBounds (590, 232, 44, 72);
        phaseA->setBounds (814, 150, 44, 92);
        phaseB->setBounds (862, 150, 44, 92);
        sync.setBounds (660, 262, 128, 24);
        cvMode.setBounds (930, 116, 100, 22);
        bypass->setBounds (188, 250, 108, 30);
    }

    void paint (juce::Graphics& g) override
    {
        // panel + brushed texture
        g.fillAll (kPanel);
        {
            juce::ColourGradient grad (kPanelLight, 0, 0, kPanelDark, 0, (float) H, false);
            g.setGradientFill (grad);
            g.fillRect (getLocalBounds());
            juce::Random rnd (7);
            for (int y = 0; y < H; y += 2)
            {
                g.setColour (juce::Colours::white.withAlpha (rnd.nextFloat() * 0.07f));
                g.drawHorizontalLine (y, (float) kEarW, (float) (W - kEarW));
            }
        }
        // rack ears
        for (int side = 0; side < 2; ++side)
        {
            auto ear = juce::Rectangle<int> (side == 0 ? 0 : W - kEarW, 0, kEarW, H);
            g.setColour (kEar);
            g.fillRect (ear);
            g.setColour (kEar.brighter (0.25f));
            g.drawVerticalLine (side == 0 ? kEarW - 1 : W - kEarW, 0.0f, (float) H);
            for (int yy : { 26, H - 26 })
            {
                auto s = juce::Rectangle<float> (10, 10).withCentre ({ (float) ear.getCentreX(), (float) yy });
                g.setColour (juce::Colour (0xff8d9199));
                g.fillEllipse (s);
                g.setColour (kEar.darker (0.4f));
                g.drawLine (s.getX() + 2, s.getCentreY(), s.getRight() - 2, s.getCentreY(), 1.4f);
            }
        }
        // header
        g.setColour (kInk);
        g.setFont (legend (17.0f));
        g.drawText ("STEREO BUCKET FLANGER", 42, 16, 420, 24, juce::Justification::centredLeft);
        g.setColour (kInk2);
        g.setFont (font (10.5f));
        g.drawText ("two-channel 1024-stage bucket-brigade flanger / chorus", 44, 38, 420, 14, juce::Justification::centredLeft);
        g.setColour (kAccent);
        g.fillRoundedRectangle (42.0f, 58.0f, (float) (W - 2 * kEarW - 36), 2.0f, 1.0f);
        g.setColour (kInk2);
        g.setFont (font (10.0f));
        g.drawText (status, W - kEarW - 320, 30, 300, 14, juce::Justification::centredRight);

        // sections
        for (auto& s : kSections)
        {
            g.setColour (kInk);
            g.setFont (legend (9.5f));
            g.drawText (s.title, s.x0, 70, s.x1 - s.x0, 14, juce::Justification::centred);
            g.setColour (kRule);
            g.drawRoundedRectangle ((float) s.x0, 88.0f, (float) (s.x1 - s.x0), 222.0f, 6.0f, 1.0f);
            g.setColour (kPanelLight.withAlpha (0.6f));
            g.drawRoundedRectangle ((float) s.x0 + 1, 89.0f, (float) (s.x1 - s.x0) - 2, 220.0f, 5.0f, 1.0f);
        }
        auto cap = [&] (const juce::String& t, int cx, int y) { g.setColour (kInk); g.setFont (legend (9.0f)); g.drawText (t, cx - 60, y, 120, 12, juce::Justification::centred); };
        cap ("IN TRIM", 118, 98);   cap ("LEVEL", 76, 220);
        cap ("MANUAL", 420, 90);     cap ("FEEDBACK", 508, 96);
        cap ("RATE", 632, 96);      cap ("DEPTH", 740, 96);  cap ("CH-B MOD", 612, 220); cap ("RATE SYNC", 724, 248);
        cap ("CH-A", 836, 136);      cap ("CH-B", 884, 136);
        cap ("SOURCE", 980, 102);    cap ("CV VALUE", 980, 160);
        cap ("OUT TRIM", 1095, 98); cap ("LEVEL", 1086, 220); cap ("NOISE", 1128, 220);

        // mode legends around the selector
        static const char* modes[] = { "OFF", "FL I", "FL II", "FL III", "CHORUS" };
        const float a0 = juce::MathConstants<float>::pi * 1.4f, a1 = juce::MathConstants<float>::pi * 2.6f;
        g.setColour (kInk);
        g.setFont (font (9.5f, true));
        for (int i = 0; i < 5; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 4.0f;
            juce::Point<float> pt (242.0f + std::sin (a) * 72.0f, 166.0f + 2.0f - std::cos (a) * 58.0f);
            g.drawText (modes[i], juce::Rectangle<float> (48, 12).withCentre (pt), juce::Justification::centred);
        }

        // LEDs
        const auto& m = proc.engine.meters;
        drawLed (g, 588, 104, m.ledNeg.load(), kAmber);  g.setColour (kInk2); g.setFont (font (8.5f, true)); g.drawText ("-", 580, 112, 16, 10, juce::Justification::centred);
        drawLed (g, 676, 104, m.ledPos.load(), kAmber);  g.setColour (kInk2); g.drawText ("+", 668, 112, 16, 10, juce::Justification::centred);
        drawLed (g, 136, 250, m.inSignal.load(), kGreen); g.setColour (kInk2); g.drawText ("SIG", 122, 260, 28, 10, juce::Justification::centred);
        drawLed (g, 136, 284, m.inClip.load(), kRed);    g.setColour (kInk2); g.drawText ("CLIP", 122, 294, 28, 10, juce::Justification::centred);
    }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    static void drawLed (juce::Graphics& g, int cx, int cy, bool on, juce::Colour c)
    {
        auto r = juce::Rectangle<float> (10, 10).withCentre ({ (float) cx, (float) cy });
        g.setColour (kRecess);
        g.fillEllipse (r.expanded (2.0f));
        g.setColour (on ? c : c.withAlpha (0.2f));
        g.fillEllipse (r);
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (r.expanded (4.0f)); }
    }

    void timerCallback() override
    {
        const auto& m = proc.engine.meters;
        status = juce::String::formatted ("latency %d smp   cpu %.1f %%   ", proc.getLatencySamples(), 100.0f * m.cpuLoad.load()) + proc.tunablesStatus;
        repaint (576, 94, 112, 30);
        repaint (120, 240, 40, 70);
        repaint (W - kEarW - 320, 30, 300, 14);
    }

    SbfProcessor& proc;
    Look look;
    juce::Slider manual, feedback, rate, depth, inFine, outFine, cv;
    ModeKnob mode;
    juce::ComboBox cvMode, sync;
    std::unique_ptr<ToggleSwitch> chbMod, phaseA, phaseB, inPad, outPad, noise;
    std::unique_ptr<LitButton> bypass;
    std::unique_ptr<SA> aManual, aFeedback, aRate, aDepth, aInFine, aOutFine, aCv, aMode;
    std::unique_ptr<CA> aCvMode, aSync;
    juce::String status;
};
} // namespace sbfui

class SbfEditor : public juce::AudioProcessorEditor
{
public:
    explicit SbfEditor (SbfProcessor& p) : AudioProcessorEditor (p), panel (p)
    {
        addAndMakeVisible (panel);
        setResizable (true, true);
        getConstrainer()->setFixedAspectRatio ((double) sbfui::Panel::W / sbfui::Panel::H);
        setResizeLimits (sbfui::Panel::W * 0.6, sbfui::Panel::H * 0.6, sbfui::Panel::W * 2, sbfui::Panel::H * 2);
        setSize (sbfui::Panel::W, sbfui::Panel::H);
    }
    void resized() override
    {
        const float sc = (float) getWidth() / (float) sbfui::Panel::W;
        panel.setTransform (juce::AffineTransform::scale (sc));
        panel.setBounds (0, 0, sbfui::Panel::W, sbfui::Panel::H);
    }
    void paint (juce::Graphics& g) override { g.fillAll (sbfui::kEar); }
    sbfui::Panel panel;
};
