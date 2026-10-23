#include "Views.h"

using namespace juce;

namespace Digi
{
namespace
{
    const Colour yellow { 0xffd9e85a };

    float norm (Fiss::Session& s, const char* id)
    {
        auto* q = s.param (id);
        return q != nullptr ? q->getValue() : 0.0f;
    }
    String textOf (Fiss::Session& s, const char* id)
    {
        auto* q = s.param (id);
        return q != nullptr ? q->getCurrentValueAsText() : String();
    }
}

String noteName (double hz, double* cents)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi = 69.0 + 12.0 * std::log2 (jmax (1.0, hz) / 440.0);
    const int n = (int) std::round (midi);
    if (cents != nullptr) *cents = (midi - (double) n) * 100.0;
    return String (names[((n % 12) + 12) % 12]) + String ((int) std::floor ((double) n / 12.0) - 1);
}

void drawBox (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setColour (Col::dim);
    g.drawRect (r, 1);
    if (title.isNotEmpty()) text (g, title, r.getX() + 6, r.getY() + 4, 2, Col::mid);
}

void segBar (Graphics& g, int x, int y, int n, int lit, int w, int h, int gap, Colour on, Colour off)
{
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i < lit ? on : off);
        g.fillRect (x + i * (w + gap), y, w, h);
    }
}

// ======================================================================= analyzer
Analyzer::Analyzer (RutherfordProcessor& p) : proc (p), window ((size_t) N), work ((size_t) (2 * N)), tmpO ((size_t) N), tmpK ((size_t) N), tmpB ((size_t) N)
{
    for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (N - 1));
    startTimerHz (30);
}

void Analyzer::analyse (const float* x, std::array<float, bands>& dst, double sr, float* subShare)
{
    for (int i = 0; i < N; ++i) work[(size_t) i] = x[i] * window[(size_t) i];
    std::fill (work.begin() + N, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data());
    const double binHz = sr / (double) N;
    if (subShare != nullptr)
    {
        double total = 0.0, sub = 0.0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double pw = (double) work[(size_t) k] * work[(size_t) k];
            total += pw;
            if ((double) k * binHz < 60.0) sub += pw;
        }
        const float now = total > 1.0e-12 ? (float) (sub / total * 100.0) : 0.0f;
        *subShare += (now - *subShare) * 0.2f;
    }
    for (int b = 0; b < bands; ++b)
    {
        const int k0 = jmax (1, (int) std::floor (bandEdgeHz (b) / binHz));
        const int k1 = jmin (N / 2 - 1, jmax (k0, (int) std::ceil (bandEdgeHz (b + 1) / binHz)));
        float m = 0.0f;
        for (int k = k0; k <= k1; ++k) m = jmax (m, work[(size_t) k]);
        const float db = 20.0f * std::log10 (m * 4.0f / (float) N + 1.0e-6f);
        const float n = jlimit (0.0f, 1.0f, (db + 66.0f) / 66.0f);
        dst[(size_t) b] = n > dst[(size_t) b] ? n : dst[(size_t) b] * 0.8f + n * 0.2f;
    }
}

void Analyzer::timerCallback()
{
    const float pk = proc.engine.guiPreClip.exchange (0.0f);
    if (pk > 0.98f) clipFrames = 12; else if (clipFrames > 0) --clipFrames;
    crushK = proc.engine.guiCrushK.load();
    crushB = proc.engine.guiCrushB.load();
    dc = proc.engine.guiDc.load();
    if (frozen) return;

    const double sr = jmax (8000.0, proc.taps.sampleRate.load());
    proc.scope.copy (tmpO.data(), N);
    proc.taps.copyRaw (tmpK.data(), tmpB.data(), N);
    analyse (tmpO.data(), out, sr, &subPct);
    analyse (tmpK.data(), kick, sr);
    analyse (tmpB.data(), bass, sr);
    for (int b = 0; b < bands; ++b) caps[(size_t) b] = jmax (out[(size_t) b], caps[(size_t) b] - 0.012f);

    const int m = 2048;
    const float* x = tmpO.data() + (N - m);
    float peak = 0.0f, peak512 = 0.0f;
    double sum = 0.0;
    for (int i = 0; i < m; ++i)
    {
        const float a = std::abs (x[i]);
        peak = jmax (peak, a);
        if (i >= m - 512) peak512 = jmax (peak512, a);
        sum += (double) x[i] * x[i];
    }
    const float pDb = Decibels::gainToDecibels (peak, -90.0f), rDb = Decibels::gainToDecibels ((float) std::sqrt (sum / m), -90.0f);
    peakDb = jmax (pDb, peakDb - 1.5f);
    rmsDb = rmsDb + (rDb - rmsDb) * 0.3f;
    crestDb = rDb > -80.0f ? pDb - rDb : 0.0f;
    const float lDb = Decibels::gainToDecibels (peak512, -90.0f);
    levelDb = jmax (lDb, levelDb - 2.5f);
    holdDb = jmax (lDb, holdDb - 0.4f);
}

