#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sbf;

static const juce::StringArray kSyncChoices { "Off", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
static const double kSyncBeats[] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25 };

juce::AudioProcessorValueTreeState::ParameterLayout SbfProcessor::makeLayout()
{
    using P = juce::ParameterID;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto fl = [] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, const juce::String& unit, int decimals, bool sign = false)
    {
        auto attr = juce::AudioParameterFloatAttributes().withLabel (unit)
            .withStringFromValueFunction ([unit, decimals, sign] (float v, int) { return (sign && v >= 0 ? "+" : "") + juce::String (v, decimals) + (unit.isEmpty() ? "" : " " + unit); })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); });
        return std::make_unique<juce::AudioParameterFloat> (P (id, 1), name, r, def, attr);
    };

    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kMode, 1), "Mode", juce::StringArray { "Off", "Flanger I", "Flanger II", "Flanger III", "Chorus" }, 2));
    juce::NormalisableRange<float> manual (0.5f, 15.0f, 0.001f); manual.setSkewForCentre (2.5f);
    ps.push_back (fl (kManual, "Manual", manual, 2.0f, "ms", 2));
    juce::NormalisableRange<float> rate (0.02f, 5.0f, 0.001f); rate.setSkewForCentre (0.3f);
    ps.push_back (fl (kRate, "Rate", rate, 0.5f, "Hz", 3));
    ps.push_back (fl (kDepth, "Depth", { 0.0f, 100.0f, 0.1f }, 50.0f, "%", 0));
    ps.push_back (fl (kFeedback, "Feedback", { 0.0f, 100.0f, 0.1f }, 0.0f, "%", 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kChBMod, 1), "CH-B Mod", juce::StringArray { "Normal", "Invert" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kPhaseA, 1), "CH-A Phase", juce::StringArray { "Normal", "Invert" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kPhaseB, 1), "CH-B Phase", juce::StringArray { "Normal", "Invert" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kCvMode, 1), "Ext CV", juce::StringArray { "Off", "Sidechain", "Parameter" }, 0));
    ps.push_back (fl (kCv, "Ext CV Value", { -1.0f, 1.0f, 0.001f }, 0.0f, "", 2, true));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kInPad, 1), "Input Level", juce::StringArray { "0 dB", "-20 dB" }, 0));
    ps.push_back (fl (kInFine, "Input Trim", { -6.0f, 6.0f, 0.1f }, 0.0f, "dB", 1, true));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kOutPad, 1), "Output Level", juce::StringArray { "0 dB", "-20 dB" }, 0));
    ps.push_back (fl (kOutFine, "Output Trim", { -6.0f, 6.0f, 0.1f }, 0.0f, "dB", 1, true));
    ps.push_back (std::make_unique<juce::AudioParameterBool> (P (kNoise, 1), "Noise", true));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (P (kSync, 1), "Rate Sync", kSyncChoices, 0));
    ps.push_back (std::make_unique<juce::AudioParameterBool> (P (kBypass, 1), "Bypass", false));
    return { ps.begin(), ps.end() };
}

SbfProcessor::SbfProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Ext CV", juce::AudioChannelSet::mono(), false)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SBFPARAMS", makeLayout())
{
    // looked up after construction: member default-initialisers would otherwise run after makeLayout()
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (kBypass));
    jassert (bypassParam != nullptr);
    setLatencySamples (engine.latencySamples());
    tunablesStatus = reloadTunables();
}

juce::File SbfProcessor::tunablesFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("StereoBucketFlanger").getChildFile ("tunables.txt");
}

// Optional user override file: one "key=value" per line (keys from src/dsp/Tunables.h).
juce::String SbfProcessor::reloadTunables()
{
    Tunables t;
    auto f = tunablesFile();
    int applied = 0, unknown = 0;
    if (f.existsAsFile())
    {
        juce::StringArray lines; f.readLines (lines);
        for (auto& l : lines)
        {
            auto s = l.trim();
            if (s.isEmpty() || s.startsWithChar ('#')) continue;
            if (t.setFromString (s.toStdString())) ++applied; else ++unknown;
        }
    }
    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        tunables = t;
        if (getSampleRate() > 0) engine.prepare (getSampleRate(), tunables);
    }
    if (! f.existsAsFile()) return "tunables: defaults";
    return "tunables: " + juce::String (applied) + " applied" + (unknown ? ", " + juce::String (unknown) + " unknown" : "");
}

