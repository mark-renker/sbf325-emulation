// Stereo Bucket Flanger — JUCE processor wrapping the JUCE-free engine (src/dsp/Flanger.h).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Flanger.h"

class SbfProcessor : public juce::AudioProcessor
{
public:
    SbfProcessor();
    ~SbfProcessor() override = default;

    void prepareToPlay (double sr, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stereo Bucket Flanger"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::File tunablesFile();
    juce::String reloadTunables();     // returns a status line for the UI
    juce::String tunablesStatus;

    sbf::Flanger engine;
    juce::AudioProcessorValueTreeState apvts;
    juce::SpinLock engineLock;

    // parameter IDs
    static constexpr const char* kMode = "mode", *kManual = "manual", *kRate = "rate", *kDepth = "depth", *kFeedback = "feedback",
        *kChBMod = "chb_mod", *kPhaseA = "phase_a", *kPhaseB = "phase_b", *kCvMode = "ext_cv_mode", *kCv = "ext_cv",
        *kInPad = "in_pad", *kInFine = "in_fine", *kOutPad = "out_pad", *kOutFine = "out_fine", *kNoise = "noise",
        *kBypass = "bypass", *kSync = "rate_sync";

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    sbf::Params gatherParams();
    void syncLatency();

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    juce::AudioParameterBool* bypassParam = nullptr;
    sbf::Tunables tunables;
    std::vector<float> dryTmp[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SbfProcessor)
};
