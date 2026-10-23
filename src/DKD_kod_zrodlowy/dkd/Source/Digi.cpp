#include "Digi.h"

using namespace juce;

namespace Digi
{
bool frozen = false;
String selectedId = PID::tail;
std::function<void()> onSelect;
void select (const String& id)
{
    if (id == selectedId) return;
    selectedId = id;
    if (onSelect) onSelect();
}
static String dotSep() { return String::fromUTF8 ("  \xc2\xb7  "); }

// ======================================================================= pixel font
namespace
{
    struct Glyph { char c; uint8_t r[7]; };
    const Glyph glyphs[] = {
        { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
        { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
        { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
        { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
        { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
        { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
        { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
        { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
        { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
        { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
        { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
        { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
        { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 } }, { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
        { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } }, { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
        { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
        { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
        { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
        { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
        { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
        { '.', { 0, 0, 0, 0, 0, 0x0C, 0x0C } }, { ',', { 0, 0, 0, 0, 0x0C, 0x04, 0x08 } },
        { ':', { 0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0 } }, { '-', { 0, 0, 0, 0x1F, 0, 0, 0 } },
        { '+', { 0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0 } }, { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
        { '%', { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } }, { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
        { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } }, { '<', { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } },
        { '>', { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } }, { '=', { 0, 0, 0x1F, 0, 0x1F, 0, 0 } },
        { '*', { 0, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0 } }, { '_', { 0, 0, 0, 0, 0, 0, 0x1F } },
        { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04 } }, { '?', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04 } },
        { '\'', { 0x04, 0x04, 0x08, 0, 0, 0, 0 } }, { '#', { 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A } },
        { '[', { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E } }, { ']', { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E } },
        { '\x01', { 0, 0, 0, 0x04, 0, 0, 0 } },   // middle dot
    };

    const uint8_t* glyphOf (juce_wchar c)
    {
        static const std::array<const uint8_t*, 128> lut = []
        {
            std::array<const uint8_t*, 128> a {};
            for (auto& gl : glyphs) a[(size_t) (unsigned char) gl.c] = gl.r;
            return a;
        }();
        if (c == 0xB7) c = 1;
        if (c >= 128) c = '?';
        const auto* r = lut[(size_t) c];
        return r != nullptr ? r : lut[(size_t) '?'];
    }
}

int textWidth (const String& s, int scale) { return s.length() * advance * scale - scale; }

void text (Graphics& g, const String& s, int x, int y, int scale, Colour c)
{
    RectangleList<float> rl;
    const auto up = s.toUpperCase();
    float px = (float) x;
    const float sc = (float) scale;
    for (auto ch : up)
    {
        const auto* r = glyphOf (ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (r[row] & (0x10 >> col))
                    rl.addWithoutMerging (Rectangle<float> (px + (float) col * sc, (float) y + (float) row * sc, sc + 0.35f, sc + 0.35f));
        px += (float) advance * sc;
    }
    g.setColour (c);
    g.fillRectList (rl);
}

void textRight (Graphics& g, const String& s, int right, int y, int scale, Colour c) { text (g, s, right - textWidth (s, scale), y, scale, c); }

void textCentre (Graphics& g, const String& s, Rectangle<int> r, int scale, Colour c)
{
    text (g, s, r.getCentreX() - textWidth (s, scale) / 2, r.getCentreY() - (lineH * scale) / 2, scale, c);
}

String fit (const String& s, int maxChars)
{
    return s.length() <= maxChars ? s : s.substring (0, jmax (0, maxChars - 2)) + "..";
}

String shortValue (String v)
{
    for (auto* cut : { " of beat", " of step" }) v = v.replace (cut, "");
    return v.trim();
}

// ======================================================================= header
Header::Header (Fiss::Session& s, const String& name) : session (s), pluginName (name) {}

Rectangle<int> Header::zoneRect (Zone z) const
{
    const int w = getWidth(), nextX = w - 124 - 10 - 28;
    switch (z)
    {
        case Zone::prev: return { 66, 2, 28, 26 };
        case Zone::name: return { 94, 2, nextX - 94, 26 };
        case Zone::next: return { nextX, 2, 28, 26 };
        case Zone::mine: return { w - 124, 2, 56, 26 };
        case Zone::undo: return { w - 62, 2, 60, 26 };
        case Zone::none: break;
    }
    return {};
}

Header::Zone Header::zoneAt (Point<float> p) const
{
    for (auto z : { Zone::prev, Zone::name, Zone::next, Zone::mine, Zone::undo })
        if (zoneRect (z).toFloat().contains (p)) return z;
    return Zone::none;
}

void Header::paint (Graphics& g)
{
    g.setColour (Col::hi);
    g.fillRect (0, 0, 60, 30);
    textCentre (g, pluginName, { 0, 0, 60, 30 }, 2, Col::bg);

    auto box = [&] (Zone z, const String& s, bool enabled = true)
    {
        const auto r = zoneRect (z);
        const bool on = over == z && enabled;
        g.setColour (on ? Col::hi : (enabled ? Col::dim : Col::grid));
        if (on) g.fillRect (r); else g.drawRect (r, 1);
        textCentre (g, s, r, 2, on ? Col::bg : (enabled ? Col::mid : Col::dim));
    };
    box (Zone::prev, "<");
    box (Zone::next, ">");
    box (Zone::mine, "USER");
    box (Zone::undo, "UNDO", session.canUndo());
    {
        const auto r = zoneRect (Zone::name);
        const bool on = over == Zone::name;
        g.setColour (on ? Col::hi : Col::dim);
        if (on) g.fillRect (r); else g.drawRect (r, 1);
        textCentre (g, fit (currentName ? currentName() : String(), (r.getWidth() - 8) / (advance * 2)), r, 2, on ? Col::bg : Col::hi);
    }
    g.setColour (Col::dim);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void Header::mouseMove (const MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != over) { over = z; setMouseCursor (z == Zone::none ? MouseCursor::NormalCursor : MouseCursor::PointingHandCursor); repaint(); }
    if (z == Zone::prev || z == Zone::next) session.status ("PRESET", z == Zone::prev ? "previous" : "next");
    else if (z == Zone::name) session.status ("PRESET", "factory list");
    else if (z == Zone::mine) session.status ("USER", "my presets");
    else if (z == Zone::undo) session.status ("UNDO", "last random");
    else session.hover (nullptr);
}

void Header::mouseExit (const MouseEvent&)
{
    over = Zone::none;
    session.hover (nullptr);
    repaint();
}

void Header::mouseUp (const MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    switch (zoneAt (e.position))
    {
        case Zone::prev: stepPreset (-1); break;
        case Zone::next: stepPreset (1); break;
        case Zone::name: showFactoryMenu(); break;
        case Zone::mine: showMyPresets(); break;
        case Zone::undo: session.undo(); break;
        case Zone::none: break;
    }
    repaint();
}

void Header::stepPreset (int delta)
{
    if (factoryNames.isEmpty() || ! applyFactory) return;
    const int cur = factoryNames.indexOf (currentName ? currentName() : String());
    const int n = factoryNames.size();
    applyFactory (cur < 0 ? (delta > 0 ? 0 : n - 1) : (cur + delta + n) % n);
    repaint();
}

void Header::showFactoryMenu()
{
    if (factoryNames.isEmpty() || ! applyFactory) return;
    PopupMenu m;
    m.addSectionHeader ("FACTORY PRESETS");
    const auto cur = currentName ? currentName() : String();
    for (int i = 0; i < factoryNames.size(); ++i) m.addItem (i + 1, factoryNames[i], true, factoryNames[i] == cur);
    SafePointer<Header> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (zoneRect (Zone::name))),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->applyFactory (r - 1);
                         safe->repaint();
                     });
}

void Header::showMyPresets()
{
    userFiles = Fiss::UserPresets::list (pluginName);
    PopupMenu m;
    m.addSectionHeader ("MY PRESETS");
    m.addItem (1, "Save current sound...");
    m.addSeparator();
    if (userFiles.isEmpty()) m.addItem (2, "(nothing saved yet)", false);
    for (int i = 0; i < userFiles.size(); ++i) m.addItem (100 + i, userFiles[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addItem (3, "Open presets folder");
    SafePointer<Header> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (zoneRect (Zone::mine))),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         if (r == 1) safe->askNameAndSave();
                         else if (r == 3) { auto f = Fiss::UserPresets::folder (safe->pluginName); f.createDirectory(); f.startAsProcess(); }
                         else if (r >= 100 && r - 100 < safe->userFiles.size())
                         {
                             const auto file = safe->userFiles[r - 100];
                             if (Fiss::UserPresets::loadFrom (safe->session.proc, file, safe->session.state) && safe->onUserPresetLoaded)
                                 safe->onUserPresetLoaded (file.getFileNameWithoutExtension());
                             safe->repaint();
                         }
                     });
}

void Header::askNameAndSave()
{
    String name;   // a free name, so OK alone works even when the host keeps the keyboard
    for (int n = 1; n < 1000; ++n)
        if (! Fiss::UserPresets::folder (pluginName).getChildFile ("My Sound " + String (n) + ".fnpreset").exists())
        { name = "My Sound " + String (n); break; }

    auto* aw = new AlertWindow ("SAVE PRESET", "Name for this sound:", MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", name);
    aw->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<Header> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto n = File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (n.isEmpty()) return;
        const auto file = Fiss::UserPresets::folder (safe->pluginName).getChildFile (n + ".fnpreset");
        if (Fiss::UserPresets::saveTo (safe->session.proc, file) && safe->onUserPresetLoaded) safe->onUserPresetLoaded (n);
    }), true);
}

// ======================================================================= footer
Footer::Footer() { startTimerHz (30); }

void Footer::timerCallback()
{
    if (! isShowing()) return;
    const float lv = staticDb <= 0.0f ? Decibels::decibelsToGain (staticDb) : (level ? level() : 0.0f);
    const float db = Decibels::gainToDecibels (lv, -60.0f);
    shown = db > shown ? db : jmax (-60.0f, shown - 1.5f);
    repaint (getWidth() - 110, 0, 110, getHeight());
}

void Footer::paint (Graphics& g)
{
    g.setColour (Col::dim);
    g.fillRect (0, 0, getWidth(), 1);
    const auto first = restText.upToFirstOccurrenceOf (dotSep(), false, false);
    const int maxChars = (getWidth() - 8 - 110) / (advance * 2);
    const auto bold = fit (boldText, maxChars);
    text (g, bold, 4, 4, 2, Col::hi);
    const int left = maxChars - bold.length() - 1;
    if (left > 3) text (g, fit (first, left), 4 + (bold.length() + 1) * advance * 2, 4, 2, Col::mid);

    const int lit = roundToInt (jlimit (0.0f, 1.0f, (shown + 48.0f) / 48.0f) * 12.0f);
    int x = getWidth() - 4 - 12 * 8 + 2;
    for (int i = 0; i < 12; ++i, x += 8)
    {
        g.setColour (i < lit ? (i >= 10 ? Col::warn : Col::hi) : Col::grid);
        g.fillRect (x, 5, 6, 12);
    }
}

// ======================================================================= plot
PlotView::PlotView (Fiss::Session& s, RutherfordProcessor& p) : session (s), proc (p)
{
    setMouseCursor (MouseCursor::CrosshairCursor);
    setOpaque (true);
    startTimerHz (30);
}

float PlotView::real (const char* id) const
{
    auto* q = session.param (id);
    return q != nullptr ? q->convertFrom0to1 (q->getValue()) : 0.0f;
}

void PlotView::setReal (const char* id, float v)
{
    if (auto* q = session.param (id)) q->setValueNotifyingHost (q->convertTo0to1 (v));
}

Rectangle<float> PlotView::laneK() const { return { 30.0f, 16.0f, (float) getWidth() - 44.0f, 60.0f }; }
Rectangle<float> PlotView::laneB() const { return { 30.0f, 100.0f, (float) getWidth() - 44.0f, 60.0f }; }
float PlotView::xAt (float frac) const { return laneK().getX() + frac * laneK().getWidth(); }
float PlotView::fracAt (float x) const { return (x - laneK().getX()) / laneK().getWidth(); }

std::vector<Point<float>> PlotView::handles() const
{
    const float steps = (float) ruth::stepsPerBeat ((int) real (PID::pattern));
    const auto b = laneB();
    const float be = xAt (real (PID::bassEnd) / steps);
    return { { xAt (real (PID::tail)), laneK().getBottom() }, { xAt (real (PID::bassStart) / steps), b.getBottom() },
             { be, b.getBottom() }, { be, b.getBottom() - 14.0f - real (PID::rise) / 24.0f * (b.getHeight() - 14.0f) } };
}

int PlotView::handleAt (Point<float> q) const
{
    const auto hs = handles();
    int best = -1;
    float bd = 13.0f;
    for (int i = 0; i < (int) hs.size(); ++i) { const float d = hs[(size_t) i].getDistanceFrom (q); if (d < bd) { bd = d; best = i; } }
    return best;
}

static const char* const handleIds[] = { PID::tail, PID::bassStart, PID::bassEnd, PID::rise };
static const char* const handleNames[] = { "TAIL", "IN", "OUT", "RISE" };

void PlotView::mouseDown (const MouseEvent& e)
{
    dragging = handleAt (e.position);
    if (dragging < 0) return;
    select (handleIds[dragging]);
    held = session.param (handleIds[dragging]);
    if (held != nullptr) held->beginChangeGesture();
    mouseDrag (e);
}

void PlotView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const float steps = (float) ruth::stepsPerBeat ((int) real (PID::pattern)), f = fracAt (e.position.x);
    const auto b = laneB();
    switch (dragging)
    {
        case 0: setReal (PID::tail, jlimit (0.10f, 0.98f, f)); break;
        case 1: setReal (PID::bassStart, jlimit (0.02f, real (PID::bassEnd) - 0.1f, f * steps)); break;
        case 2: setReal (PID::bassEnd, jlimit (real (PID::bassStart) + 0.1f, 1.0f, f * steps)); break;
        case 3: setReal (PID::rise, jlimit (0.0f, 24.0f, (b.getBottom() - 14.0f - e.position.y) / (b.getHeight() - 14.0f) * 24.0f)); break;
        default: break;
    }
}

void PlotView::mouseUp (const MouseEvent&)
{
    if (held != nullptr) held->endChangeGesture();
    held = nullptr;
    dragging = -1;
}

void PlotView::mouseMove (const MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h == hover) return;
    hover = h;
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
    if (h >= 0) { if (auto* q = session.param (handleIds[h])) session.hover (q); }
    else session.hover (nullptr);
    repaint();
}

void PlotView::mouseExit (const MouseEvent&)
{
    if (hover >= 0) { hover = -1; session.hover (nullptr); repaint(); }
}

void PlotView::paint (Graphics& g)
{
    g.fillAll (Col::bg);
    const auto K = laneK(), B = laneB();
    const float bpm = jmax (20.0f, proc.engine.guiBpm.load()), phase = proc.engine.guiPhase.load();
    const double beatMs = 60000.0 / bpm;
    const RuthParams prm = proc.readParams();
    const int steps = ruth::stepsPerBeat (prm.pattern);
    const double stepSec = ruth::stepBeats (prm.pattern) * 60.0 / bpm;
    const bool live = phase >= 0.0f;
    const int cols = (int) (K.getWidth() / 4.0f), rows = (int) (K.getHeight() / 4.0f);

    // the unlit segments of both lanes, drawn once per size
    if (! offImage.isValid() || offImage.getWidth() != getWidth() || offImage.getHeight() != getHeight())
    {
        offImage = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
        Graphics og (offImage);
        RectangleList<float> rl;
        for (auto lane : { K, B })
            for (int c = 0; c < cols; ++c)
                for (int r = 0; r < rows; ++r)
                    rl.addWithoutMerging (Rectangle<float> (lane.getX() + (float) c * 4.0f, lane.getBottom() - 4.0f * (float) (r + 1) + 1.0f, 3.0f, 3.0f));
        og.setColour (Col::grid);
        og.fillRectList (rl);
    }
    g.drawImageAt (offImage, 0, 0);

    // lane names in the margin
    text (g, "K", 8, (int) K.getCentreY() - 7, 2, Col::mid);
    text (g, "B", 8, (int) B.getCentreY() - 7, 2, Col::mid);

    // the overlap of tail and first swell, as a hatch
    const float firstStart = prm.bassStart / (float) steps, overlap = prm.tail - firstStart;
    if (overlap > 0.0f)
    {
        g.setColour (Col::warn.withAlpha (0.35f));
        for (float x = xAt (firstStart); x < xAt (prm.tail); x += 4.0f)
            for (auto lane : { K, B }) g.fillRect (x, lane.getY(), 1.0f, lane.getHeight());
    }

    // kick: the amplitude of the tail; bass: the swells. Segments left of the playhead are lit bright.
    RectangleList<float> played, waiting, pitch;
    for (int c = 0; c < cols; ++c)
    {
        const float f = ((float) c + 0.5f) / (float) cols;
        const bool lit = live && f < phase;
        const int kn = roundToInt (ruth::kickEnv (prm, (double) f * beatMs, beatMs) * (float) rows);
        float riseU = 0.0f;
        const double q = (double) f * (double) steps;
        const float amp = ruth::swell (prm, q - std::floor (q), stepSec, riseU);
        const int bn = roundToInt (amp * (float) rows);
        for (int r = 0; r < jmax (kn, bn); ++r)
        {
            if (r < kn)
                (lit ? played : waiting).addWithoutMerging (Rectangle<float> (K.getX() + (float) c * 4.0f, K.getBottom() - 4.0f * (float) (r + 1) + 1.0f, 3.0f, 3.0f));
            if (r < bn)
                (lit ? played : waiting).addWithoutMerging (Rectangle<float> (B.getX() + (float) c * 4.0f, B.getBottom() - 4.0f * (float) (r + 1) + 1.0f, 3.0f, 3.0f));
        }
        if (amp > 0.0f)   // the pitch of the swell: flat, then climbing at its end
        {
            const float py = B.getBottom() - 14.0f - prm.riseSt * riseU / 24.0f * (B.getHeight() - 14.0f);
            pitch.addWithoutMerging (Rectangle<float> (B.getX() + (float) c * 4.0f, py - 1.0f, 3.0f, 3.0f));
        }
    }
    g.setColour (Col::mid);
    g.fillRectList (waiting);
    g.setColour (Col::hi);
    g.fillRectList (played);
    g.setColour (Col::warn);
    g.fillRectList (pitch);

    if (live)
    {
        g.setColour (Col::hi);
        g.fillRect (xAt (phase) - 1.0f, K.getY(), 2.0f, B.getBottom() - K.getY());
    }

    // ruler: a tick every 1/16 beat, the time at 0 / 1/4 / 1/2 / 3/4
    for (int i = 0; i <= 16; ++i)
    {
        const float x = xAt ((float) i / 16.0f);
        g.setColour (i % 4 == 0 ? Col::mid : Col::dim);
        g.fillRect (x, 190.0f, 1.0f, i % 4 == 0 ? 8.0f : 4.0f);
    }
    for (int i = 0; i < 4; ++i)
        text (g, String (roundToInt ((double) i * 0.25 * beatMs)), (int) xAt ((float) i * 0.25f) + 3, 200, 2, Col::mid);
    textRight (g, "MS", getWidth() - 4, 200, 2, Col::mid);

    // handles with their names
    const auto hs = handles();
    for (int i = 0; i < (int) hs.size(); ++i)
    {
        const bool on = hover == i || dragging == i;
        const auto r = Rectangle<float> (11.0f, 11.0f).withCentre (hs[(size_t) i]);
        g.setColour (Col::bg);
        g.fillRect (r);
        g.setColour (on ? Col::warn : Col::hi);
        if (on) g.fillRect (r.reduced (2.0f)); else g.drawRect (r, 1.0f);
        g.drawRect (r, 1.0f);
        const int tw = textWidth (handleNames[i], 2);
        int tx = (int) hs[(size_t) i].x + 9, ty = (int) hs[(size_t) i].y - 7;
        if (i == 0) { tx = (int) hs[0].x - tw / 2; ty = (int) hs[0].y + 8; }
        else if (i == 1) { tx = (int) hs[1].x - tw / 2; ty = (int) hs[1].y + 8; }
        else if (i == 2) { tx = (int) hs[2].x - tw / 2; ty = (int) hs[2].y + 8; }
        tx = jlimit (2, getWidth() - tw - 2, tx);
        g.setColour (Col::bg);
        g.fillRect (tx - 2, ty - 1, tw + 4, 16);
        text (g, handleNames[i], tx, ty, 2, on ? Col::warn : Col::hi);
    }

    // numbers
    const double gap = (1.0 - prm.bassEnd) / steps * beatMs;
    text (g, "TAIL " + String (roundToInt (prm.tail * beatMs)) + " MS  BASS " + String (roundToInt (prm.bassStart / steps * beatMs)) + "-"
                 + String (roundToInt (prm.bassEnd / steps * beatMs)) + " MS", 6, 218, 2, Col::hi);
    text (g, "GAP " + String (roundToInt (gap)) + " MS  RISE +" + String (prm.riseSt, 1) + " ST  BEAT " + String (roundToInt (beatMs)) + " MS", 6, 238, 2, Col::mid);
    if (overlap > 0.0f)
    {
        if (frozen || (Time::getMillisecondCounterHiRes() / 400.0 - std::floor (Time::getMillisecondCounterHiRes() / 400.0)) < 0.7)
            text (g, "!! OVERLAP " + String (roundToInt (overlap * beatMs)) + " MS = MUD", 6, 258, 2, Col::warn);
    }
    else
        text (g, "CLEAN: " + String (roundToInt (-overlap * beatMs)) + " MS BETWEEN TAIL AND BASS", 6, 258, 2, Col::mid);
    g.setColour (Col::dim);
    g.drawRect (getLocalBounds(), 1);
}

// ======================================================================= rows
void SliderRow::paint (Graphics& g)
{
    const bool hov = isMouseOver (true);
    if (hov) { g.setColour (Col::grid); g.fillRect (getLocalBounds()); }
    const int ty = (getHeight() - 14) / 2, n = barW() / 8;
    const bool chosen = selectedId == param.paramID;
    if (chosen) { g.setColour (Col::warn); g.fillRect (0, 4, 3, getHeight() - 8); }
    text (g, fit (label, 12), 6, ty, 2, hov || chosen ? Col::hi : Col::mid);
    const float zn = zeroNorm();
    const int lit = roundToInt (value * (float) n), zi = zn >= 0.0f ? roundToInt (zn * (float) n) : 0;
    for (int i = 0; i < n; ++i)
    {
        const bool on = zn >= 0.0f ? (i >= jmin (lit, zi) && i < jmax (lit, zi)) : i < lit;
        g.setColour (on ? Col::hi : (zn >= 0.0f && i == zi ? Col::dim : Col::grid));
        g.fillRect (Rectangle<float> ((float) (barL + i * 8), (float) ty, 7.0f, 14.0f));
    }
    if (effNorm)
    {
        const float e = effNorm();
        if (e >= 0.0f && std::abs (e - value) > 0.004f)
        {
            const int mx = barL + (int) (jlimit (0.0f, 1.0f, e) * (float) (n * 8 - 1));
            g.setColour (Col::warn);
            g.fillRect (mx - 1, ty - 4, 3, 22);
        }
    }
    textRight (g, fit (shortValue (valueText()), 9), getWidth() - 4, ty, 2, Col::hi);
    if (session.isLocked (param.paramID)) { g.setColour (Col::warn); g.fillRect (getWidth() - 7, 2, 5, 5); }
}

void SliderRow::apply (float n) { param.setValueNotifyingHost (jlimit (0.0f, 1.0f, n)); }

void SliderRow::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { Fiss::Control::mouseDown (e); return; }
    select (param.paramID);
    gesture = true;
    param.beginChangeGesture();
    fineMode = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
    baseV = value;
    baseX = e.position.x;
    if (! fineMode) apply (fromX (e.position.x));
}

void SliderRow::mouseDrag (const MouseEvent& e)
{
    if (! gesture) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
    if (fine != fineMode) { fineMode = fine; baseV = value; baseX = e.position.x; }
    apply (fine ? baseV + (e.position.x - baseX) / ((float) barW() * 5.0f) : fromX (e.position.x));
}

void SliderRow::mouseUp (const MouseEvent&)
{
    if (! gesture) return;
    gesture = false;
    param.endChangeGesture();
}

Rectangle<int> ChoiceRow::cell (int i) const
{
    const int left = 154, w = getWidth() - 4 - left, n = jmax (1, names.size()), cw = w / n;
    return { left + i * cw, (getHeight() - 20) / 2, cw - 4, 20 };
}

void ChoiceRow::paint (Graphics& g)
{
    const bool hov = isMouseOver (true);
    if (hov) { g.setColour (Col::grid); g.fillRect (getLocalBounds()); }
    const bool chosen = selectedId == param.paramID;
    if (chosen) { g.setColour (Col::warn); g.fillRect (0, 4, 3, getHeight() - 8); }
    text (g, fit (label, 12), 6, (getHeight() - 14) / 2, 2, hov || chosen ? Col::hi : Col::mid);
    const int sel = roundToInt (value * (float) steps());
    const auto mouse = getMouseXYRelative().toFloat();
    for (int i = 0; i < names.size(); ++i)
    {
        const auto r = cell (i);
        const bool on = i == sel, under = isMouseOver (true) && r.toFloat().expanded (2.0f).contains (mouse);
        g.setColour (on ? Col::hi : (under ? Col::mid : Col::dim));
        if (on) g.fillRect (r); else g.drawRect (r, 1);
        textCentre (g, names[i], r, 2, on ? Col::bg : (under ? Col::hi : Col::mid));
    }
    if (session.isLocked (param.paramID)) { g.setColour (Col::warn); g.fillRect (getWidth() - 7, 2, 5, 5); }
}

void ChoiceRow::clickAt (Point<float> p)
{
    for (int i = 0; i < names.size(); ++i)
        if (cell (i).toFloat().expanded (2.0f).contains (p)) { select (param.paramID); setNorm ((float) i / (float) steps()); return; }
}

// ======================================================================= own sound
OwnBox::OwnBox (Fiss::Session& s, Sonic::OwnSound& o) : session (s), own (o)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    refresh();
    startTimerHz (15);
}

void OwnBox::refresh()
{
    shownName = own.hasSound() ? own.name() : String();
    peaks = own.peaks (56);
    repaint();
}

void OwnBox::timerCallback()
{
    if ((own.hasSound() ? own.name() : String()) != shownName || own.hasSound() == peaks.empty()) refresh();
    const int h = own.hits.load();
    if (h != lastHits) { lastHits = h; hitAt = Time::getMillisecondCounterHiRes(); }
    if (Time::getMillisecondCounterHiRes() - hitAt < 300.0) repaint();
}

void OwnBox::paint (Graphics& g)
{
    auto b = getLocalBounds().reduced (4, 2);
    const auto box = b.removeFromTop (46);
    const bool flash = Time::getMillisecondCounterHiRes() - hitAt < 300.0;
    g.setColour (Col::bg);
    g.fillRect (box);
    g.setColour (dragOver ? Col::warn : hovered ? Col::mid : Col::dim);
    g.drawRect (box, 1);
    if (! peaks.empty())
    {
        const float x0 = (float) box.getX() + 6.0f, w = (float) box.getWidth() - 12.0f, cy = (float) box.getCentreY(), hh = (float) box.getHeight() * 0.5f - 4.0f;
        g.setColour (flash ? Col::hi : Col::mid);
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float x = x0 + w * (float) i / (float) (peaks.size() - 1);
            const float h = (float) jmax (1, roundToInt (peaks[i] * hh / 4.0f)) * 4.0f;
            g.fillRect (x, cy - h, 3.0f, 2.0f * h);
        }
    }
    else
        textCentre (g, dragOver ? "DROP" : "NO SAMPLE", box, 2, dragOver ? Col::warn : Col::mid);
    text (g, "FILE " + (shownName.isEmpty() ? String ("NONE") : fit (shownName, 28)), b.getX(), box.getBottom() + 6, 2, shownName.isEmpty() ? Col::dim : Col::hi);
}

void OwnBox::hint()
{
    if (own.hasSound())
        session.status ("OWN SOUND", own.name() + dotSep() + String (own.seconds(), 2) + " s");
    else
        session.status ("OWN SOUND", "wav aiff flac mp3 ogg");
}

void OwnBox::mouseEnter (const MouseEvent&) { hovered = true; hint(); repaint(); }
void OwnBox::mouseExit (const MouseEvent&) { hovered = false; session.hover (nullptr); repaint(); }

void OwnBox::mouseUp (const MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    if (! e.mods.isPopupMenu()) { choose(); return; }
    SafePointer<OwnBox> safe (this);
    PopupMenu m;
    m.addSectionHeader ("OWN SOUND");
    m.addItem ("Load a sample...", [safe] { if (safe != nullptr) safe->choose(); });
    m.addItem ("Remove the sample", own.hasSound(), false, [safe]
    {
        if (safe == nullptr) return;
        safe->own.clear();
        safe->refresh();
        safe->session.status ("OWN SOUND", "removed" + dotSep() + "the plugin plays its own sound again");
    });
    m.showMenuAsync (PopupMenu::Options().withMousePosition());
}

void OwnBox::choose()
{
    static File lastFolder = File::getSpecialLocation (File::userMusicDirectory);
    chooser = std::make_unique<FileChooser> ("Load your own sound", lastFolder, Sonic::OwnSound::fileTypes());
    SafePointer<OwnBox> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (safe == nullptr || ! f.existsAsFile()) return;
        lastFolder = f.getParentDirectory();
        safe->load (f);
    });
}

void OwnBox::load (const File& f)
{
    const auto note = own.loadFile (f);
    refresh();
    if (own.hasSound() && own.name() == f.getFileNameWithoutExtension())
        session.status ("OWN SOUND", own.name() + " loaded" + dotSep() + "" + String (own.seconds(), 2) + " s" + (note.isEmpty() ? String() : dotSep() + note));
    else
        session.status ("OWN SOUND", note);
}

bool OwnBox::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void OwnBox::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { load (File (f)); return; }
    repaint();
}
}
