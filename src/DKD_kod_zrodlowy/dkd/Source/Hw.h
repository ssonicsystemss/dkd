#pragma once
// dkd console hardware: knobs with a ring of lamps, vertical faders, lever switches, a DIP switch bank, status lamps,
// the level ladder and the morph crossfader. Flat matte parts on the graphite shell, amber lamps. Each one is bound to
// a parameter (or to something the editor does), so the housing is a second control surface.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Digi.h"
#include "Views.h"

namespace Hw
{
namespace Col
{
    const juce::Colour shell { 0xff1a1b1f }, keyFill { 0xff25262b }, keyRim { 0xff464850 }, steelMid { 0xff858892 };
    const juce::Colour steelDk { 0xff17181c }, steelLo { 0xff34363c }, print { 0xffa7a9b1 }, bright { 0xffecebe7 };
    const juce::Colour amber { 0xffffa63d }, green { 0xff59d65e }, red { 0xffe5484d }, well { 0xff101114 };
}

// a rotary control: ring of lamps lit up to the value, a white tick where the modulated value is now
class Knob : public Fiss::Control
{
public:
    Knob (Fiss::Session& s, const char* id, const char* l) : Fiss::Control (s, id, l) {}
    ~Knob() override { if (gesture) param.endChangeGesture(); }
    std::function<float()> effNorm;   // the value with modulation applied, normalised (or nothing)
    juce::Point<float> sizeAt (float) const override { return { 76.0f, 96.0f }; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { setNorm (param.getDefaultValue()); }
protected:
    void drawGraphic (juce::Graphics&, juce::Rectangle<float>) override {}
    float graphicHeight() const override { return 0.0f; }
    bool dragsVertically() const override { return false; }
private:
    bool gesture = false;
    float lastY = 0.0f;
};

// a vertical fader
class Fader : public Fiss::Control
{
public:
    Fader (Fiss::Session& s, const char* id, const char* l) : Fiss::Control (s, id, l) {}
    ~Fader() override { if (gesture) param.endChangeGesture(); }
    juce::Point<float> sizeAt (float) const override { return { 56.0f, 118.0f }; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { setNorm (param.getDefaultValue()); }
protected:
    void drawGraphic (juce::Graphics&, juce::Rectangle<float>) override {}
    float graphicHeight() const override { return 0.0f; }
    bool dragsVertically() const override { return false; }
private:
    static constexpr float top = 8.0f, travel = 76.0f;
    void setFromY (float y);
    bool gesture = false;
};

// a lever switch for a choice of 2 or 3 (first option on top)
class Switch : public Fiss::Control
{
public:
    Switch (Fiss::Session& s, const char* id, const char* l, juce::StringArray n) : Fiss::Control (s, id, l), names (std::move (n)) {}
    juce::Point<float> sizeAt (float) const override { return { 38.0f, 112.0f }; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
protected:
    void drawGraphic (juce::Graphics&, juce::Rectangle<float>) override {}
    float graphicHeight() const override { return 0.0f; }
    bool dragsVertically() const override { return false; }
private:
    float posY (int i) const { return 16.0f + (float) i * (names.size() > 1 ? 44.0f / (float) (names.size() - 1) : 0.0f); }
    void pick (float y);
    juce::StringArray names;
};

// eight little slide switches in one housing; each is a parameter on / off or a flag of the editor
class DipBank : public juce::Component, private juce::Timer
{
public:
    struct Entry { juce::String code, name; std::function<bool()> get; std::function<void (bool)> set; };
    DipBank (Fiss::Session&, std::vector<Entry>);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { session.hover (nullptr); }
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    int indexAt (juce::Point<float>) const;
    Fiss::Session& session;
    std::vector<Entry> entries;
};

// lamps with a name: SYNC, BEAT, KICK, BASS, SEQ, LFO, MUTE, CLIP
class StatusLeds : public juce::Component, private juce::Timer
{
public:
    struct Row { juce::String label; std::function<float()> level; juce::Colour colour; };
    explicit StatusLeds (std::vector<Row> r) : rows (std::move (r)) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    std::vector<Row> rows;
};

// vertical level ladder with peak hold
class LevelLadder : public juce::Component, private juce::Timer
{
public:
    explicit LevelLadder (Digi::Analyzer& a) : an (a) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    Digi::Analyzer& an;
};

// crossfader between two stored sounds: [A] -----|----- [B]; a click on A / B stores the current sound there
class MorphSlider : public juce::Component
{
public:
    explicit MorphSlider (Digi::Analyzer& a) : an (a) {}
    std::function<void()> onStoreA, onStoreB, onBegin, onEnd;
    std::function<void (float)> onMorph;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> aBox() const { return { 0, 0, 34, getHeight() }; }
    juce::Rectangle<int> bBox() const { return { getWidth() - 34, 0, 34, getHeight() }; }
    float fromX (float x) const { return juce::jlimit (0.0f, 1.0f, (x - 48.0f) / (float) (getWidth() - 96)); }
    Digi::Analyzer& an;
    bool dragging = false;
};
}