// ======================================================================= SCOPE
void ScopeView::paint (Graphics& g)
{
    g.fillAll (Col::bg);
    const float bpm = jmax (20.0f, proc.engine.guiBpm.load()), phase = proc.engine.guiPhase.load();
    const double sr = jmax (8000.0, proc.taps.sampleRate.load());
    const RuthParams prm = proc.readParams();

    // ---- WAVE: one beat of the kick (green) and the bass (amber), newest at the cursor
    drawBox (g, { 4, 2, 272, 132 }, "WAVE");
    {
        const int ix = 10, iy = 24, iw = 260, ih = 104, cy = iy + ih / 2, cols = iw / 2;
        g.setColour (Col::grid);
        g.fillRect (ix, cy, iw, 1);
        const int bpb = jlimit (8, Taps::buckets - 4, (int) (60.0 / (double) bpm * sr / (double) Taps::bucketLen));
        const float ph = phase < 0.0f ? 0.999f : phase, sc = (float) ih * 0.46f;
        RectangleList<float> kl, bl;
        auto add = [&] (RectangleList<float>& l, int col, float lo, float hi)
        {
            const float y0 = (float) cy - jlimit (-1.0f, 1.0f, hi) * sc, y1 = (float) cy - jlimit (-1.0f, 1.0f, lo) * sc;
            l.addWithoutMerging (Rectangle<float> ((float) (ix + col * 2), y0, 2.0f, jmax (1.0f, y1 - y0)));
        };
        for (int c = 0; c < cols; ++c)
        {
            float age = ph - (float) c / (float) cols;
            if (age < 0.0f) age += 1.0f;
            float kmin, kmax, bmin, bmax;
            if (! proc.taps.bucketAgo ((int) (age * (float) bpb), kmin, kmax, bmin, bmax)) continue;
            add (bl, c, bmin, bmax);
            add (kl, c, kmin, kmax);
        }
        g.setColour (Col::warn);
        g.fillRectList (bl);
        g.setColour (Col::hi);
        g.fillRectList (kl);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillRect (ix + (int) (ph * (float) iw), iy, 1, ih);
    }

    // ---- SPECTRUM: 32 bands from 20 Hz to 16 kHz, the sub (below 60 Hz) in amber, peak caps
    drawBox (g, { 284, 2, 272, 132 }, "SPECTRUM");
    {
        const int x0 = 292, yb = 112;
        for (int b = 0; b < Analyzer::bands; ++b)
        {
            const bool sub = Analyzer::bandEdgeHz (b + 1) <= 61.0f;
            const int lit = roundToInt (an.out[(size_t) b] * 11.0f), cap = roundToInt (an.caps[(size_t) b] * 11.0f);
            for (int r = 0; r < 11; ++r)
            {
                g.setColour (r < lit ? (sub ? Col::warn : Col::mid) : Col::grid);
                g.fillRect (x0 + b * 8, yb - (r + 1) * 8 + 1, 7, 7);
            }
            if (cap > 0 && cap > lit)
            {
                g.setColour (Col::hi);
                g.fillRect (x0 + b * 8, yb - cap * 8 + 1, 7, 2);
            }
        }
        for (auto [hz, label] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1K" }, { 10000.0f, "10K" } })
            text (g, label, x0 + (int) (std::log (hz / 20.0f) / std::log (800.0f) * 256.0f) - textWidth (label, 2) / 2, 116, 2, Col::dim);
    }

    // ---- KICK PITCH: the drop from the start pitch to the tail note, with the live position
    drawBox (g, { 4, 140, 272, 136 }, "KICK PITCH");
    {
        const int ix = 12, iy = 164, iw = 256, ih = 84;
        const double tailHz = ruth::noteHz (proc.engine.guiNote.load(), prm.fineCents);
        auto yAt = [&] (double hz) { return (float) iy + (float) ih - (float) (std::log (jmax (30.0, hz) / 30.0) / std::log (2000.0 / 30.0)) * (float) ih; };
        for (double hz : { 50.0, 100.0, 500.0, 1000.0 }) { g.setColour (Col::grid); g.fillRect (ix, (int) yAt (hz), iw, 1); }
        Path p;
        for (int x = 0; x <= iw; x += 2)
        {
            const float y = jlimit ((float) iy, (float) (iy + ih), yAt (ruth::kickHz (prm, (double) x / (double) iw * 300.0, tailHz)));
            if (x == 0) p.startNewSubPath ((float) ix, y); else p.lineTo ((float) (ix + x), y);
        }
        g.setColour (Col::hi);
        g.strokePath (p, PathStrokeType (2.0f));
        const float khz = proc.engine.guiKickHz.load(), kms = proc.engine.guiKickMs.load();
        if (khz > 0.0f)
        {
            g.setColour (Col::warn);
            g.fillRect (ix + (int) jmin (250.0f, kms / 300.0f * (float) iw) - 3, (int) yAt (khz) - 3, 7, 7);
        }
        double cents = 0.0;
        const auto nn = noteName (tailHz, &cents);
        text (g, "TAIL " + nn + " " + String (tailHz, 2) + " HZ", 10, 252, 2, Col::mid);
        textRight (g, String (roundToInt (prm.kickStartHz)), 270, 166, 2, Col::dim);
    }

    // ---- METERS: level ladder, clip lamp, how hard the shapers work, numbers
    drawBox (g, { 284, 140, 272, 136 }, {});
    {
        const int lit = jlimit (0, 20, roundToInt ((an.levelDb + 48.0f) / 48.0f * 20.0f));
        const int hold = jlimit (0, 20, roundToInt ((an.holdDb + 48.0f) / 48.0f * 20.0f));
        for (int i = 0; i < 20; ++i)
        {
            const bool on = i < lit || i == hold - 1;
            g.setColour (on ? (i >= 18 ? Col::warn : i >= 15 ? yellow : Col::hi) : Col::grid);
            g.fillRect (294, 266 - i * 6, 22, 5);
        }
        g.setColour (an.clipFrames > 0 ? Col::warn : Col::grid);
        g.fillRect (326, 146, 12, 12);
        text (g, "CLIP", 344, 144, 2, an.clipFrames > 0 ? Col::warn : Col::dim);
        text (g, "CRUSH K", 326, 164, 2, Col::mid);
        segBar (g, 434, 166, 12, roundToInt (jlimit (0.0f, 1.0f, an.crushK) * 12.0f), 7, 10, 1, Col::hi, Col::grid);
        text (g, "CRUSH B", 326, 182, 2, Col::mid);
        segBar (g, 434, 184, 12, roundToInt (jlimit (0.0f, 1.0f, an.crushB) * 12.0f), 7, 10, 1, Col::hi, Col::grid);
        text (g, "PEAK " + String (an.peakDb, 1) + " DB", 326, 202, 2, Col::hi);
        text (g, "RMS  " + String (an.rmsDb, 1) + " DB", 326, 220, 2, Col::mid);
        text (g, "CREST " + String (an.crestDb, 1) + " DB", 326, 238, 2, Col::mid);
        text (g, "DC " + String (an.dc, 3), 326, 256, 2, Col::dim);
    }
}

// ======================================================================= MOD
void ModView::setFrom (Point<float> pt)
{
    const auto r = pad();
    const float x = jlimit (0.0f, 1.0f, (pt.x - (float) r.getX()) / (float) r.getWidth());
    const float y = jlimit (0.0f, 1.0f, ((float) r.getBottom() - pt.y) / (float) r.getHeight());
    if (gx != nullptr) gx->setValueNotifyingHost (x);
    if (gy != nullptr) gy->setValueNotifyingHost (y);
}

void ModView::mouseDown (const MouseEvent& e)
{
    if (! pad().contains (e.getPosition())) return;
    gx = session.param (PID::tail);
    gy = session.param (PID::kickDrive);
    if (gx != nullptr) gx->beginChangeGesture();
    if (gy != nullptr) gy->beginChangeGesture();
    select (PID::tail);
    setFrom (e.position);
}

void ModView::mouseDrag (const MouseEvent& e) { if (gx != nullptr || gy != nullptr) setFrom (e.position); }

void ModView::mouseUp (const MouseEvent&)
{
    if (gx != nullptr) gx->endChangeGesture();
    if (gy != nullptr) gy->endChangeGesture();
    gx = gy = nullptr;
}

void ModView::paint (Graphics& g)
{
    g.fillAll (Col::bg);

    // ---- XY pad: tail across, drive up
    drawBox (g, { 4, 2, 184, 184 }, "XY");
    {
        const auto r = pad();
        for (int i = 1; i < 4; ++i)
        {
            g.setColour (Col::grid);
            g.fillRect (r.getX() + i * r.getWidth() / 4, r.getY(), 1, r.getHeight());
            g.fillRect (r.getX(), r.getY() + i * r.getHeight() / 4, r.getWidth(), 1);
        }
        text (g, "DRIVE", r.getX() + 4, r.getY() + 2, 2, Col::dim);
        textRight (g, "TAIL", r.getRight() - 4, r.getBottom() - 16, 2, Col::dim);
        const Point<float> pt ((float) r.getX() + norm (session, PID::tail) * (float) r.getWidth(),
                               (float) r.getBottom() - norm (session, PID::kickDrive) * (float) r.getHeight());
        if (trail[(size_t) ((trailPos + 23) % 24)].getDistanceFrom (pt) > 0.5f) { trail[(size_t) trailPos] = pt; trailPos = (trailPos + 1) % 24; }
        for (int i = 0; i < 24; ++i)
        {
            const auto t = trail[(size_t) ((trailPos + i) % 24)];
            if (t.x <= 0.0f) continue;
            g.setColour (Col::mid.withAlpha ((float) i / 30.0f));
            g.fillRect (t.x - 1.0f, t.y - 1.0f, 3.0f, 3.0f);
        }
        g.setColour (Col::dim);
        g.fillRect (pt.x, (float) r.getY(), 1.0f, (float) r.getHeight());
        g.fillRect ((float) r.getX(), pt.y, (float) r.getWidth(), 1.0f);
        g.setColour (gx != nullptr ? Col::warn : Col::hi);
        g.fillRect (pt.x - 5.0f, pt.y - 5.0f, 11.0f, 11.0f);
    }

    // ---- the two LFOs: two cycles of the shape, scaled by the depth, with the live position
    const char* ids[2][4] = { { PID::lfo1Rate, PID::lfo1Shape, PID::lfo1Dest, PID::lfo1Depth }, { PID::lfo2Rate, PID::lfo2Shape, PID::lfo2Dest, PID::lfo2Depth } };
    const char* onIds[2] = { PID::lfo1On, PID::lfo2On };
    for (int i = 0; i < 2; ++i)
    {
        const Rectangle<int> box (194, 2 + i * 94, 362, 90);
        const bool on = norm (session, onIds[i]) > 0.5f, wired = session.param (ids[i][2]) != nullptr && norm (session, ids[i][2]) > 0.0f;
        const bool active = on && wired;
        drawBox (g, box, "LFO" + String (i + 1) + " " + textOf (session, ids[i][0]) + " " + textOf (session, ids[i][1]).toUpperCase());
        textRight (g, active ? Digi::fit (textOf (session, ids[i][2]).toUpperCase(), 10) + " " + shortValue (textOf (session, ids[i][3])) : (on ? String ("NO TARGET") : String ("OFF")),
                   box.getRight() - 6, box.getY() + 4, 2, active ? Col::hi : Col::dim);
        const int ix = box.getX() + 6, iw = 350, cy = box.getY() + 56, amp = 26;
        g.setColour (Col::grid);
        g.fillRect (ix, cy, iw, 1);
        const int shape = roundToInt (norm (session, ids[i][1]) * 4.0f), rate = roundToInt (norm (session, ids[i][0]) * 6.0f);
        const float depth = session.param (ids[i][3]) != nullptr ? session.param (ids[i][3])->convertFrom0to1 (norm (session, ids[i][3])) : 0.0f;
        Path p;
        for (int x = 0; x <= iw; x += 2)
        {
            const float v = ruth::lfoValue (shape, (double) x / (double) iw * 2.0, 1.0);
            const float y = (float) cy - v * jmax (0.12f, std::abs (depth)) * (depth < 0.0f ? -1.0f : 1.0f) * (float) amp;
            if (x == 0) p.startNewSubPath ((float) ix, y); else p.lineTo ((float) (ix + x), y);
        }
        g.setColour (active ? Col::hi : Col::dim);
        g.strokePath (p, PathStrokeType (2.0f));
        if (active)
        {
            const double beats = proc.engine.guiBeats.load(), cyc = beats / ruth::lfoDivBeats (rate);
            const float ph = (float) (cyc - std::floor (cyc));
            for (int k = 0; k < 2; ++k)
            {
                const float x = (float) ix + ((float) k + ph) * (float) iw * 0.5f;
                g.setColour (Col::warn.withAlpha (0.6f));
                g.fillRect (x, (float) (box.getY() + 22), 1.0f, 64.0f);
            }
            const float v = proc.engine.guiLfo[(size_t) i].load();
            g.setColour (Col::warn);
            g.fillRect ((float) ix + ph * (float) iw * 0.5f - 3.0f, (float) cy - v * std::abs (depth) * (depth < 0.0f ? -1.0f : 1.0f) * (float) amp - 3.0f, 7.0f, 7.0f);
        }
    }

    // ---- the sequencer as it plays (edit it on the SEQ page)
    if (Variant::hasBass)
    {
    drawBox (g, { 4, 192, 372, 84 }, "SEQ");
        const bool on = norm (session, PID::seqOn) > 0.5f;
        const int cur = proc.engine.guiSeqStep.load();
        const bool live = proc.engine.guiPhase.load() >= 0.0f;
        for (int i = 0; i < 16; ++i)
        {
            const float lv = norm (session, PID::seqLevel[i]);
            const int lit = roundToInt (lv * 9.0f);
            const bool now = on && live && cur == i;
            for (int r = 0; r < 9; ++r)
            {
                g.setColour (r < lit ? (now ? Col::warn : (on ? Col::hi : Col::dim)) : Col::grid);
                g.fillRect (10 + i * 22, 268 - r * 6, 18, 5);
            }
        }
    }

    // ---- the macros
    const int mx = Variant::hasBass ? 382 : 4;
    drawBox (g, { mx, 192, Variant::hasBass ? 174 : 552, 84 }, "MACRO");
    {
        const char* names[3] = { "PUNCH", "WEIGHT", "RAW" };
        const char* mids[3] = { PID::macro1, PID::macro2, PID::macro3 };
        const bool on = norm (session, PID::macroOn) > 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            text (g, names[i], mx + 8, 214 + i * 18, 2, on ? Col::mid : Col::dim);
            segBar (g, mx + 82, 216 + i * 18, 10, roundToInt (norm (session, mids[i]) * 10.0f), 7, 10, 1, on ? Col::hi : Col::dim, Col::grid);
        }
    }
}

