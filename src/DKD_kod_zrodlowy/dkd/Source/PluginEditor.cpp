#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

// ======================================================================= console chrome
// One flat graphite colour for the whole shell, thin steel lines, matte keys. The screens are Digi.
namespace ds
{
    const Colour shell { 0xff1a1b1f }, shellRim { 0xff30323a };
    const Colour steelHi { 0xffc6c9d0 }, steelMid { 0xff858892 }, steelLo { 0xff34363c }, steelDk { 0xff17181c };
    const Colour keyFill { 0xff25262b }, keyDown { 0xff141518 }, keyRim { 0xff464850 }, print { 0xffa7a9b1 }, bright { 0xffecebe7 };
    const Colour ledGreen { 0xff59d65e }, ledAmber { 0xffffa63d };

    // two different corner radii (top / bottom), clockwise from the top edge
    Path roundedPath (Rectangle<float> r, float top, float bottom)
    {
        const float x = r.getX(), y = r.getY(), right = r.getRight(), bot = r.getBottom(), hp = MathConstants<float>::halfPi;
        Path p;
        p.startNewSubPath (x + top, y);
        p.lineTo (right - top, y);
        p.addCentredArc (right - top, y + top, top, top, 0.0f, 0.0f, hp);
        p.lineTo (right, bot - bottom);
        p.addCentredArc (right - bottom, bot - bottom, bottom, bottom, 0.0f, hp, 2.0f * hp);
        p.lineTo (x + bottom, bot);
        p.addCentredArc (x + bottom, bot - bottom, bottom, bottom, 0.0f, 2.0f * hp, 3.0f * hp);
        p.lineTo (x, y + top);
        p.addCentredArc (x + top, y + top, top, top, 0.0f, 3.0f * hp, 4.0f * hp);
        p.closeSubPath();
        return p;
    }

    void body (Graphics& g, Rectangle<float> r, float top, float bottom)
    {
        const auto p = roundedPath (r, top, bottom);
        g.setColour (shell);
        g.fillPath (p);
        g.setColour (shellRim);
        g.strokePath (p, PathStrokeType (1.0f));
    }

    // the hinge: a slim, plain, flat bar
    void hinge (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colour (0xff23252a));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (shellRim);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }

    void ledDot (Graphics& g, Point<float> c, Colour col, bool lit)
    {
        g.setColour (lit ? col : col.withAlpha (0.3f));
        g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
    }

    // a thin black bezel around a recessed LCD
    void lcd (Graphics& g, Rectangle<float> screen)
    {
        auto bz = screen.expanded (10.0f);
        g.setColour (Colour (0xff070709));
        g.fillRoundedRectangle (bz, 8.0f);
        g.setColour (shellRim);
        g.drawRoundedRectangle (bz, 8.0f, 1.0f);
        g.setColour (Colours::black);
        g.fillRect (screen.expanded (2.0f));
        g.setColour (Digi::Col::bg);
        g.fillRect (screen);
    }

    // scan lines, a soft vignette and a faint reflection, painted over the live screen
    Image glassImage (int w, int h)
    {
        Image img (Image::ARGB, w, h, true);
        Graphics g (img);
        Path p;
        p.startNewSubPath (0.0f, 0.0f);
        p.lineTo ((float) w * 0.55f, 0.0f);
        p.lineTo (0.0f, (float) h * 0.34f);
        p.closeSubPath();
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), 0.0f, 0.0f, Colours::transparentWhite, (float) w * 0.35f, (float) h * 0.22f, false));
        g.fillPath (p);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.5f, Colours::black.withAlpha (0.18f), 0.0f, 0.0f, true));
        g.fillRect (0, 0, w, h);
        g.setColour (Colours::black.withAlpha (0.1f));
        for (int y = 1; y < h; y += 3) g.fillRect (0, y, w, 1);
        return img;
    }
}

void DsButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool over = isMouseOver(), on = lit && lit();
    const auto txtCol = [&] (bool active) { return active ? ds::ledAmber : (over ? ds::bright : ds::print); };
    switch (kind)
    {
        case Kind::pad:   // a pad: name on top, its state below, a small lamp
        {
            g.setColour (down ? ds::keyDown : ds::keyFill);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (over ? ds::steelMid : ds::keyRim);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            const bool active = playIcon ? down : on;
            g.setColour (active ? ds::ledAmber : ds::ledAmber.withAlpha (0.2f));
            g.fillEllipse (r.getRight() - 12.0f, r.getY() + 7.0f, 5.0f, 5.0f);
            g.setColour (txtCol (false));
            g.setFont (Fiss::mono (10.0f, Fiss::Weight::semibold));
            g.drawText (face, Rectangle<float> (r.getX() + 7.0f, r.getY() + 4.0f, r.getWidth() - 20.0f, 14.0f), Justification::centredLeft, false);
            if (playIcon)
            {
                Path tri; tri.addTriangle (-7.0f, -10.0f, -7.0f, 10.0f, 10.0f, 0.0f);
                g.setColour (txtCol (down));
                g.fillPath (tri, AffineTransform::translation (r.getCentreX(), r.getBottom() - 19.0f));
            }
            else if (caption)
            {
                g.setColour (txtCol (on));
                g.setFont (Fiss::mono (16.0f, Fiss::Weight::semibold));
                g.drawText (caption(), Rectangle<float> (r.getX(), r.getBottom() - 32.0f, r.getWidth(), 22.0f), Justification::centred, false);
            }
            break;
        }
        case Kind::pill:
        {
            g.setColour (down ? ds::keyDown : ds::keyFill);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (over ? ds::steelMid : ds::keyRim);
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (txtCol (false));
            g.setFont (Fiss::mono (11.0f, Fiss::Weight::semibold));
            g.drawText (face, r, Justification::centred, false);
            break;
        }
        case Kind::tab:
        {
            const auto t = getLocalBounds().reduced (2, 1);
            g.setColour (on ? Digi::Col::hi : Digi::Col::bg);
            g.fillRect (t);
            g.setColour (on ? Digi::Col::hi : (over ? Digi::Col::mid : Digi::Col::dim));
            g.drawRect (t, 1);
            Digi::textCentre (g, face, t, 2, on ? Digi::Col::bg : (over ? Digi::Col::hi : Digi::Col::mid));
            break;
        }
    }
}

// ----------------------------------------------------------------------- the data wheel
void DsWheel::paint (Graphics& g)
{
    auto* q = session.param (Digi::selectedId);
    const float v = q != nullptr ? q->getValue() : 0.0f;
    const Point<float> c ((float) getWidth() * 0.5f, 52.0f);
    const float R = 48.0f, pi = MathConstants<float>::pi, a0 = pi * 0.75f, sweep = pi * 1.5f;
    constexpr int N = 31;

    const int lit = roundToInt (v * (float) (N - 1));
    for (int i = 0; i < N; ++i)   // the ring of lamps: lit up to the value
    {
        const float a = a0 + sweep * (float) i / (float) (N - 1);
        const Point<float> d (std::cos (a), std::sin (a));
        g.setColour (i <= lit ? ds::ledAmber : ds::steelLo);
        g.drawLine ({ c + d * (R - 6.0f), c + d * R }, 2.0f);
    }
    // the knob: a flat disc, a thin steel rim, a pointer
    const float kr = 34.0f;
    g.setColour (ds::keyFill);
    g.fillEllipse (c.x - kr, c.y - kr, 2 * kr, 2 * kr);
    g.setColour (ds::steelMid);
    g.drawEllipse (c.x - kr, c.y - kr, 2 * kr, 2 * kr, 1.2f);
    const float a = a0 + sweep * v;
    const Point<float> d (std::cos (a), std::sin (a));
    g.setColour (ds::bright);
    g.drawLine ({ c + d * 12.0f, c + d * 28.0f }, 2.4f);

    if (q != nullptr)   // what it edits
    {
        g.setColour (ds::print);
        g.setFont (Fiss::mono (11.0f, Fiss::Weight::semibold));
        g.drawText (Digi::fit (q->getName (32).toUpperCase(), 16), Rectangle<float> (0.0f, 106.0f, (float) getWidth(), 15.0f), Justification::centred, false);
        g.setColour (ds::ledAmber);
        g.setFont (Fiss::mono (14.0f, Fiss::Weight::semibold));
        g.drawText (Digi::fit (Digi::shortValue (q->getCurrentValueAsText()), 12), Rectangle<float> (0.0f, 123.0f, (float) getWidth(), 18.0f), Justification::centred, false);
    }
}

