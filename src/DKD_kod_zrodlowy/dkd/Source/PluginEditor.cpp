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
        case Kind::key:   // a view key: a lamp and a name
        {
            g.setColour (down ? ds::keyDown : ds::keyFill);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? ds::ledAmber.withAlpha (0.7f) : (over ? ds::steelMid : ds::keyRim));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            g.setColour (on ? ds::ledAmber : ds::ledAmber.withAlpha (0.2f));
            g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
            g.setColour (on ? ds::bright : txtCol (false));
            g.setFont (Fiss::mono (11.0f, Fiss::Weight::semibold));
            g.drawText (face, r.withTrimmedLeft (12.0f), Justification::centred, false);
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

// ----------------------------------------------------------------------- the body
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
    : analyzer (p), top (s, Branding::displayName), plot (s, p), scope (s, p, analyzer), mod (s, p, analyzer), lab (s, p, analyzer),
      wheel (s), session (s), proc (p)
{
    setOpaque (true);
    addAndMakeVisible (top);
    addAndMakeVisible (footer);
    addAndMakeVisible (plot);
    addAndMakeVisible (scope);
    addAndMakeVisible (mod);
    addAndMakeVisible (lab);
    addAndMakeVisible (wheel);
    s.hintBold = String (Branding::displayName);
    s.hintRest = {};
    s.showStatus = [this] (const String& bo, const String& re) { footer.setStatus (bo, re); };
    s.onFlash = [this] { repaint(); };
    s.onUndoChanged = [this] { top.repaint(); };
    Digi::onSelect = [this] { repaint(); };

    // ---- keys
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
    {
        const char* names[4] = { "BEAT", "SCOPE", "MOD", "LAB" };
        const char* tips[4] = { "timing of one beat", "wave, spectrum, pitch, meters", "xy pad, lfos, sequencer", "kick against bass" };
        for (int i = 0; i < 4; ++i)
        {
            viewKeys[(size_t) i] = add (DsButton::Kind::key, names[i], tips[i], [this, i] { showView (i); });
            viewKeys[(size_t) i]->lit = [this, i] { return view == i; };
        }
    }

    // ---- knobs, faders, lever switches
    auto knob = [&] (const char* id, const char* label, int eff)
    {
        auto* k = addHw<Hw::Knob> (s, id, label);
        if (eff >= 0)
            k->effNorm = [this, id, eff] { auto* q = session.param (id); return q != nullptr ? q->convertTo0to1 (proc.engine.guiEff[(size_t) eff].load()) : -1.0f; };
        return k;
    };
    knobs = { knob (PID::tail, "TAIL", 0), knob (PID::kickDrive, "DRIVE", 2), knob (PID::bassTone, "TONE", 4), knob (PID::output, "OUT", -1) };
    faders = { addHw<Hw::Fader> (s, PID::kickLevel, "KICK"), addHw<Hw::Fader> (s, PID::bassLevel, "BASS") };
    switches = { addHw<Hw::Switch> (s, PID::trigger, "TRG", StringArray { "SYNC", "MIDI" }),
                 addHw<Hw::Switch> (s, PID::kickType, "DST", StringArray { "TANH", "HARD", "FOLD" }),
                 addHw<Hw::Switch> (s, PID::clipMode, "CLP", StringArray { "OFF", "SOFT", "HARD" }),
                 addHw<Hw::Switch> (s, PID::pattern, "PTN", StringArray { "1/4", "1/8", "1/16" }),
                 addHw<Hw::Switch> (s, Sonic::OwnID::mode, "OWN", StringArray { "LAYER", "REPL" }) };

    // ---- the DIP bank: 7 parameters on / off and the freeze of the displays
    auto pdip = [this] (const char* code, const char* name, const char* id)
    {
        Hw::DipBank::Entry e;
        e.code = code; e.name = name;
        e.get = [this, id] { auto* q = session.param (id); return q != nullptr && q->getValue() > 0.5f; };
        e.set = [this, id] (bool v)
        {
            if (auto* q = session.param (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (v ? 1.0f : 0.0f); q->endChangeGesture(); }
        };
        return e;
    };
    Hw::DipBank::Entry freeze;
    freeze.code = "FZ"; freeze.name = "FREEZE";
    freeze.get = [this] { return analyzer.frozen; };
    freeze.set = [this] (bool v) { analyzer.frozen = v; };
    dip = addHw<Hw::DipBank> (s, std::vector<Hw::DipBank::Entry> { pdip ("L1", "LFO 1", PID::lfo1On), pdip ("L2", "LFO 2", PID::lfo2On), pdip ("SQ", "SEQUENCER", PID::seqOn),
                                                                  pdip ("MC", "MACROS", PID::macroOn), pdip ("CK", "CLICK", PID::clickOn), pdip ("RS", "PITCH RISE", PID::riseOn),
                                                                  pdip ("DC", "DC FILTER", PID::dcOn), freeze });

    // ---- lamps, ladder, crossfader
    auto lamp = [] (const char* label, std::function<float()> f, Colour c) { return Hw::StatusLeds::Row { label, std::move (f), c }; };
    leds = addHw<Hw::StatusLeds> (std::vector<Hw::StatusLeds::Row> {
        lamp ("SYNC", [this] { return proc.engine.guiPlaying.load() ? 1.0f : 0.0f; }, Hw::Col::green),
        lamp ("BEAT", [this] { const double b = proc.engine.guiBeats.load(); const float f = (float) (b - std::floor (b)); return f < 0.18f ? 1.0f - f / 0.18f : 0.0f; }, Hw::Col::amber),
        lamp ("KICK", [this]
              {
                  const int k = proc.engine.kicksFired.load();
                  if (k != lastKicks) { lastKicks = k; kickAt = Time::getMillisecondCounterHiRes(); }
                  return jlimit (0.0f, 1.0f, 1.0f - (float) ((Time::getMillisecondCounterHiRes() - kickAt) / 150.0));
              }, Hw::Col::green),
        lamp ("BASS", [this] { return jlimit (0.0f, 1.0f, proc.engine.guiBassAmp.load() * 1.4f); }, Hw::Col::amber),
        lamp ("SEQ", [this] { auto* q = session.param (PID::seqOn); return q != nullptr && q->getValue() > 0.5f ? 1.0f : 0.0f; }, Hw::Col::green),
        lamp ("LFO", [this]
              {
                  const bool a = session.param (PID::lfo1On)->getValue() > 0.5f, b = session.param (PID::lfo2On)->getValue() > 0.5f;
                  return (a || b) ? 0.35f + 0.65f * jmax (a ? std::abs (proc.engine.guiLfo[0].load()) : 0.0f, b ? std::abs (proc.engine.guiLfo[1].load()) : 0.0f) : 0.0f;
              }, Hw::Col::amber),
        lamp ("MUTE", [this] { return proc.muteKick.load() || proc.muteBass.load() ? 1.0f : 0.0f; }, Hw::Col::amber),
        lamp ("CLIP", [this] { return analyzer.clipFrames > 0 ? 1.0f : 0.0f; }, Hw::Col::red) });
    ladder = addHw<Hw::LevelLadder> (analyzer);
    morphSlider = addHw<Hw::MorphSlider> (analyzer);
    morphSlider->onStoreA = [this] { store (true); };
    morphSlider->onStoreB = [this] { store (false); };
    morphSlider->onBegin = [this]
    {
        morphHeld = snapA;
        for (auto& [q, v] : morphHeld) q->beginChangeGesture();
    };
    morphSlider->onMorph = [this] (float t) { morphTo (t); };
    morphSlider->onEnd = [this] { for (auto& [q, v] : morphHeld) q->endChangeGesture(); morphHeld.clear(); };
    snapA = fromText (proc.apvts.state.getProperty ("morphA").toString());
    snapB = fromText (proc.apvts.state.getProperty ("morphB").toString());
    analyzer.hasA = ! snapA.empty();
    analyzer.hasB = ! snapB.empty();

    showView (0);
    startTimerHz (6);   // pads follow automation
    setSize (W, H);
}

DsBody::~DsBody()
{
    stopTimer();
    proc.testHeld = false;
    for (auto& [q, v] : morphHeld) q->endChangeGesture();
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

// ---- the two stored sounds of the crossfader: every parameter except OWN SOUND and the trigger mode
DsBody::Snap DsBody::capture() const
{
    Snap s;
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
        {
            if (r->paramID.startsWith ("own") || r->paramID == PID::trigger) continue;
            s.push_back ({ r, r->getValue() });
        }
    return s;
}

String DsBody::toText (const Snap& s)
{
    String t;
    for (auto& [q, v] : s) t << q->paramID << "=" << String (v, 5) << ";";
    return t;
}

DsBody::Snap DsBody::fromText (const String& t) const
{
    Snap s;
    for (auto& tok : StringArray::fromTokens (t, ";", ""))
    {
        const auto id = tok.upToFirstOccurrenceOf ("=", false, false);
        if (auto* q = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (id)))
            s.push_back ({ q, tok.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
    }
    return s;
}

void DsBody::store (bool toA)
{
    auto s = capture();
    proc.apvts.state.setProperty (toA ? "morphA" : "morphB", toText (s), nullptr);
    (toA ? snapA : snapB) = s;
    (toA ? analyzer.hasA : analyzer.hasB) = true;
    analyzer.morph = toA ? 0.0f : 1.0f;
    session.status (toA ? "MORPH A" : "MORPH B", "stored");
}

void DsBody::morphTo (float t)
{
    analyzer.morph = t;
    if (snapA.empty() || snapA.size() != snapB.size()) return;
    for (size_t i = 0; i < snapA.size(); ++i)
    {
        auto* q = snapA[i].first;
        if (q != snapB[i].first) continue;
        const float a = snapA[i].second, b = snapB[i].second;
        q->setValueNotifyingHost (q->isDiscrete() ? (t < 0.5f ? a : b) : a + (b - a) * t);
    }
    top.refresh();
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

void DsBody::showView (int v)
{
    view = jlimit (0, 3, v);
    plot.setVisible (view == 0);
    scope.setVisible (view == 1);
    mod.setVisible (view == 2);
    lab.setVisible (view == 3);
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
    scope.setBounds (t);
    mod.setBounds (t);
    lab.setBounds (t);

    auto bl = bottomLcd().toNearestInt();
    if (! tabs.empty())
    {
        auto strip = bl.removeFromTop (26);
        const int tw = strip.getWidth() / (int) tabs.size();
        for (size_t i = 0; i < tabs.size(); ++i)
            tabs[i]->setBounds (strip.getX() + (int) i * tw, strip.getY(), i + 1 == tabs.size() ? strip.getRight() - (strip.getX() + (int) i * tw) : tw, 26);
    }
    for (auto& pn : pages) pn->setBounds (bl);

    // lid: view keys under the top screen, lamps and ladder at the sides, lever switches below them
    for (size_t i = 0; i < viewKeys.size(); ++i) viewKeys[i]->setBounds (200 + (int) i * 116, 414, 92, 26);
    leds->setBounds (18, 70, 104, 200);
    ladder->setBounds (738, 70, 54, 226);
    for (int i = 0; i < 3; ++i) switches[(size_t) i]->setBounds (10 + i * 38, 306, 38, 112);
    switches[3]->setBounds (730, 306, 38, 112);
    switches[4]->setBounds (772, 306, 38, 112);

    // base, left: knobs, data wheel, preset keys, DIP bank
    knobs[0]->setBounds (14, 500, 76, 96);
    knobs[1]->setBounds (94, 500, 76, 96);
    wheel.setBounds (38, 606, 110, 142);
    prevKey->setBounds (14, 756, 74, 24);
    nextKey->setBounds (98, 756, 74, 24);
    dip->setBounds (14, 794, 158, 94);

    // base, right: knobs, pads, keys, faders
    knobs[2]->setBounds (668, 500, 76, 96);
    knobs[3]->setBounds (748, 500, 76, 96);
    const int px = W - 93, py = 664, pad = 56, off = 31;
    padTest   ->setBounds (px - off - pad / 2, py - off - pad / 2, pad, pad);
    padPattern->setBounds (px + off - pad / 2, py - off - pad / 2, pad, pad);
    padKick   ->setBounds (px - off - pad / 2, py + off - pad / 2, pad, pad);
    padBass   ->setBounds (px + off - pad / 2, py + off - pad / 2, pad, pad);
    randomKey->setBounds (668, 756, 74, 24);
    undoKey  ->setBounds (752, 756, 74, 24);
    faders[0]->setBounds (684, 792, 56, 118);
    faders[1]->setBounds (754, 792, 56, 118);

    // under the touch screen: the crossfader between two stored sounds
    morphSlider->setBounds (200, 884, 440, 30);
}

// ======================================================================= editor
DsLiteEditor::DsLiteEditor (RutherfordProcessor& p)
    : AudioProcessorEditor (p), proc (p), session (p, [&p] { return p.apvts.state; }), body (session, p)
{
    setLookAndFeel (&laf);

    session.skip ({ PID::trigger, PID::fine, PID::output, PID::clipMode, PID::kickLevel });
    // RANDOM touches the sound, never the modulation / sequencer / switches
    for (auto* id : { PID::lfo1Rate, PID::lfo1Shape, PID::lfo1Dest, PID::lfo1Depth, PID::lfo2Rate, PID::lfo2Shape, PID::lfo2Dest, PID::lfo2Depth,
                      PID::macro1, PID::macro2, PID::macro3, PID::lfo1On, PID::lfo2On, PID::seqOn, PID::macroOn, PID::clickOn, PID::riseOn, PID::dcOn })
        session.rules[id].skip = true;
    for (int i = 0; i < 16; ++i) { session.rules[PID::seqLevel[i]].skip = true; session.rules[PID::seqPitch[i]].skip = true; }
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
    // `eff` = which target of the modulation this parameter is (-1 = none): its row shows where the sound is now
    auto slider = [&] (const char* id, const char* l, int eff = -1)
    {
        auto* r = keep (std::make_unique<Digi::SliderRow> (s, id, l));
        if (eff >= 0)
            r->effNorm = [this, id, eff] { auto* q = session.param (id); return q != nullptr ? q->convertTo0to1 (proc.engine.guiEff[(size_t) eff].load()) : -1.0f; };
        return r;
    };
    auto choice = [&] (const char* id, const char* l, StringArray names) { return keep (std::make_unique<Digi::ChoiceRow> (s, id, l, names)); };
    auto pick = [&] (const char* id, const char* l) { return keep (std::make_unique<Digi::PickRow> (s, id, l)); };
    auto page = [&] (const String& name, std::vector<std::pair<Component*, int>> rows)
    {
        auto pg = std::make_unique<Digi::Page>();
        for (auto& [c, h] : rows) pg->add (c, h);
        body.addChildComponent (*pg);
        body.pages.push_back (std::move (pg));
        body.pageNames.add (name);
    };
    page ("BEAT", { { choice (PID::trigger, "TRIGGER", { "SYNC", "MIDI" }), 40 }, { choice (PID::pattern, "BASS PATTERN", { "1/4", "1/8", "1/16" }), 40 },
                    { slider (PID::tail, "KICK TAIL", 0), 40 }, { slider (PID::bassStart, "BASS IN"), 40 },
                    { slider (PID::bassEnd, "BASS OUT"), 40 }, { slider (PID::rise, "RISE", 7), 40 } });
    page ("KICK", { { slider (PID::kickStart, "START PITCH"), 40 }, { slider (PID::kickSweep, "PITCH DROP", 1), 40 }, { slider (PID::kickDrive, "DRIVE", 2), 40 },
                    { choice (PID::kickType, "DISTORTION", { "TANH", "HARD", "FOLD" }), 40 }, { slider (PID::click, "CLICK", 3), 40 } });
    page ("BASS", { { slider (PID::bassDrive, "DRIVE", 5), 40 }, { slider (PID::curve, "SWELL"), 40 }, { slider (PID::bassTone, "TONE", 4), 40 } });
    page ("OUT", { { slider (PID::kickLevel, "KICK LEVEL"), 40 }, { slider (PID::bassLevel, "BASS LEVEL", 6), 40 }, { slider (PID::output, "OUTPUT"), 40 },
                   { slider (PID::fine, "FINE TUNE", 8), 40 }, { choice (PID::clipMode, "CLIPPER", { "OFF", "SOFT", "HARD" }), 40 } });
    page ("LFO", { { pick (PID::lfo1Rate, "L1 RATE"), 34 }, { pick (PID::lfo1Shape, "L1 SHAPE"), 34 }, { pick (PID::lfo1Dest, "L1 TARGET"), 34 }, { slider (PID::lfo1Depth, "L1 DEPTH"), 34 },
                   { pick (PID::lfo2Rate, "L2 RATE"), 34 }, { pick (PID::lfo2Shape, "L2 SHAPE"), 34 }, { pick (PID::lfo2Dest, "L2 TARGET"), 34 }, { slider (PID::lfo2Depth, "L2 DEPTH"), 34 } });
    page ("SEQ", { { keep (std::make_unique<Digi::SeqGrid> (s, proc)), 290 } });
    page ("MAC", { { slider (PID::macro1, "PUNCH"), 40 }, { slider (PID::macro2, "WEIGHT"), 40 }, { slider (PID::macro3, "RAW"), 40 } });
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