// ======================================================================= LAB
void LabView::paint (Graphics& g)
{
    g.fillAll (Col::bg);
    const RuthParams prm = proc.readParams();
    const float bpm = jmax (20.0f, proc.engine.guiBpm.load()), phase = proc.engine.guiPhase.load();
    const double beatMs = 60000.0 / (double) bpm;
    const int steps = ruth::stepsPerBeat (prm.pattern);

    // ---- both spectra on one axis: kick green, bass amber, both yellow
    drawBox (g, { 4, 2, 364, 150 }, {});
    {
        text (g, "KICK", 10, 6, 2, Col::hi);
        text (g, "BASS", 66, 6, 2, Col::warn);
        text (g, "BOTH", 122, 6, 2, yellow);
        const int x0 = 12, yb = 134;
        for (int b = 0; b < Analyzer::bands; ++b)
        {
            const int ik = roundToInt (an.kick[(size_t) b] * 10.0f), ib = roundToInt (an.bass[(size_t) b] * 10.0f);
            for (int r = 0; r < 10; ++r)
            {
                const bool k = r < ik, s = r < ib;
                g.setColour (k && s ? yellow : k ? Col::hi : s ? Col::warn : Col::grid);
                g.fillRect (x0 + b * 11, yb - (r + 1) * 10 + 2, 9, 8);
            }
        }
        for (int edge : { 5, 12 })   // 60 Hz and 250 Hz
        {
            g.setColour (Col::dim);
            g.fillRect (x0 + edge * 11 - 1, 28, 1, 106);
        }
        text (g, "SUB", x0, 136, 2, Col::dim);
        text (g, "LOW", x0 + 5 * 11 + 6, 136, 2, Col::dim);
        text (g, "MID", x0 + 12 * 11 + 6, 136, 2, Col::dim);
    }

    // ---- tone: the note of tail and bass, the live bass pitch, the fine tuning
    drawBox (g, { 374, 2, 182, 150 }, "TONE");
    {
        const double tailHz = ruth::noteHz (proc.engine.guiNote.load(), prm.fineCents);
        double cents = 0.0;
        const auto nn = noteName (tailHz, &cents);
        text (g, "TAIL " + nn + " " + String (tailHz, 1), 380, 26, 2, Col::hi);
        text (g, "BASS " + nn + " " + String (tailHz, 1), 380, 44, 2, Col::warn);
        const float now = proc.engine.guiBassNow.load();
        text (g, "NOW  " + (now > 1.0f ? String (now, 1) : String ("--")), 380, 62, 2, Col::mid);
        const float fine = proc.engine.guiEff[8].load();
        text (g, "FINE " + String (fine >= 0.0f ? "+" : "") + String (roundToInt (fine)) + " CT", 380, 80, 2, Col::mid);
        const int cx = 466;
        g.setColour (Col::grid);
        g.fillRect (380, 112, 172, 10);
        for (int i = -6; i <= 6; ++i) { g.setColour (i == 0 ? Col::mid : Col::dim); g.fillRect (cx + i * 12, 126, 1, i == 0 ? 8 : 5); }
        g.setColour (Col::warn);
        g.fillRect (cx + roundToInt (jlimit (-100.0f, 100.0f, fine) / 100.0f * 84.0f) - 2, 108, 5, 18);
        text (g, "-", 380, 134, 2, Col::dim);
        textRight (g, "+", 552, 134, 2, Col::dim);
    }

    // ---- overlap map: where the kick and the bass are in one beat, red-amber where they sit on each other
    drawBox (g, { 4, 158, 364, 118 }, "OVERLAP 1 BEAT");
    {
        const double stepSec = ruth::stepBeats (prm.pattern) * 60.0 / (double) bpm;
        const int cols = 88;
        for (int c = 0; c < cols; ++c)
        {
            const double f = ((double) c + 0.5) / (double) cols;
            const bool k = ruth::kickEnv (prm, f * beatMs, beatMs) > 0.2f && f < (double) prm.tail + 0.02;
            float ru = 0.0f;
            const double q = f * (double) steps;
            const bool b = ruth::swell (prm, q - std::floor (q), stepSec, ru) > 0.05f;
            g.setColour (k && b ? Col::warn : k ? Col::hi : b ? Col::mid : Col::grid);
            g.fillRect (10 + c * 4, 186, 3, 22);
        }
        if (phase >= 0.0f) { g.setColour (Colours::white.withAlpha (0.8f)); g.fillRect (10 + (int) (phase * 352.0f), 182, 1, 30); }
        const double overlapMs = ((double) prm.tail - (double) prm.bassStart / steps) * beatMs, gapMs = (1.0 - (double) prm.bassEnd) / steps * beatMs;
        text (g, overlapMs > 0.0 ? "MUD " + String (roundToInt (overlapMs)) + " MS" : "MUD 0 MS", 10, 216, 2, overlapMs > 0.0 ? Col::warn : Col::mid);
        text (g, "GAP " + String (roundToInt (gapMs)) + " MS", 190, 216, 2, Col::mid);
        text (g, "SUB " + String (roundToInt (an.subPct)) + " %", 10, 236, 2, Col::hi);
        text (g, "RMS " + String (an.rmsDb, 1) + " DB", 150, 236, 2, Col::hi);
        text (g, "CLIP " + String (an.clipFrames > 0 ? "!" : "0"), 10, 256, 2, an.clipFrames > 0 ? Col::warn : Col::mid);
        text (g, "CRUSH " + String (roundToInt (an.crushK * 100.0f)) + "/" + String (roundToInt (an.crushB * 100.0f)), 150, 256, 2, Col::mid);
    }

    // ---- morph and macros
    drawBox (g, { 374, 158, 182, 118 }, "MORPH");
    {
        text (g, "A", 382, 184, 2, an.hasA ? Col::hi : Col::dim);
        textRight (g, "B", 548, 184, 2, an.hasB ? Col::hi : Col::dim);
        segBar (g, 400, 186, 16, 0, 7, 10, 1, Col::hi, Col::grid);
        const int mx = 400 + roundToInt (jlimit (0.0f, 1.0f, an.morph) * 15.0f) * 8;
        g.setColour (Col::warn);
        g.fillRect (mx, 182, 7, 18);
        const char* names[3] = { "PUNCH", "WEIGHT", "RAW" };
        const char* mids[3] = { PID::macro1, PID::macro2, PID::macro3 };
        for (int i = 0; i < 3; ++i)
        {
            text (g, names[i], 382, 208 + i * 20, 2, Col::mid);
            segBar (g, 464, 210 + i * 20, 10, roundToInt (norm (session, mids[i]) * 10.0f), 7, 10, 1, Col::hi, Col::grid);
        }
    }
}