void DsWheel::turn (float amount, bool fine)
{
    auto* q = session.param (Digi::selectedId);
    if (q == nullptr) return;
    float n = q->getValue();
    if (q->isDiscrete())
    {
        const int steps = jmax (1, q->getNumSteps() - 1);
        n = (float) jlimit (0, steps, roundToInt (n * (float) steps) + (amount > 0.0f ? 1 : -1)) / (float) steps;
    }
    else
        n = jlimit (0.0f, 1.0f, n + amount * (fine ? 0.2f : 1.0f));
    q->setValueNotifyingHost (n);
    session.status (q->getName (32).toUpperCase(), Digi::shortValue (q->getCurrentValueAsText()));
}

void DsWheel::mouseDown (const MouseEvent& e)
{
    gesture = session.param (Digi::selectedId);
    if (gesture != nullptr) gesture->beginChangeGesture();
    lastY = e.position.y;
    acc = 0.0f;
}

void DsWheel::mouseDrag (const MouseEvent& e)
{
    if (gesture == nullptr) return;
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
    if (gesture->isDiscrete())
    {
        acc += dy;
        while (acc >= 22.0f) { turn (1.0f, false); acc -= 22.0f; }
        while (acc <= -22.0f) { turn (-1.0f, false); acc += 22.0f; }
    }
    else
        turn (dy / 220.0f, fine);
    repaint();
}

void DsWheel::mouseUp (const MouseEvent&)
{
    if (gesture != nullptr) gesture->endChangeGesture();
    gesture = nullptr;
}

void DsWheel::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (w.deltaY == 0.0f) return;
    auto* q = session.param (Digi::selectedId);
    if (q == nullptr) return;
    q->beginChangeGesture();
    turn (w.deltaY > 0.0f ? 0.01f : -0.01f, e.mods.isShiftDown());
    q->endChangeGesture();
    repaint();
}

DsButton* DsBody::add (DsButton::Kind k, const String& f, const String& tip, std::function<void()> fn)
{
    owned.push_back (std::make_unique<DsButton> (k, f, tip));
    auto* bt = owned.back().get();
    bt->onClick = std::move (fn);
    bt->onHover = [this, f] (const String& t) { if (t.isEmpty()) session.hover (nullptr); else session.status (f, t); };
    addAndMakeVisible (bt);
    buttons.push_back (bt);
    return bt;
}

DsBody::DsBody (Fiss::Session& s, RutherfordProcessor& p)
    : top (s, Branding::displayName), plot (s, p), wheel (s), session (s), proc (p)
{
    setOpaque (true);
    addAndMakeVisible (top);
    addAndMakeVisible (footer);
    addAndMakeVisible (plot);
    addAndMakeVisible (wheel);
    s.hintBold = String (Branding::displayName);
    s.hintRest = {};
    s.showStatus = [this] (const String& bo, const String& re) { footer.setStatus (bo, re); };
    s.onFlash = [this] { repaint(); };
    s.onUndoChanged = [this] { top.repaint(); };
    Digi::onSelect = [this] { repaint(); };

    auto txt = [this] (const char* id) { auto* q = session.param (id); return q != nullptr ? q->getCurrentValueAsText() : String(); };
    padTest    = add (DsButton::Kind::pad, "TEST", "audition", {});
    padPattern = add (DsButton::Kind::pad, "PTN", "bass pattern", [this] { cycle (PID::pattern, 1); });
    padKick    = add (DsButton::Kind::pad, "KICK", "kick", [this]
                      { proc.muteKick = ! proc.muteKick.load(); session.status ("KICK", proc.muteKick.load() ? "off" : "on"); });
    padBass    = add (DsButton::Kind::pad, "BASS", "bass", [this]
                      { proc.muteBass = ! proc.muteBass.load(); session.status ("BASS", proc.muteBass.load() ? "off" : "on"); });
    padTest->playIcon = true;
    padTest->onDown = [this] { proc.testHeld = true; };
    padTest->onUp = [this] { proc.testHeld = false; };
    padPattern->caption = [txt] { return txt (PID::pattern); };
    padKick->caption = [this] { return String (proc.muteKick.load() ? "OFF" : "ON"); };
    padBass->caption = [this] { return String (proc.muteBass.load() ? "OFF" : "ON"); };
    padKick->lit = [this] { return proc.muteKick.load(); };
    padBass->lit = [this] { return proc.muteBass.load(); };
    randomKey = add (DsButton::Kind::pill, "RANDOM", "new sound", [this] { session.randomize(); });
    undoKey   = add (DsButton::Kind::pill, "UNDO", "last random", [this] { session.undo(); });
    prevKey   = add (DsButton::Kind::pill, "PRESET -", "previous preset", [this] { stepPreset (-1); });
    nextKey   = add (DsButton::Kind::pill, "PRESET +", "next preset", [this] { stepPreset (1); });
    startTimerHz (6);   // pads follow automation
    setSize (W, H);
}