bool SbfProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (l.inputBuses.size() > 1)
    {
        auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void SbfProcessor::syncLatency() { setLatencySamples (engine.latencySamples()); }

void SbfProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    engine.prepare (sr, tunables);
    engine.setParams (gatherParams(), true);
    for (auto& v : dryTmp) v.assign ((size_t) std::max (samplesPerBlock, 16), 0.0f);
    syncLatency();
}

Params SbfProcessor::gatherParams()
{
    Params q;
    q.mode = (Mode) (int) p (kMode)->load();
    q.manualMs = p (kManual)->load();
    q.rateHz = p (kRate)->load();
    const int sync = (int) p (kSync)->load();
    if (sync > 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
        q.rateHz = std::clamp (bpm / 60.0 / kSyncBeats[sync], 0.001, 50.0);
    }
    q.depth = p (kDepth)->load() / 100.0;
    q.feedback = p (kFeedback)->load() / 100.0;
    q.chBModInvert = p (kChBMod)->load() > 0.5f;
    q.phaseAInvert = p (kPhaseA)->load() > 0.5f;
    q.phaseBInvert = p (kPhaseB)->load() > 0.5f;
    q.cvMode = (CvMode) (int) p (kCvMode)->load();
    q.cvValue = p (kCv)->load();
    q.inTrimDb = (p (kInPad)->load() > 0.5f ? -20.0 : 0.0) + p (kInFine)->load();
    q.outTrimDb = (p (kOutPad)->load() > 0.5f ? -20.0 : 0.0) + p (kOutFine)->load();
    q.noise = p (kNoise)->load() > 0.5f;
    q.bypass = bypassParam->get();
    return q;
}

void SbfProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    if (bypassParam->get()) { processBlockBypassed (buf, midi); return; }
    const juce::SpinLock::ScopedTryLockType sl (engineLock);
    if (! sl.isLocked()) { buf.clear(); return; }
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buf.getNumSamples();

    engine.setParams (gatherParams());

    auto mainIn = getBusBuffer (buf, true, 0);
    const float* inA = mainIn.getReadPointer (0);
    const float* inB = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : inA;
    const float* cv = nullptr;
    if (getBusCount (true) > 1)
    {
        auto sc = getBusBuffer (buf, true, 1);
        if (sc.getNumChannels() > 0 && ! getBus (true, 1)->getCurrentLayout().isDisabled()) cv = sc.getReadPointer (0);
    }
    // inputs may alias outputs: copy first
    if ((int) dryTmp[0].size() < n) for (auto& v : dryTmp) v.resize ((size_t) n);
    std::copy (inA, inA + n, dryTmp[0].begin());
    std::copy (inB, inB + n, dryTmp[1].begin());
    auto mainOut = getBusBuffer (buf, false, 0);
    engine.process (dryTmp[0].data(), dryTmp[1].data(), mainOut.getWritePointer (0), mainOut.getWritePointer (1), n, cv);

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs * getSampleRate() / n);
    engine.meters.cpuLoad.store (0.9f * engine.meters.cpuLoad.load() + 0.1f * load);
}

// Hard bypass: unity, but delayed by the reported latency so the host's compensation stays valid.
void SbfProcessor::processBlockBypassed (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    const juce::SpinLock::ScopedTryLockType sl (engineLock);
    if (! sl.isLocked()) return;
    const int n = buf.getNumSamples();
    if ((int) dryTmp[0].size() < n) for (auto& v : dryTmp) v.resize ((size_t) n);
    auto mainIn = getBusBuffer (buf, true, 0);
    const float* inA = mainIn.getReadPointer (0);
    const float* inB = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : inA;
    std::copy (inA, inA + n, dryTmp[0].begin());
    std::copy (inB, inB + n, dryTmp[1].begin());
    Params q = engine.currentParams(); q.mode = Mode::Off; q.inTrimDb = 0; q.outTrimDb = 0;
    engine.setParams (q, true);
    auto mainOut = getBusBuffer (buf, false, 0);
    engine.process (dryTmp[0].data(), dryTmp[1].data(), mainOut.getWritePointer (0), mainOut.getWritePointer (1), n);
}

void SbfProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SbfProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SbfProcessor::createEditor() { return new SbfEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SbfProcessor(); }