// ======================================================================= bottom screen
void PickRow::paint (Graphics& g)
{
    const bool hov = isMouseOver (true);
    if (hov) { g.setColour (Col::grid); g.fillRect (getLocalBounds()); }
    const bool chosen = selectedId == param.paramID;
    if (chosen) { g.setColour (Col::warn); g.fillRect (0, 4, 3, getHeight() - 8); }
    const int ty = (getHeight() - 14) / 2;
    text (g, fit (label, 12), 6, ty, 2, hov || chosen ? Col::hi : Col::mid);
    const int left = 154, right = getWidth() - 4;
    g.setColour (Col::dim);
    g.drawRect (left, 6, 30, getHeight() - 12, 1);
    g.drawRect (right - 30, 6, 30, getHeight() - 12, 1);
    textCentre (g, "<", { left, 6, 30, getHeight() - 12 }, 2, Col::mid);
    textCentre (g, ">", { right - 30, 6, 30, getHeight() - 12 }, 2, Col::mid);
    textCentre (g, fit (valueText().toUpperCase(), 14), { left + 30, 6, right - left - 60, getHeight() - 12 }, 2, Col::hi);
    if (session.isLocked (param.paramID)) { g.setColour (Col::warn); g.fillRect (getWidth() - 7, 2, 5, 5); }
}