DsBody::~DsBody()
{
    stopTimer();
    proc.testHeld = false;
    Digi::onSelect = nullptr;
    session.showStatus = nullptr;
    session.onFlash = nullptr;
    session.onUndoChanged = nullptr;
}

void DsBody::cycle (const char* id, int delta)
{
    auto* q = session.param (id);
    if (q == nullptr) return;
    const int n = jmax (2, q->getNumSteps());
    const int cur = roundToInt (q->getValue() * (float) (n - 1));
    const int next = ((cur + delta) % n + n) % n;
    q->beginChangeGesture();
    q->setValueNotifyingHost ((float) next / (float) (n - 1));
    q->endChangeGesture();
    session.touchForHost (*q);
    session.status (q->getName (32), q->getCurrentValueAsText());
    repaint();
}

void DsBody::stepPreset (int delta)
{
    const int n = (int) factoryPresets().size();
    if (n == 0) return;
    // start from the preset that is shown now (picked in the menu, loaded with the project...), not from the first one
    const auto shown = proc.apvts.state.getProperty ("preset").toString();
    for (int i = 0; i < n; ++i)
        if (shown == factoryPresets()[(size_t) i].name) presetIndex = i;
    presetIndex = ((presetIndex + delta) % n + n) % n;
    applyFactoryPreset (proc.apvts, presetIndex);
    top.refresh();
    session.status ("PRESET", factoryPresets()[(size_t) presetIndex].name);
}

void DsBody::showPage (int pg)
{
    if (pages.empty()) return;
    const int n = (int) pages.size();
    page = ((pg % n) + n) % n;
    for (int i = 0; i < n; ++i) pages[(size_t) i]->setVisible (i == page);
    if (tabs.empty())
        for (int i = 0; i < n; ++i)
        {
            auto* t = add (DsButton::Kind::tab, pageNames[i], pageNames[i], [this, i] { showPage (i); });
            t->lit = [this, i] { return page == i; };
            tabs.push_back (t);
        }
    resized();
    repaint();
}

void DsBody::paint (Graphics& g)
{
    g.setColour (Colour (0xff050506));
    g.fillAll();

    ds::body (g, { 6.0f, 6.0f, W - 12.0f, 446.0f }, 40.0f, 12.0f);           // lid
    ds::body (g, { 6.0f, 476.0f, W - 12.0f, H - 482.0f }, 12.0f, 40.0f);     // base
    const auto hinge = Rectangle<float> (40.0f, 447.0f, W - 80.0f, 34.0f);
    ds::hinge (g, hinge);

    // power + charge LEDs on the hinge
    const float hy = hinge.getCentreY();
    ds::ledDot (g, { W * 0.5f - 30.0f, hy }, ds::ledGreen, true);
    ds::ledDot (g, { W * 0.5f + 30.0f, hy }, ds::ledAmber, false);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.setFont (Fiss::mono (9.0f, Fiss::Weight::regular));
    g.drawText ("POWER", Rectangle<float> (W * 0.5f - 86.0f, hy - 5.0f, 48.0f, 10.0f), Justification::centredRight, false);
    g.drawText ("CHARGE", Rectangle<float> (W * 0.5f + 38.0f, hy - 5.0f, 50.0f, 10.0f), Justification::centredLeft, false);

    ds::lcd (g, topLcd());
    ds::lcd (g, bottomLcd());

    // lid: "dkd" on the left, "sonicsystems" on the right, one plain font, one flat colour
    const auto lcd = topLcd().expanded (10.0f);
    g.setColour (Colour (0xffc7cad1));
    g.setFont (Fiss::mono (26.0f, Fiss::Weight::semibold));
    g.drawText (Branding::displayName, Rectangle<float> (lcd.getX(), 10.0f, 200.0f, 34.0f), Justification::centredLeft, false);
    g.setColour (ds::print);
    g.setFont (Fiss::mono (15.0f, Fiss::Weight::regular));
    g.drawText (Branding::companyName, Rectangle<float> (lcd.getRight() - 240.0f, 10.0f, 240.0f, 34.0f), Justification::centredRight, false);

    // microphone
    g.setColour (Colours::black);
    g.fillEllipse (W * 0.5f - 2.5f, 910.0f, 5.0f, 5.0f);
}

