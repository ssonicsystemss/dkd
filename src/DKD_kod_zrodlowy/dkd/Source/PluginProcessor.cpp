#include "PluginProcessor.h"
#include "PluginEditor.h"

RutherfordProcessor::RutherfordProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RutherfordState", createParameterLayout())
{
    own.attach (apvts);
    own.replaceClearsAll = false;   // REPLACE swaps only the kick: the reverse bass keeps playing
    engine.own = &own;
    apvts.state.setProperty ("preset", "U-150 Classic", nullptr);   // the parameter defaults = that factory preset
}

void RutherfordProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepared.store (false);
    testOn = false;
    own.prepare (sampleRate);
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParams (readParams());
    setLatencySamples (juce::roundToInt (engine.getLatency()));
    own.latency = getLatencySamples();   // the own sample starts in time with the engine
    prepared.store (true);
}

bool RutherfordProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

RuthParams RutherfordProcessor::readParams() const
{
    auto f = [this] (const char* id) { return params.get (id); };
    RuthParams p;
    p.trigger     = (int) f (PID::trigger);
    p.pattern     = (int) f (PID::pattern);
    p.tail        = f (PID::tail);
    p.bassStart   = f (PID::bassStart);
    p.bassEnd     = f (PID::bassEnd);
    p.riseSt      = f (PID::rise);
    p.curve       = f (PID::curve);
    p.kickStartHz = f (PID::kickStart);
    p.kickSweepMs = f (PID::kickSweep);
    p.kickDriveDb = f (PID::kickDrive);
    p.kickType    = (int) f (PID::kickType);
    p.click       = f (PID::click);
    p.kickLevel   = f (PID::kickLevel);
    p.bassDriveDb = f (PID::bassDrive);
    p.bassToneHz  = f (PID::bassTone);
    p.bassLevel   = f (PID::bassLevel);
    p.fineCents   = f (PID::fine);
    p.outputDb    = f (PID::output);
    p.clipMode    = (int) f (PID::clipMode);
    return p;
}

void RutherfordProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (! prepared.load()) return;   // not prepared (yet / any more): output silence instead of touching the engine
    HostTime host;
    host.bpm = lastBpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) if (std::isfinite (*bpm) && *bpm > 20.0) host.bpm = lastBpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) if (std::isfinite (*ppq)) { host.ppq = *ppq; host.playing = pos->getIsPlaying(); }
        }
    engine.setParams (readParams());
    const bool wantTest = testHeld.load();
    if (wantTest != testOn)   // the TEST key of the editor: a note F (kick + reverse bass) for as long as it is held
    {
        midi.addEvent (wantTest ? juce::MidiMessage::noteOn (1, 53, 0.9f) : juce::MidiMessage::noteOff (1, 53), 0);
        testOn = wantTest;
    }
    engine.kickMuted = own.replacing() || muteKick.load();
    engine.bassMuted = muteBass.load();
    engine.process (buffer, midi, host);
    own.render (buffer);
    scope.push (buffer.getReadPointer (0), buffer.getNumSamples());
}

juce::AudioProcessorEditor* RutherfordProcessor::createEditor() { return new DsLiteEditor (*this); }

void RutherfordProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

// a corrupted / hand-edited state must not inject NaN/inf parameter values: such PARAM entries are dropped
static juce::XmlElement& dropNonFiniteValues (juce::XmlElement& xml)
{
    for (int i = xml.getNumChildElements(); --i >= 0;)
        if (auto* c = xml.getChildElement (i))
            if (c->hasAttribute ("value") && ! std::isfinite (c->getDoubleAttribute ("value")))
                xml.removeChildElement (c, true);
    return xml;
}

void RutherfordProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (dropNonFiniteValues (*xml)));
            own.restore();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RutherfordProcessor(); }