void PickRow::clickAt (Point<float> p)
{
    select (param.paramID);
    const int n = steps() + 1, cur = roundToInt (value * (float) steps());
    const int d = p.x < 154.0f + 36.0f ? -1 : 1;
    setNorm ((float) (((cur + d) % n + n) % n) / (float) steps());
}

SeqGrid::SeqGrid (Fiss::Session& s, RutherfordProcessor& p) : session (s), proc (p) { startTimerHz (20); }

void SeqGrid::setNorm (const char* id, float n)
{
    if (auto* q = session.param (id))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (jlimit (0.0f, 1.0f, n));
        q->endChangeGesture();
    }
}

void SeqGrid::beginOn (RangedAudioParameter* p)
{
    if (p == active) return;
    endGesture();
    active = p;
    if (active != nullptr) active->beginChangeGesture();
}

void SeqGrid::endGesture()
{
    if (active != nullptr) active->endChangeGesture();
    active = nullptr;
}

void SeqGrid::apply (Point<float> pt)
{
    const int col = columnAt (pt.x);
    if (col < 0 || col > 15) return;
    if (pt.y >= 34.0f && pt.y <= 144.0f)
    {
        auto* q = session.param (PID::seqLevel[col]);
        beginOn (q);
        select (PID::seqLevel[col]);
        if (q != nullptr) q->setValueNotifyingHost (jlimit (0.0f, 1.0f, std::round ((140.0f - pt.y) / 100.0f * 20.0f) / 20.0f));
    }
    else if (pt.y >= 162.0f && pt.y <= 268.0f)
    {
        auto* q = session.param (PID::seqPitch[col]);
        beginOn (q);
        select (PID::seqPitch[col]);
        if (q != nullptr) q->setValueNotifyingHost (q->convertTo0to1 (jlimit (-12.0f, 12.0f, std::round ((214.0f - pt.y) / 100.0f * 24.0f))));
    }
}