void DsBody::paintOverChildren (Graphics& g)
{
    const auto t = topLcd().toNearestInt(), bt = bottomLcd().toNearestInt();
    if (! glassTop.isValid()) glassTop = ds::glassImage (t.getWidth(), t.getHeight());
    if (! glassBottom.isValid()) glassBottom = ds::glassImage (bt.getWidth(), bt.getHeight());
    g.drawImageAt (glassTop, t.getX(), t.getY());
    g.drawImageAt (glassBottom, bt.getX(), bt.getY());
}

void DsBody::resized()
{
    auto t = topLcd().toNearestInt();
    top.setBounds (t.removeFromTop (30));
    footer.setBounds (t.removeFromBottom (22));
    plot.setBounds (t);

    auto bl = bottomLcd().toNearestInt();
    if (! tabs.empty())
    {
        auto strip = bl.removeFromTop (26);
        const int tw = strip.getWidth() / (int) tabs.size();
        for (size_t i = 0; i < tabs.size(); ++i)
            tabs[i]->setBounds (strip.getX() + (int) i * tw, strip.getY(), i + 1 == tabs.size() ? strip.getRight() - (strip.getX() + (int) i * tw) : tw, 26);
    }
    for (auto& pn : pages) pn->setBounds (bl);

    // hardware, either side of the touch screen (all >= 36 px hit targets at 100 %)
    const int lx = 90, rx = W - 90, cy = 699;
    wheel.setBounds (lx - 55, cy - 55, 110, 140);
    const int pad = 62, off = 35;
    padTest   ->setBounds (rx - off - pad / 2, cy - off - pad / 2, pad, pad);
    padPattern->setBounds (rx + off - pad / 2, cy - off - pad / 2, pad, pad);
    padKick   ->setBounds (rx - off - pad / 2, cy + off - pad / 2, pad, pad);
    padBass   ->setBounds (rx + off - pad / 2, cy + off - pad / 2, pad, pad);
    prevKey  ->setBounds (lx - 70, 796, 66, 30);
    nextKey  ->setBounds (lx + 4, 796, 66, 30);
    randomKey->setBounds (rx - 70, 796, 66, 30);
    undoKey  ->setBounds (rx + 4, 796, 66, 30);
}

