#pragma once
// dkd: the analyzer and the extra views of the top screen (SCOPE, MOD, LAB) and the SEQ / LFO rows of the bottom
// screen. Same early-digital look as Digi.h. Everything reads the sound through lock-free taps (Analysis.h) and
// atomics of the engine; nothing here touches the audio thread.
#include <juce_dsp/juce_dsp.h>
#include "Digi.h"

namespace Digi
{
// ---- small helpers shared by the views
juce::String noteName (double hz, double* cents = nullptr);   // 43.65 -> "F1" (scientific octave), cents off the nearest note
void drawBox (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);
// a row of lit / unlit segments (horizontal)
void segBar (juce::Graphics&, int x, int y, int n, int lit, int w, int h, int gap, juce::Colour on, juce::Colour off);

// ======================================================================= analyzer
// One per editor, runs on the message thread at 30 Hz: three spectra (output, kick, bass), levels, lamps.
class Analyzer : private juce::Timer
{
public:
    static constexpr int bands = 32, order = 13, N = 1 << order;
    explicit Analyzer (RutherfordProcessor&);
    ~Analyzer() override { stopTimer(); }
    void update() { timerCallback(); }   // one step by hand (the render test has no message loop)

    std::array<float, bands> out {}, kick {}, bass {}, caps {};   // 0..1 (-66 .. 0 dB)
    float peakDb = -90.0f, rmsDb = -90.0f, crestDb = 0.0f, dc = 0.0f;
    float crushK = 0.0f, crushB = 0.0f, subPct = 0.0f;
    float levelDb = -90.0f, holdDb = -90.0f;   // ladder: now / peak hold
    int clipFrames = 0;                         // > 0 while the CLIP lamp is lit
    bool frozen = false;                        // DIP FREEZE: the displays stop
    float morph = 0.5f;                         // the crossfader of the console
    bool hasA = false, hasB = false;

    static float bandEdgeHz (int i) { return 20.0f * std::pow (800.0f, (float) i / (float) bands); }
    static int bandOfHz (float hz) { return juce::jlimit (0, bands - 1, (int) std::floor (std::log (hz / 20.0f) / std::log (800.0f) * (float) bands)); }

private:
    void timerCallback() override;
    void analyse (const float* x, std::array<float, bands>& dst, double sr, float* subShare = nullptr);
    RutherfordProcessor& proc;
    juce::dsp::FFT fft { order };
    std::vector<float> window, work, tmpO, tmpK, tmpB;
};

// ======================================================================= top-screen views
class ViewBase : public juce::Component, private juce::Timer
{
public:
    ViewBase (Fiss::Session& s, RutherfordProcessor& p, Analyzer& a) : session (s), proc (p), an (a) { setOpaque (true); startTimerHz (30); }
protected:
    void timerCallback() override { if (isShowing()) repaint(); }
    Fiss::Session& session;
    RutherfordProcessor& proc;
    Analyzer& an;
};

// the sound itself: waveform of one beat (kick / bass), spectrum, kick pitch, meters + lamps
class ScopeView : public ViewBase
{
public:
    using ViewBase::ViewBase;
    void paint (juce::Graphics&) override;
};

// motion: XY pad (tail / drive), the two LFOs, the bass sequencer, the macros
class ModView : public ViewBase
{
public:
    using ViewBase::ViewBase;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> pad() const { return { 10, 26, 172, 154 }; }
    void setFrom (juce::Point<float>);
    juce::RangedAudioParameter *gx = nullptr, *gy = nullptr;
    std::array<juce::Point<float>, 24> trail {};
    int trailPos = 0;
};

// the relation of kick and bass: both spectra on one axis, tone ruler, overlap map, counters, morph
class LabView : public ViewBase
{
public:
    using ViewBase::ViewBase;
    void paint (juce::Graphics&) override;
};

// ======================================================================= bottom-screen pieces
// a choice with many options: LABEL  < value >
class PickRow : public Fiss::Control
{
public:
    PickRow (Fiss::Session& s, const char* id, const char* l) : Fiss::Control (s, id, l) {}
    juce::Point<float> sizeAt (float) const override { return { 460.0f, 34.0f }; }
    void paint (juce::Graphics&) override;
protected:
    void drawGraphic (juce::Graphics&, juce::Rectangle<float>) override {}
    float graphicHeight() const override { return 0.0f; }
    bool dragsVertically() const override { return false; }
    void clickAt (juce::Point<float>) override;
};

// the 16 steps of the bass sequencer: level (up) and pitch (down, +-12 st) of every swell
class SeqGrid : public juce::Component, private juce::Timer
{
public:
    SeqGrid (Fiss::Session&, RutherfordProcessor&);
    ~SeqGrid() override { endGesture(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { session.hover (nullptr); }
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    juce::Rectangle<int> onBox() const { return { 8, 4, 150, 24 }; }
    juce::Rectangle<int> clearBox() const { return { 166, 4, 110, 24 }; }
    juce::Rectangle<int> randomBox() const { return { 284, 4, 120, 24 }; }
    int columnAt (float x) const { return (int) std::floor ((x - 14.0f) / 27.0f); }
    void apply (juce::Point<float>);
    void beginOn (juce::RangedAudioParameter*);
    void endGesture();
    void setNorm (const char* id, float norm);
    Fiss::Session& session;
    RutherfordProcessor& proc;
    juce::RangedAudioParameter* active = nullptr;
};
}