void SeqGrid::mouseDown (const MouseEvent& e)
{
    if (onBox().contains (e.getPosition())) { setNorm (PID::seqOn, norm (session, PID::seqOn) > 0.5f ? 0.0f : 1.0f); return; }
    if (clearBox().contains (e.getPosition()))
    {
        for (int i = 0; i < 16; ++i)
        {
            if (auto* q = session.param (PID::seqLevel[i])) setNorm (PID::seqLevel[i], q->getDefaultValue());
            if (auto* q = session.param (PID::seqPitch[i])) setNorm (PID::seqPitch[i], q->getDefaultValue());
        }
        return;
    }
    if (randomBox().contains (e.getPosition()))
    {
        Random r;
        static const float semis[] = { 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 5.0f, 7.0f, 12.0f, -5.0f, -12.0f };
        for (int i = 0; i < 16; ++i)
        {
            setNorm (PID::seqLevel[i], 0.4f + 0.1f * (float) r.nextInt (7));
            if (auto* q = session.param (PID::seqPitch[i])) setNorm (PID::seqPitch[i], q->convertTo0to1 (semis[r.nextInt (10)]));
        }
        return;
    }
    apply (e.position);
}

void SeqGrid::mouseDrag (const MouseEvent& e) { apply (e.position); }
void SeqGrid::mouseUp (const MouseEvent&) { endGesture(); }

