#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Fission.h"
#include "Digi.h"

// ======================================================================= the editor
// A two-screen music-production device: top LCD = preset bar, the timing diagram of the beat,
// status + meter; bottom LCD = the rows of one page. Hardware: a data wheel (edits the parameter touched last),
// four pads (TEST, PATTERN, KICK, BASS) and four keys (RANDOM, UNDO, PRESET -, PRESET +). The screens are Digi.h.
class DsButton : public juce::Component
{
public:
    enum class Kind { pad, pill, tab };
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
    Digi::Header top;
    Digi::Footer footer;
    Digi::PlotView plot;
    DsWheel wheel;
    std::vector<std::unique_ptr<Digi::Page>> pages;   // one per bottom-screen tab
    juce::StringArray pageNames;
    int page = 0;

private:
    void timerCallback() override { for (auto* b : buttons) b->repaint(); }
    void cycle (const char* id, int delta);
    void stepPreset (int delta);
    juce::Rectangle<float> topLcd() const    { return { 140.0f, 64.0f, 560.0f, 330.0f }; }
    juce::Rectangle<float> bottomLcd() const { return { 190.0f, 534.0f, 460.0f, 330.0f }; }
    Fiss::Session& session;
    RutherfordProcessor& proc;
    std::vector<std::unique_ptr<DsButton>> owned;
    std::vector<DsButton*> buttons;
    DsButton* add (DsButton::Kind, const juce::String&, const juce::String&, std::function<void()>);
    DsButton *padTest {}, *padPattern {}, *padKick {}, *padBass {}, *randomKey {}, *undoKey {}, *prevKey {}, *nextKey {};
    std::vector<DsButton*> tabs;
    int presetIndex = 0;
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

private:
    template <class T> T* keep (std::unique_ptr<T> c) { auto* raw = c.get(); parts.push_back (std::move (c)); return raw; }

    RutherfordProcessor& proc;
    Fiss::LookAndFeel laf;
    Fiss::Session session;
    std::vector<std::unique_ptr<juce::Component>> parts;   // the rows: destroyed after body (declared before it)
    DsBody body;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsLiteEditor)
};
