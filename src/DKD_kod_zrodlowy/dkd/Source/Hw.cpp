#include "Hw.h"

using namespace juce;

namespace Hw
{
namespace
{
    Font mono (float px, bool bold = false) { return Fiss::mono (px, bold ? Fiss::Weight::semibold : Fiss::Weight::regular); }
}

// ======================================================================= knob
void Knob::paint (Graphics& g)
{
    const Point<float> c (38.0f, 30.0f);
    const float R = 28.0f, pi = MathConstants<float>::pi, a0 = pi * 0.75f, sweep = pi * 1.5f;
    constexpr int N = 25;
    const int lit = roundToInt (value * (float) (N - 1));
    for (int i = 0; i < N; ++i)
    {
        const float a = a0 + sweep * (float) i / (float) (N - 1);
        const Point<float> d (std::cos (a), std::sin (a));
        g.setColour (i <= lit ? Col::amber : Col::steelLo);
        g.drawLine ({ c + d * (R - 6.0f), c + d * R }, 2.2f);
    }
    if (effNorm)   // where modulation has moved it to
    {
        const float e = effNorm();
        if (e >= 0.0f && std::abs (e - value) > 0.004f)
        {
            const float a = a0 + sweep * jlimit (0.0f, 1.0f, e);
            const Point<float> d (std::cos (a), std::sin (a));
            g.setColour (Col::bright);
            g.drawLine ({ c + d * (R - 9.0f), c + d * (R + 2.0f) }, 2.8f);
        }
    }
    g.setColour (Col::keyFill);
    g.fillEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f);
    g.setColour (Digi::isSelected (param.paramID) ? Col::amber : Col::steelMid);
    g.drawEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f, 1.2f);
    const float a = a0 + sweep * value;
    const Point<float> d (std::cos (a), std::sin (a));
    g.setColour (Col::bright);
    g.drawLine ({ c + d * 6.0f, c + d * 17.0f }, 2.4f);
    g.setColour (Col::print);
    g.setFont (mono (9.5f, true));
    g.drawText (label.toUpperCase(), Rectangle<float> (0.0f, 62.0f, 76.0f, 12.0f), Justification::centred, false);
    g.setColour (Col::amber);
    g.setFont (mono (10.5f, true));
    g.drawText (Digi::fit (Digi::shortValue (valueText()), 9), Rectangle<float> (0.0f, 76.0f, 76.0f, 14.0f), Justification::centred, false);
    if (session.isLocked (param.paramID)) { g.setColour (Col::red); g.fillRect (68.0f, 2.0f, 5.0f, 5.0f); }
}

void Knob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { Fiss::Control::mouseDown (e); return; }
    Digi::select (param.paramID);
    gesture = true;
    param.beginChangeGesture();
    lastY = e.position.y;
}

void Knob::mouseDrag (const MouseEvent& e)
{
    if (! gesture) return;
    const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
    param.setValueNotifyingHost (jlimit (0.0f, 1.0f, value + (lastY - e.position.y) / (fine ? 800.0f : 160.0f)));
    lastY = e.position.y;
}

void Knob::mouseUp (const MouseEvent&)
{
    if (! gesture) return;
    gesture = false;
    param.endChangeGesture();
}

// ======================================================================= fader
void Fader::paint (Graphics& g)
{
    const float cx = 28.0f, capY = top + (1.0f - value) * travel;
    g.setColour (Col::well);
    g.fillRoundedRectangle (cx - 3.0f, top, 6.0f, travel + 14.0f, 3.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float y = top + 7.0f + (float) i * travel / 10.0f;
        g.setColour (i % 5 == 0 ? Col::steelMid : Col::steelLo);
        const float len = i % 5 == 0 ? 7.0f : 4.0f;
        g.fillRect (cx - 12.0f - len + 4.0f, y, len, 1.0f);
        g.fillRect (cx + 8.0f, y, len, 1.0f);
    }
    g.setColour (Col::amber);
    g.fillRect (cx - 1.5f, capY + 7.0f, 3.0f, top + travel + 7.0f - (capY + 7.0f));
    const auto cap = Rectangle<float> (cx - 14.0f, capY, 28.0f, 14.0f);
    g.setColour (Col::keyFill);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (Digi::isSelected (param.paramID) ? Col::amber : Col::steelMid);
    g.drawRoundedRectangle (cap, 3.0f, 1.2f);
    g.setColour (Col::bright);
    g.fillRect (cap.getX() + 4.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 8.0f, 2.0f);
    g.setColour (Col::print);
    g.setFont (mono (9.5f, true));
    g.drawText (label.toUpperCase(), Rectangle<float> (0.0f, 94.0f, 56.0f, 12.0f), Justification::centred, false);
    g.setColour (Col::amber);
    g.setFont (mono (10.0f, true));
    g.drawText (Digi::shortValue (valueText()), Rectangle<float> (0.0f, 105.0f, 56.0f, 13.0f), Justification::centred, false);
    if (session.isLocked (param.paramID)) { g.setColour (Col::red); g.fillRect (48.0f, 2.0f, 5.0f, 5.0f); }
}

