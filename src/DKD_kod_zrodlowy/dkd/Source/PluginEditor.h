#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Fission.h"
#include "Digi.h"
#include "Views.h"
#include "Hw.h"

// ======================================================================= the editor
// A two-screen music-production device. Top screen: preset bar, one of four views (BEAT timing, SCOPE wave / spectrum /
// pitch / meters, MOD xy / LFOs / sequencer / macros, LAB kick vs bass), status + meter. Bottom screen: the rows of one
// page (BEAT KICK BASS OUT LFO SEQ MAC OWN). The housing is a second control surface: view keys under the top screen,
// status lamps and a level ladder beside it, four knobs, a data wheel, four pads, two faders, five lever switches,
// a DIP bank, a morph crossfader. The screens are Digi.h / Views.h, the parts are Hw.h.
class DsButton : public juce::Component
{
public:
    enum class Kind { pad, pill, tab, key };
    DsButton (Kind k, juce::String face, juce::String tip) : kind (k), face (std::move (face)), tip (std::move (tip))
    { setMouseCursor (juce::MouseCursor::PointingHandCursor); setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick, onDown, onUp;
    std::function<juce::String()> caption;   // pad: what it is set to right now (live, automation included)
    std::function<bool()> lit;
    bool playIcon = false;                    // pad: a play triangle instead of a value
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); if (onDown) onDown(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false; repaint();
        if (onUp) onUp();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { if (onHover) onHover (tip); }
    void mouseExit (const juce::MouseEvent&) override { if (onHover) onHover ({}); }
    std::function<void (const juce::String&)> onHover;
    Kind kind;
    juce::String face, tip;
    bool isDown() const { return down; }
private:
    bool down = false;
};

// the data wheel: turns the parameter touched last (drag up / down, scroll; Shift = fine), a ring of lamps shows its value
class DsWheel : public juce::Component, private juce::Timer
{
public:
    explicit DsWheel (Fiss::Session& s) : session (s) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); startTimerHz (15); }
    ~DsWheel() override { if (gesture != nullptr) gesture->endChangeGesture(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    void turn (float amount, bool fine);   // amount: wheel clicks (discrete) / fraction of the range
    Fiss::Session& session;
    juce::RangedAudioParameter* gesture = nullptr;
    float lastY = 0.0f, acc = 0.0f;
};

class DsBody : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 840, H = 930;
    DsBody (Fiss::Session&, RutherfordProcessor&);
    ~DsBody() override;   // unhooks the session callbacks (crash-hardening: they capture this component)
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void showPage (int);
    void showView (int);
    Digi::Analyzer analyzer;
    Digi::Header top;
    Digi::Footer footer;
    Digi::PlotView plot;
    Digi::ScopeView scope;
    Digi::ModView mod;
    Digi::LabView lab;
    DsWheel wheel;
    std::vector<std::unique_ptr<Digi::Page>> pages;   // one per bottom-screen tab
    juce::StringArray pageNames;
    int page = 0, view = 0;

private:
    using Snap = std::vector<std::pair<juce::RangedAudioParameter*, float>>;
    void timerCallback() override { for (auto* b : buttons) b->repaint(); }
    void cycle (const char* id, int delta);
    void stepPreset (int delta);
    Snap capture() const;
    static juce::String toText (const Snap&);
    Snap fromText (const juce::String&) const;
    void store (bool toA);
    void morphTo (float t);
    juce::Rectangle<float> topLcd() const    { return { 140.0f, 64.0f, 560.0f, 330.0f }; }
    juce::Rectangle<float> bottomLcd() const { return { 190.0f, 534.0f, 460.0f, 330.0f }; }
    Fiss::Session& session;
    RutherfordProcessor& proc;
    std::vector<std::unique_ptr<DsButton>> owned;
    std::vector<DsButton*> buttons;
    DsButton* add (DsButton::Kind, const juce::String&, const juce::String&, std::function<void()>);
    template <class T, class... Args> T* addHw (Args&&... a)
    {
        auto c = std::make_unique<T> (std::forward<Args> (a)...);
        auto* raw = c.get();
        addAndMakeVisible (*c);
        hw.push_back (std::move (c));
        return raw;
    }
    std::vector<std::unique_ptr<juce::Component>> hw;   // knobs, faders, switches, lamps...
    DsButton *padTest {}, *padPattern {}, *padKick {}, *padBass {}, *randomKey {}, *undoKey {}, *prevKey {}, *nextKey {};
    std::array<DsButton*, 4> viewKeys {};
    std::array<Hw::Knob*, 4> knobs {};
    std::array<Hw::Fader*, 2> faders {};
    std::array<Hw::Switch*, 5> switches {};
    Hw::DipBank* dip {};
    Hw::StatusLeds* leds {};
    Hw::LevelLadder* ladder {};
    Hw::MorphSlider* morphSlider {};
    std::vector<DsButton*> tabs;
    int presetIndex = 0, lastKicks = 0;
    double kickAt = 0.0;
    Snap snapA, snapB, morphHeld;
    juce::Image glassTop, glassBottom;   // scan lines + reflection of the two screens, built on first paint
};

class DsLiteEditor : public juce::AudioProcessorEditor
{
public:
    explicit DsLiteEditor (RutherfordProcessor&);
    ~DsLiteEditor() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff050506)); }
    void resized() override;
    void prepareScreenshot();   // render test: one fixed animation moment
    void showPage (int i) { body.showPage (i); }   // render test: every touch-screen page
    void showView (int i) { body.showView (i); }   // render test: every view of the top screen
    void updateAnalyzer() { body.analyzer.update(); }   // render test: spectra and levels without the timer

private:
    template <class T> T* keep (std::unique_ptr<T> c) { auto* raw = c.get(); parts.push_back (std::move (c)); return raw; }

    RutherfordProcessor& proc;
    Fiss::LookAndFeel laf;
    Fiss::Session session;
    std::vector<std::unique_ptr<juce::Component>> parts;   // the rows: destroyed after body (declared before it)
    DsBody body;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsLiteEditor)
};