void SeqGrid::mouseDoubleClick (const MouseEvent& e)
{
    const int col = columnAt (e.position.x);
    if (col < 0 || col > 15) return;
    if (auto* q = session.param (PID::seqLevel[col])) setNorm (PID::seqLevel[col], q->getDefaultValue());
    if (auto* q = session.param (PID::seqPitch[col])) setNorm (PID::seqPitch[col], q->getDefaultValue());
}

void SeqGrid::mouseMove (const MouseEvent& e)
{
    const int col = columnAt (e.position.x);
    if (col < 0 || col > 15) { session.hover (nullptr); return; }
    auto* lq = session.param (PID::seqLevel[col]);
    auto* pq = session.param (PID::seqPitch[col]);
    if (lq != nullptr && pq != nullptr)
        session.status ("STEP " + String (col + 1), lq->getCurrentValueAsText() + "  " + pq->getCurrentValueAsText());
}

void SeqGrid::paint (Graphics& g)
{
    const bool on = norm (session, PID::seqOn) > 0.5f;
    const int cur = proc.engine.guiSeqStep.load();
    const bool live = proc.engine.guiPhase.load() >= 0.0f;
    auto button = [&] (Rectangle<int> r, const String& s, bool lit)
    {
        const bool over = r.contains (getMouseXYRelative());
        g.setColour (lit ? Col::hi : (over ? Col::mid : Col::dim));
        if (lit) g.fillRect (r); else g.drawRect (r, 1);
        textCentre (g, s, r, 2, lit ? Col::bg : (over ? Col::hi : Col::mid));
    };
    button (onBox(), on ? "SEQ ON" : "SEQ OFF", on);
    button (clearBox(), "CLEAR", false);
    button (randomBox(), "RANDOM", false);

    for (int i = 0; i < 16; ++i)
    {
        const int x = 14 + i * 27;
        const bool now = on && live && cur == i;
        const float lv = norm (session, PID::seqLevel[i]);
        const int lit = roundToInt (lv * 10.0f);
        for (int r = 0; r < 10; ++r)
        {
            g.setColour (r < lit ? (now ? Col::warn : (on ? Col::hi : Col::dim)) : Col::grid);
            g.fillRect (x, 140 - (r + 1) * 10 + 1, 21, 9);
        }
        g.setColour (now ? Col::warn : (i % 4 == 0 ? Col::mid : Col::dim));
        g.fillRect (x, 148, 21, 6);
        // pitch: bars from the centre line, up = higher
        auto* pq = session.param (PID::seqPitch[i]);
        const float st = pq != nullptr ? pq->convertFrom0to1 (pq->getValue()) : 0.0f;
        const int h = roundToInt (std::abs (st) / 12.0f * 50.0f);
        g.setColour (Col::grid);
        g.fillRect (x, 164, 21, 100);
        g.setColour (Col::dim);
        g.fillRect (x, 213, 21, 2);
        if (h > 0)
        {
            g.setColour (now ? Col::warn : (on ? Col::mid : Col::dim));
            if (st > 0.0f) g.fillRect (x, 214 - h, 21, h); else g.fillRect (x, 215, 21, h);
        }
    }
}
}