// ======================================================================= editor
DsLiteEditor::DsLiteEditor (RutherfordProcessor& p)
    : AudioProcessorEditor (p), proc (p), session (p, [&p] { return p.apvts.state; }), body (session, p)
{
    setLookAndFeel (&laf);

    session.skip ({ PID::trigger, PID::fine, PID::output, PID::clipMode, PID::kickLevel });
    session.rules[PID::pattern].pick = { 0.0f, 0.0f, 0.0f, 1.0f, 2.0f };
    session.rules[PID::tail]      = { 0.06f, 0.40f };
    session.rules[PID::bassStart] = { 0.35f, 0.62f };
    session.rules[PID::bassEnd]   = { 0.85f, 0.97f };
    session.rules[PID::rise].pick = { 0.0f, 3.0f, 5.0f, 7.0f, 12.0f, 12.0f };
    session.rules[PID::curve]     = { 0.15f, 0.65f };
    session.rules[PID::kickStart] = { 0.35f, 0.75f };
    session.rules[PID::kickSweep] = { 0.25f, 0.65f };
    session.rules[PID::kickDrive] = { 0.35f, 0.85f };
    session.rules[PID::click]     = { 0.10f, 0.55f };
    session.rules[PID::bassDrive] = { 0.15f, 0.70f };
    session.rules[PID::bassTone]  = { 0.30f, 0.70f };
    session.rules[PID::bassLevel] = { 0.40f, 0.70f };

    auto& s = session;
    auto slider = [&] (const char* id, const char* l) { return keep (std::make_unique<Digi::SliderRow> (s, id, l)); };
    auto choice = [&] (const char* id, const char* l, StringArray names) { return keep (std::make_unique<Digi::ChoiceRow> (s, id, l, names)); };
    auto page = [&] (const String& name, std::vector<std::pair<Component*, int>> rows)
    {
        auto pg = std::make_unique<Digi::Page>();
        for (auto& [c, h] : rows) pg->add (c, h);
        body.addChildComponent (*pg);
        body.pages.push_back (std::move (pg));
        body.pageNames.add (name);
    };
    page ("BEAT", { { choice (PID::trigger, "TRIGGER", { "SYNC", "MIDI" }), 40 }, { choice (PID::pattern, "BASS PATTERN", { "1/4", "1/8", "1/16" }), 40 },
                    { slider (PID::tail, "KICK TAIL"), 40 }, { slider (PID::bassStart, "BASS IN"), 40 },
                    { slider (PID::bassEnd, "BASS OUT"), 40 }, { slider (PID::rise, "RISE"), 40 } });
    page ("KICK", { { slider (PID::kickStart, "START PITCH"), 40 }, { slider (PID::kickSweep, "PITCH DROP"), 40 }, { slider (PID::kickDrive, "DRIVE"), 40 },
                    { choice (PID::kickType, "DISTORTION", { "TANH", "HARD", "FOLD" }), 40 }, { slider (PID::click, "CLICK"), 40 } });
    page ("BASS", { { slider (PID::bassDrive, "DRIVE"), 40 }, { slider (PID::curve, "SWELL"), 40 }, { slider (PID::bassTone, "TONE"), 40 } });
    page ("OUT", { { slider (PID::kickLevel, "KICK LEVEL"), 40 }, { slider (PID::bassLevel, "BASS LEVEL"), 40 }, { slider (PID::output, "OUTPUT"), 40 },
                   { slider (PID::fine, "FINE TUNE"), 40 }, { choice (PID::clipMode, "CLIPPER", { "OFF", "SOFT", "HARD" }), 40 } });
    page ("OWN", { { keep (std::make_unique<Digi::OwnBox> (s, proc.own)), 68 }, { slider (Sonic::OwnID::level, "LEVEL"), 40 }, { slider (Sonic::OwnID::tune, "TUNE"), 40 },
                   { choice (Sonic::OwnID::key, "PITCH", { "FIXED", "NOTES" }), 40 }, { choice (Sonic::OwnID::mode, "MODE", { "LAYER", "REPLACE" }), 40 } });
    body.showPage (0);

    for (auto& fp : factoryPresets()) body.top.factoryNames.add (fp.name);
    body.top.applyFactory = [this] (int i) { applyFactoryPreset (proc.apvts, i); };
    body.top.currentName = [this] { return proc.apvts.state.getProperty ("preset").toString(); };
    body.top.onUserPresetLoaded = [this] (const String& n) { proc.apvts.state.setProperty ("preset", n, nullptr); body.top.refresh(); };
    session.onRandomized = [this] { proc.apvts.state.setProperty ("preset", "Random", nullptr); body.top.refresh(); };
    body.footer.level = [this] { return proc.scope.peak (512); };
    session.hover (nullptr);

    // sizing: same "uiScale" state property and 60-150 % range as the other FISSION windows, fixed aspect ratio
    addAndMakeVisible (body);
    double startScale = (double) proc.apvts.state.getProperty ("uiScale", 0.0);
    if (! (startScale > 0.0))
    {
        startScale = 0.85;
        if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
            startScale = jmin (0.85, (d->userArea.getHeight() - 90) / (double) DsBody::H, (d->userArea.getWidth() - 40) / (double) DsBody::W);
    }
    startScale = jlimit (0.6, 1.5, startScale);
    setResizable (true, true);
    setResizeLimits (roundToInt (DsBody::W * 0.6), roundToInt (DsBody::H * 0.6), roundToInt (DsBody::W * 1.5), roundToInt (DsBody::H * 1.5));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) DsBody::W / (double) DsBody::H);
    setSize (roundToInt (DsBody::W * startScale), roundToInt (DsBody::H * startScale));
    setOpaque (true);
    Fiss::keepKeyboardForHost (*this);
}

DsLiteEditor::~DsLiteEditor()
{
    proc.testHeld = false;
    session.onRandomized = nullptr;
    body.footer.level = nullptr;
    setLookAndFeel (nullptr);
}

void DsLiteEditor::resized()
{
    const float scale = jlimit (0.3f, 3.0f, (float) getWidth() / (float) DsBody::W);
    body.setTransform (AffineTransform::scale (scale));
    body.setBounds (0, 0, DsBody::W, DsBody::H);
    if (body.getParentComponent() == this)
        proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void DsLiteEditor::prepareScreenshot()
{
    Digi::frozen = true;
    body.footer.setStatic (-4.0f);
}
