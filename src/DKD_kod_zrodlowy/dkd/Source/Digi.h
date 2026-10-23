#pragma once
// dkd screens: "early digital" - a 5x7 pixel font, phosphor green on near-black, segmented bars and blocky plots.
// Replaces the FISSION look inside the two LCDs. Fiss::Session (RANDOM, undo, freeze, automation menu, status line)
// and Fiss::Control (parameter attachment, right-click menu, wheel, hover status) are still used underneath.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Fission.h"
#include "PluginProcessor.h"

namespace Digi
{
namespace Col
{
    const juce::Colour bg   { 0xff03100a };
    const juce::Colour grid { 0xff0a2415 };   // unlit segments
    const juce::Colour dim  { 0xff1f6e40 };
    const juce::Colour mid  { 0xff4ccf80 };
    const juce::Colour hi   { 0xff8bffb4 };
    const juce::Colour warn { 0xffffb347 };
}

// ---- pixel font (upper case only; every text is drawn at an integer scale, 2 = 10 x 14 px glyphs)
constexpr int advance = 6, lineH = 7;
int textWidth (const juce::String&, int scale);
void text (juce::Graphics&, const juce::String&, int x, int y, int scale, juce::Colour);
void textRight (juce::Graphics&, const juce::String&, int right, int y, int scale, juce::Colour);
void textCentre (juce::Graphics&, const juce::String&, juce::Rectangle<int>, int scale, juce::Colour);
juce::String fit (const juce::String&, int maxChars);    // cut with ".." when longer
juce::String shortValue (juce::String);                  // "30 % of beat" -> "30 %"

extern bool frozen;   // render test: no blinking

// the parameter the data wheel of the console edits: the one touched last (a row, a handle of the plot)
extern juce::String selectedId;
extern std::function<void()> onSelect;
void select (const juce::String& paramId);
inline bool isSelected (const juce::String& id) { return selectedId == id; }

// ---- top screen
class Header : public juce::Component
{
public:
    Header (Fiss::Session&, const juce::String& pluginName);
    juce::StringArray factoryNames;
    std::function<void (int)> applyFactory;
    std::function<juce::String()> currentName;
    std::function<void (const juce::String&)> onUserPresetLoaded;
    void refresh() { repaint(); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Zone { none, prev, name, next, mine, undo };
    Zone zoneAt (juce::Point<float>) const;
    juce::Rectangle<int> zoneRect (Zone) const;
    void stepPreset (int delta);
    void showFactoryMenu();
    void showMyPresets();
    void askNameAndSave();
    Fiss::Session& session;
    juce::String pluginName;
    juce::Array<juce::File> userFiles;
    Zone over = Zone::none;
};

class Footer : public juce::Component, private juce::Timer
{
public:
    Footer();
    std::function<float()> level;   // output peak 0..1
    void setStatus (const juce::String& bold, const juce::String& rest) { boldText = bold; restText = rest; repaint(); }
    void setStatic (float db) { staticDb = db; }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String boldText, restText;
    float shown = -60.0f, staticDb = 1.0f;
};

// the beat as a timing diagram: one lane for the kick, one for the reverse bass, drag the four square handles
class PlotView : public juce::Component, private juce::Timer
{
public:
    PlotView (Fiss::Session&, RutherfordProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override { offImage = {}; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    float real (const char* id) const;
    void setReal (const char* id, float v);
    juce::Rectangle<float> laneK() const, laneB() const;
    float xAt (float frac) const;
    float fracAt (float x) const;
    std::vector<juce::Point<float>> handles() const;   // tail, bass in, bass out, rise
    int handleAt (juce::Point<float>) const;
    Fiss::Session& session;
    RutherfordProcessor& proc;
    juce::Image offImage;   // the unlit segments, drawn once per size
    int dragging = -1, hover = -1;
    juce::RangedAudioParameter* held = nullptr;
};

// ---- bottom screen
// a parameter as a row: LABEL  [segmented bar]  VALUE. Click or drag on the row (Shift = fine), double-click = default.
class SliderRow : public Fiss::Control
{
public:
    SliderRow (Fiss::Session& s, const char* id, const char* l) : Fiss::Control (s, id, l) {}
    std::function<float()> effNorm;   // the value with modulation applied, normalised: drawn as an amber marker on the bar
    ~SliderRow() override { if (gesture) param.endChangeGesture(); }
    juce::Point<float> sizeAt (float) const override { return { 460.0f, 34.0f }; }
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
    static constexpr int barL = 154;
    int barW() const { return ((getWidth() - barL - 112) / 8) * 8; }   // whole segments, room for the value on the right
    float fromX (float x) const { return juce::jlimit (0.0f, 1.0f, (x - (float) barL) / (float) barW()); }
    void apply (float n);
    bool gesture = false, fineMode = false;
    float baseV = 0.0f, baseX = 0.0f;
};

// a choice as a row of cells: LABEL  [A][B][C], the chosen one in inverse video
class ChoiceRow : public Fiss::Control
{
public:
    ChoiceRow (Fiss::Session& s, const char* id, const char* l, juce::StringArray names) : Fiss::Control (s, id, l), names (std::move (names)) {}
    juce::Point<float> sizeAt (float) const override { return { 460.0f, 34.0f }; }
    void paint (juce::Graphics&) override;
protected:
    void drawGraphic (juce::Graphics&, juce::Rectangle<float>) override {}
    float graphicHeight() const override { return 0.0f; }
    bool dragsVertically() const override { return false; }
    void clickAt (juce::Point<float>) override;
private:
    juce::Rectangle<int> cell (int i) const;
    juce::StringArray names;
};

// OWN SOUND: the user's own sample (click = load, drop a file on it, right-click = remove)
class OwnBox : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    OwnBox (Fiss::Session&, Sonic::OwnSound&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
private:
    void timerCallback() override;
    void choose();
    void load (const juce::File&);
    void refresh();
    void hint();
    Fiss::Session& session;
    Sonic::OwnSound& own;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> peaks;
    juce::String shownName;
    int lastHits = 0;
    double hitAt = -1.0e9;
    bool hovered = false, dragOver = false;
};

// the rows of one page, stacked from the top
class Page : public juce::Component
{
public:
    void add (juce::Component* c, int height) { items.push_back ({ c, height }); addAndMakeVisible (c); }
    void resized() override
    {
        int y = 4;
        for (auto& [c, h] : items) { c->setBounds (0, y, getWidth(), h); y += h; }
    }
private:
    std::vector<std::pair<juce::Component*, int>> items;
};
}
