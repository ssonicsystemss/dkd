#include "Params.h"
#include "OwnSound.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;

namespace
{
    juce::ParameterID id (const char* s) { return juce::ParameterID { s, 1 }; }

    std::unique_ptr<APF> msParam (const char* pid, const char* name, float lo, float hi, float centre, float def)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return std::make_unique<APF> (id (pid), name, r, def,
            juce::AudioParameterFloatAttributes().withLabel ("ms")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
    }

    std::unique_ptr<APF> hzParam (const char* pid, const char* name, float lo, float hi, float centre, float def)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return std::make_unique<APF> (id (pid), name, r, def,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int)
                { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; })
                .withValueFromStringFunction ([] (const juce::String& t)
                { return t.containsIgnoreCase ("k") ? t.getFloatValue() * 1000.0f : t.getFloatValue(); }));
    }

    // 0..1 shown as a percentage, with what it is a percentage of
    std::unique_ptr<APF> pct (const char* pid, const char* name, float lo, float hi, float def, const char* of = "")
    {
        const juce::String suffix (of);
        return std::make_unique<APF> (id (pid), name, juce::NormalisableRange<float> (lo, hi), def,
            juce::AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([suffix] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %" + suffix; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; }));
    }

    std::unique_ptr<APF> db (const char* pid, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<APF> (id (pid), name, juce::NormalisableRange<float> (lo, hi, 0.1f), def,
            juce::AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
    }

    std::unique_ptr<APF> plain (const char* pid, const char* name, float lo, float hi, float def, int decimals, const char* unit)
    {
        const juce::String u (unit);
        return std::make_unique<APF> (id (pid), name, juce::NormalisableRange<float> (lo, hi), def,
            juce::AudioParameterFloatAttributes().withLabel (u)
                .withStringFromValueFunction ([decimals, u] (float v, int) { return juce::String (v, decimals) + (u.isEmpty() ? "" : " " + u); })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // ---- the beat (defaults = "U-150", a short kick and a clean reverse bass at 150 BPM)
    l.add (std::make_unique<APC> (id (PID::trigger), "Trigger Mode", juce::StringArray { "Host Sync", "MIDI Note" }, 0));
    l.add (std::make_unique<APC> (id (PID::pattern), "Bass Pattern", juce::StringArray { "1/4", "1/8", "1/16" }, 0));
    l.add (pct (PID::tail, "Kick Tail", 0.10f, 0.98f, 0.30f, " of beat"));
    l.add (pct (PID::bassStart, "Bass In", 0.02f, 0.90f, 0.38f, " of step"));
    l.add (pct (PID::bassEnd, "Bass Out", 0.12f, 1.00f, 0.95f, " of step"));
    l.add (plain (PID::rise, "Pitch Rise", 0.0f, 24.0f, 12.0f, 1, "st"));
    l.add (plain (PID::curve, "Swell Curve", 0.8f, 6.0f, 2.6f, 2, ""));
    // ---- kick
    l.add (hzParam (PID::kickStart, "Kick Start Pitch", 300.0f, 6000.0f, 1500.0f, 1800.0f));
    l.add (msParam (PID::kickSweep, "Kick Pitch Drop", 10.0f, 200.0f, 55.0f, 55.0f));
    l.add (db (PID::kickDrive, "Kick Drive", 0.0f, 40.0f, 24.0f));
    l.add (std::make_unique<APC> (id (PID::kickType), "Kick Distortion", juce::StringArray { "Tanh", "Hard Clip", "Foldback" }, 1));
    l.add (pct (PID::click, "Kick Click", 0.0f, 1.0f, 0.35f));
    l.add (pct (PID::kickLevel, "Kick Level", 0.0f, 1.0f, 0.9f));
    // ---- reverse bass
    l.add (db (PID::bassDrive, "Bass Drive", 0.0f, 36.0f, 14.0f));
    l.add (hzParam (PID::bassTone, "Bass Tone", 300.0f, 6000.0f, 1400.0f, 1600.0f));
    l.add (pct (PID::bassLevel, "Bass Level", 0.0f, 1.0f, 0.55f));
    // ---- output
    l.add (plain (PID::fine, "Fine Tune", -100.0f, 100.0f, 0.0f, 0, "ct"));
    l.add (db (PID::output, "Output Gain", -24.0f, 6.0f, -3.0f));
    l.add (std::make_unique<APC> (id (PID::clipMode), "Output Clipper", juce::StringArray { "Off", "Soft", "Hard" }, 1));

    // ---- appended (2026-10-17): OWN SOUND, the user's own sample (ownLevel, ownTune, ownKey, ownMode)
    Sonic::OwnSound::addParameters (l, false);

    // ---- appended (modulation update): LFOs, macros, sequencer, DIP switches. All off / neutral by default.
    const juce::StringArray rates { "4 Bar", "2 Bar", "1 Bar", "1/2", "1/4", "1/8", "1/16" };
    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "Random" };
    const juce::StringArray dests { "Off", "Kick Tail", "Kick Drop", "Kick Drive", "Click", "Bass Tone", "Bass Drive", "Bass Level", "Rise", "Fine Tune" };
    auto depth = [] (const char* pid, const char* name)
    {
        return std::make_unique<APF> (id (pid), name, juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.004f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; }));
    };
    auto onOff = [&] (const char* pid, const char* name, int def)
    {
        l.add (std::make_unique<APC> (id (pid), name, juce::StringArray { "Off", "On" }, def));
    };
    l.add (std::make_unique<APC> (id (PID::lfo1Rate), "LFO 1 Rate", rates, 4));
    l.add (std::make_unique<APC> (id (PID::lfo1Shape), "LFO 1 Shape", shapes, 0));
    l.add (std::make_unique<APC> (id (PID::lfo1Dest), "LFO 1 Target", dests, 0));
    l.add (depth (PID::lfo1Depth, "LFO 1 Depth"));
    l.add (std::make_unique<APC> (id (PID::lfo2Rate), "LFO 2 Rate", rates, 5));
    l.add (std::make_unique<APC> (id (PID::lfo2Shape), "LFO 2 Shape", shapes, 1));
    l.add (std::make_unique<APC> (id (PID::lfo2Dest), "LFO 2 Target", dests, 0));
    l.add (depth (PID::lfo2Depth, "LFO 2 Depth"));
    l.add (pct (PID::macro1, "Macro Punch", 0.0f, 1.0f, 0.0f));
    l.add (pct (PID::macro2, "Macro Weight", 0.0f, 1.0f, 0.0f));
    l.add (pct (PID::macro3, "Macro Raw", 0.0f, 1.0f, 0.0f));
    onOff (PID::lfo1On, "LFO 1 On", 0);
    onOff (PID::lfo2On, "LFO 2 On", 0);
    onOff (PID::seqOn, "Sequencer On", 0);
    onOff (PID::macroOn, "Macros On", 1);
    onOff (PID::clickOn, "Click On", 1);
    onOff (PID::riseOn, "Pitch Rise On", 1);
    onOff (PID::dcOn, "DC Filter On", 1);
    for (int i = 0; i < 16; ++i)
        l.add (pct (PID::seqLevel[i], ("Seq Level " + juce::String (i + 1)).toRawUTF8(), 0.0f, 1.0f, 1.0f));
    for (int i = 0; i < 16; ++i)
        l.add (plain (PID::seqPitch[i], ("Seq Pitch " + juce::String (i + 1)).toRawUTF8(), -12.0f, 12.0f, 0.0f, 0, "st"));
    return l;
}