void Fader::setFromY (float y) { param.setValueNotifyingHost (jlimit (0.0f, 1.0f, 1.0f - (y - top - 7.0f) / travel)); }

void Fader::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { Fiss::Control::mouseDown (e); return; }
    Digi::select (param.paramID);
    gesture = true;
    param.beginChangeGesture();
    setFromY (e.position.y);
}

void Fader::mouseDrag (const MouseEvent& e) { if (gesture) setFromY (e.position.y); }

void Fader::mouseUp (const MouseEvent&)
{
    if (! gesture) return;
    gesture = false;
    param.endChangeGesture();
}

// ======================================================================= lever switch
void Switch::paint (Graphics& g)
{
    const int n = names.size(), idx = jlimit (0, n - 1, roundToInt (value * (float) steps()));
    g.setColour (Col::well);
    g.fillRoundedRectangle (9.0f, 4.0f, 20.0f, 72.0f, 6.0f);
    g.setColour (Col::keyRim);
    g.drawRoundedRectangle (9.0f, 4.0f, 20.0f, 72.0f, 6.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i == idx ? Col::amber : Col::steelLo);
        g.fillEllipse (17.0f, posY (i) - 2.0f, 4.0f, 4.0f);
    }
    const auto lever = Rectangle<float> (13.0f, posY (idx) - 9.0f, 12.0f, 18.0f);
    g.setColour (Col::keyFill);
    g.fillRoundedRectangle (lever, 3.0f);
    g.setColour (Col::bright);
    g.drawRoundedRectangle (lever, 3.0f, 1.2f);
    g.fillRect (lever.getX() + 3.0f, lever.getCentreY() - 1.0f, 6.0f, 2.0f);
    g.setColour (Col::print);
    g.setFont (mono (9.5f, true));
    g.drawText (label.toUpperCase(), Rectangle<float> (0.0f, 80.0f, 38.0f, 12.0f), Justification::centred, false);
    g.setColour (Col::amber);
    g.setFont (mono (9.0f, true));
    g.drawText (names[idx].toUpperCase(), Rectangle<float> (-4.0f, 93.0f, 46.0f, 12.0f), Justification::centred, false);
}

void Switch::pick (float y)
{
    int best = 0;
    for (int i = 1; i < names.size(); ++i)
        if (std::abs (posY (i) - y) < std::abs (posY (best) - y)) best = i;
    setNorm ((float) best / (float) steps());
}

void Switch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { Fiss::Control::mouseDown (e); return; }
    Digi::select (param.paramID);
    pick (e.position.y);
}

void Switch::mouseDrag (const MouseEvent& e) { if (! e.mods.isPopupMenu()) pick (e.position.y); }

// ======================================================================= DIP bank
DipBank::DipBank (Fiss::Session& s, std::vector<Entry> e) : session (s), entries (std::move (e)) { startTimerHz (10); }

int DipBank::indexAt (Point<float> p) const
{
    if (p.y < 16.0f || p.y > 70.0f) return -1;
    const int i = (int) std::floor ((p.x - 10.0f) / 18.0f);
    return i >= 0 && i < (int) entries.size() ? i : -1;
}

void DipBank::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Col::well);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (Col::keyRim);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const float x = 10.0f + (float) i * 18.0f;
        const bool on = entries[(size_t) i].get && entries[(size_t) i].get();
        g.setColour (on ? Col::amber : Col::print);
        g.setFont (mono (8.5f, true));
        g.drawText (String (i + 1), Rectangle<float> (x - 2.0f, 4.0f, 18.0f, 11.0f), Justification::centred, false);
        const auto slot = Rectangle<float> (x, 18.0f, 14.0f, 44.0f);
        g.setColour (Colour (0xff0a0b0d));
        g.fillRoundedRectangle (slot, 3.0f);
        g.setColour (on ? Col::amber.withAlpha (0.55f) : Col::steelLo);
        g.drawRoundedRectangle (slot, 3.0f, 1.0f);
        const auto block = Rectangle<float> (x + 1.5f, on ? 19.5f : 41.5f, 11.0f, 19.0f);
        g.setColour (on ? Col::bright : Col::steelMid);
        g.fillRoundedRectangle (block, 2.0f);
        g.setColour (Col::steelDk);
        g.fillRect (block.getX() + 2.0f, block.getCentreY() - 0.5f, 7.0f, 1.0f);
        g.setColour (on ? Col::amber : Col::print);
        g.setFont (mono (8.5f, true));
        g.drawText (entries[(size_t) i].code, Rectangle<float> (x - 2.0f, 66.0f, 18.0f, 11.0f), Justification::centred, false);
    }
}

void DipBank::mouseDown (const MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i < 0 || ! entries[(size_t) i].set) return;
    entries[(size_t) i].set (! (entries[(size_t) i].get && entries[(size_t) i].get()));
    session.status (entries[(size_t) i].name, entries[(size_t) i].get() ? "on" : "off");
    repaint();
}

void DipBank::mouseMove (const MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i < 0) { session.hover (nullptr); return; }
    session.status (entries[(size_t) i].name, entries[(size_t) i].get && entries[(size_t) i].get() ? "on" : "off");
}

// ======================================================================= lamps
void StatusLeds::paint (Graphics& g)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const float y = 6.0f + (float) i * 24.0f, lv = jlimit (0.0f, 1.0f, rows[i].level ? rows[i].level() : 0.0f);
        const Point<float> c (11.0f, y + 9.0f);
        if (lv > 0.4f)
        {
            g.setGradientFill (ColourGradient (rows[i].colour.withAlpha (0.4f * lv), c.x, c.y, rows[i].colour.withAlpha (0.0f), c.x + 10.0f, c.y, true));
            g.fillEllipse (c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f);
        }
        g.setColour (rows[i].colour.withAlpha (0.16f + 0.84f * lv));
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
        g.setColour (lv > 0.4f ? Col::bright : Col::print);
        g.setFont (mono (10.0f, true));
        g.drawText (rows[i].label, Rectangle<float> (26.0f, y + 2.0f, 70.0f, 14.0f), Justification::centredLeft, false);
    }
}

void LevelLadder::paint (Graphics& g)
{
    constexpr int N = 24;
    const int lit = jlimit (0, N, roundToInt ((an.levelDb + 48.0f) / 48.0f * (float) N));
    const int hold = jlimit (0, N, roundToInt ((an.holdDb + 48.0f) / 48.0f * (float) N));
    for (int i = 0; i < N; ++i)
    {
        const bool on = i < lit || i == hold - 1;
        const Colour c = i >= 21 ? Col::red : i >= 18 ? Col::amber : Col::green;
        g.setColour (on ? c : c.withAlpha (0.14f));
        g.fillRoundedRectangle (4.0f, (float) (getHeight() - 12 - i * 9), 24.0f, 7.0f, 1.5f);
    }
    g.setColour (Col::print);
    g.setFont (mono (8.5f, true));
    for (auto [db, name] : { std::pair<float, const char*> { 0.0f, "0" }, { -12.0f, "-12" }, { -24.0f, "-24" }, { -36.0f, "-36" } })
    {
        const int seg = roundToInt ((db + 48.0f) / 48.0f * (float) N) - 1;
        g.drawText (name, Rectangle<float> (30.0f, (float) (getHeight() - 14 - seg * 9), 20.0f, 10.0f), Justification::centredLeft, false);
    }
}

// ======================================================================= morph
void MorphSlider::paint (Graphics& g)
{
    const int cy = getHeight() / 2, x0 = 48, w = getWidth() - 96;
    g.setColour (Col::well);
    g.fillRoundedRectangle ((float) x0, (float) cy - 3.0f, (float) w, 6.0f, 3.0f);
    for (int i = 0; i <= 10; ++i)
    {
        g.setColour (i == 5 ? Col::steelMid : Col::steelLo);
        g.fillRect (x0 + roundToInt ((float) w * (float) i / 10.0f), cy - (i % 5 == 0 ? 9 : 6), 1, i % 5 == 0 ? 6 : 4);
    }
    const auto cap = Rectangle<float> ((float) x0 + an.morph * (float) w - 7.0f, (float) cy - 10.0f, 14.0f, 20.0f);
    g.setColour (Col::keyFill);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (dragging ? Col::amber : Col::steelMid);
    g.drawRoundedRectangle (cap, 3.0f, 1.2f);
    g.setColour (Col::bright);
    g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 4.0f, 2.0f, cap.getHeight() - 8.0f);
    for (int k = 0; k < 2; ++k)
    {
        const auto r = k == 0 ? aBox() : bBox();
        const bool lamp = k == 0 ? an.hasA : an.hasB, over = r.contains (getMouseXYRelative());
        const auto c = r.toFloat().getCentre();
        g.setColour (Col::keyFill);
        g.fillEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f);
        g.setColour (over ? Col::bright : Col::keyRim);
        g.drawEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f, 1.2f);
        g.setColour (Col::print);
        g.setFont (mono (11.0f, true));
        g.drawText (k == 0 ? "A" : "B", Rectangle<float> (c.x - 12.0f, c.y - 8.0f, 24.0f, 16.0f), Justification::centred, false);
        g.setColour (lamp ? Col::amber : Col::amber.withAlpha (0.2f));
        g.fillEllipse (c.x + 8.0f, c.y - 13.0f, 5.0f, 5.0f);
    }
}

void MorphSlider::mouseDown (const MouseEvent& e)
{
    if (aBox().contains (e.getPosition())) { if (onStoreA) onStoreA(); repaint(); return; }
    if (bBox().contains (e.getPosition())) { if (onStoreB) onStoreB(); repaint(); return; }
    dragging = true;
    if (onBegin) onBegin();
    if (onMorph) onMorph (fromX (e.position.x));
    repaint();
}

void MorphSlider::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    if (onMorph) onMorph (fromX (e.position.x));
    repaint();
}

void MorphSlider::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    if (onEnd) onEnd();
    repaint();
}
}
