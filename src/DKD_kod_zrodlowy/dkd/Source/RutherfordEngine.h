#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "OwnSound.h"   // OWN SOUND hook: the engine tells the user's sample when it starts a hit
#include "Analysis.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// =====================================================================================
//  rutherford: a hardstyle kick + reverse bass from one note (mono, last-note priority, both on the same note,
//  folded into E1..D#2 like moonquakekick: an F anywhere = F1, 43.65 Hz).
//   Host Sync (default): while a note is held, a kick on every beat of the host and reverse-bass swells between the
//     kicks (1/4, 1/8 or 1/16 steps), locked to the host beat. Transport stopped: a free clock at the host tempo
//     (150 BPM if the host sends none) that starts with the first key press.
//   MIDI Note: every note-on = one kick + the swells of one beat (a one-shot).
//   Kick: sine with an exponential pitch drop (start -> tail note) + a short click; the tail length is a fraction of
//     the beat ("Kick Tail"), so it follows the tempo.
//   Bass: saw + sine on the same note, amplitude x^curve from "Bass In" to "Bass Out" of every step, a pitch rise
//     at the end of the swell (like a reversed kick tail).
//   Both -> 4x oversampled shapers (kick: tanh / hard clip / foldback, bass: tanh) -> 24 dB lowpasses -> sum + click
//   -> DC block -> output gain -> clipper (off / soft / hard).
//  Modulation (dkd): 2 tempo-synced LFOs and 3 macros move parameters of the sound without touching the saved
//  values; a 16 step sequencer sets the level and the pitch of every bass swell. All off by default.
// =====================================================================================

struct RuthParams
{
    int   trigger = 0;           // 0 = host sync, 1 = MIDI note
    int   pattern = 0;           // 0 = 1/4, 1 = 1/8, 2 = 1/16 (bass steps)
    float tail = 0.30f;          // kick tail, fraction of a beat
    float bassStart = 0.38f;     // swell start, fraction of a step
    float bassEnd = 0.95f;       // swell end, fraction of a step (the rest = gap before the next kick)
    float riseSt = 12.0f;
    float curve = 2.6f;
    float kickStartHz = 1800.0f, kickSweepMs = 55.0f, kickDriveDb = 24.0f;
    int   kickType = 1;          // 0 tanh, 1 hard clip, 2 foldback
    float click = 0.35f, kickLevel = 0.9f;
    float bassDriveDb = 14.0f, bassToneHz = 1600.0f, bassLevel = 0.55f;
    float fineCents = 0.0f;
    float outputDb = -3.0f;
    int   clipMode = 1;          // 0 off, 1 soft, 2 hard

    struct Lfo { int rate = 4, shape = 0, dest = 0; float depth = 0.0f; bool on = false; };
    Lfo lfo[2];
    float macro[3] = { 0.0f, 0.0f, 0.0f };
    bool  macroOn = true, seqOn = false, clickOn = true, riseOn = true, dcOn = true;
    float seqLevel[16], seqPitch[16];

    RuthParams() { for (int i = 0; i < 16; ++i) { seqLevel[i] = 1.0f; seqPitch[i] = 0.0f; } }
};

struct HostTime
{
    bool   playing = false;
    double ppq = 0.0;            // quarter notes at the first sample of the block
    double bpm = 150.0;
};

namespace ruth
{
    inline int foldNote (int note) { int n = note; while (n >= 40) n -= 12; while (n < 28) n += 12; return n; }
    inline double noteHz (int note, float cents) { return 440.0 * std::pow (2.0, (foldNote (note) - 69 + cents / 100.0) / 12.0); }
    inline double stepBeats (int pattern) { return pattern == 2 ? 0.25 : pattern == 1 ? 0.5 : 1.0; }
    inline int stepsPerBeat (int pattern) { return pattern == 2 ? 4 : pattern == 1 ? 2 : 1; }

    // swell amplitude 0..1 at `s` (0..1 of a step of `stepSec`); riseU = 0..1 how far into the pitch rise.
    // Shared with the GUI.
    inline float swell (const RuthParams& p, double s, double stepSec, float& riseU)
    {
        riseU = 0.0f;
        if (p.bassEnd <= p.bassStart + 0.01f || s < p.bassStart || s >= p.bassEnd) return 0.0f;
        const double r = (s - p.bassStart) / (p.bassEnd - p.bassStart);
        const double fade = juce::jmin (1.0, (p.bassEnd - s) * stepSec / 0.0015) * juce::jmin (1.0, (s - p.bassStart) * stepSec / 0.001);
        const double rr = juce::jmax (0.0, (r - 0.6) / 0.4);
        riseU = (float) (rr * rr);
        return (float) (std::pow (r, (double) p.curve) * fade);
    }

    // kick tail as a function of the beat length; shared with the GUI
    inline double tailLenMs (const RuthParams& p, double beatMs) { return p.tail * beatMs * 0.72; }
    inline double tailDecayMs (const RuthParams& p, double beatMs) { return juce::jmax (3.0, p.tail * beatMs * 0.07); }
    inline float kickEnv (const RuthParams& p, double ms, double beatMs)
    {
        double e = 0.8 + 0.55 * std::exp (-ms / 70.0);
        const double len = tailLenMs (p, beatMs);
        if (ms > len) e *= std::exp (-(ms - len) / tailDecayMs (p, beatMs));
        return (float) juce::jmin (1.0, e);
    }
    // the pitch of the kick `ms` after its start (Hz); shared with the GUI
    inline double kickHz (const RuthParams& p, double ms, double tailHz)
    {
        const double start = juce::jmax ((double) p.kickStartHz, tailHz);
        const double x = juce::jmin (1.0, ms / juce::jmax (1.0f, p.kickSweepMs));
        return tailHz * std::pow (start / tailHz, std::pow (1.0 - x, 3.0));
    }

    inline float shape (int type, float x)
    {
        if (type == 1) return juce::jlimit (-1.0f, 1.0f, x);
        if (type == 2) { x = std::fmod (x + 1.0f, 4.0f); if (x < 0.0f) x += 4.0f; return x < 2.0f ? x - 1.0f : 3.0f - x; }
        return std::tanh (x);
    }

    inline double polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    struct Svf    // TPT state-variable lowpass (Cytomic)
    {
        float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0;
        void set (float fc, float sr, float q)
        {
            const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.0f, sr * 0.45f, fc) / sr);
            const float k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0; }
        inline float lp (float v0)
        {
            const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            return v2;
        }
    };

    // ---- modulation: the targets of an LFO / macro (index = LFO "Target" - 1) and how each one moves
    constexpr int numDest = 9;   // kick tail, kick drop, kick drive, click, bass tone, bass drive, bass level, rise, fine tune
    struct DestMap { float lo, hi; bool logScale; };
    inline const DestMap& destMap (int i)
    {
        static const DestMap m[numDest] = { { 0.10f, 0.98f, false }, { 10.0f, 200.0f, true }, { 0.0f, 40.0f, false }, { 0.0f, 1.0f, false },
                                            { 300.0f, 6000.0f, true }, { 0.0f, 36.0f, false }, { 0.0f, 1.0f, false }, { 0.0f, 24.0f, false },
                                            { -100.0f, 100.0f, false } };
        return m[juce::jlimit (0, numDest - 1, i)];
    }
    // v moved by d, a fraction of the whole range (in log steps for Hz / ms)
    inline float modulate (float v, int dest, float d)
    {
        if (d == 0.0f) return v;
        const auto& m = destMap (dest);
        if (m.logScale)
        {
            const float n = std::log (juce::jmax (m.lo, v) / m.lo) / std::log (m.hi / m.lo);
            return m.lo * std::pow (m.hi / m.lo, juce::jlimit (0.0f, 1.0f, n + d));
        }
        const float n = (v - m.lo) / (m.hi - m.lo);
        return m.lo + juce::jlimit (0.0f, 1.0f, n + d) * (m.hi - m.lo);
    }
    inline double lfoDivBeats (int rate)
    {
        static constexpr double t[7] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
        return t[juce::jlimit (0, 6, rate)];
    }
    // -1..1; shape 0 sine, 1 triangle, 2 saw, 3 square, 4 random (a new value every cycle)
    inline float lfoValue (int shape, double clockBeats, double divBeats)
    {
        const double cyc = clockBeats / divBeats, p = cyc - std::floor (cyc);
        switch (shape)
        {
            case 1: return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p);
            case 2: return (float) (2.0 * p - 1.0);
            case 3: return p < 0.5 ? 1.0f : -1.0f;
            case 4:
            {
                uint32_t x = (uint32_t) (int64_t) std::floor (cyc);
                x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
                return (float) (x & 0xffffu) / 32767.5f - 1.0f;
            }
            default: return (float) std::sin (juce::MathConstants<double>::twoPi * p);
        }
    }
}

class RutherfordEngine
{
public:
    void prepare (double sr, int)
    {
        sampleRate = sr;
        // the oversampler is created once (constructor) and only reset here: re-creating it while a host still has
        // an audio callback in flight (FL Studio: Replace / re-prepare) freed the object under the audio thread
        os->reset();
        os->initProcessing ((size_t) chunk);
        osLatency = os->getLatencyInSamples();
        for (auto& v : kicks) v = {};
        numHeld = 0; local = -1.0; clock = 0.0; prevPpq = -1.0; bassAmp = 0.0f; bassPhase = 0.0;
        for (auto* f : { &lpK1, &lpK2, &lpB1, &lpB2 }) f->reset();
        dcX = dcY = 0.0f;
        dcR = (float) std::pow (0.9987, 48000.0 / sr);   // the same ~10 Hz corner at every sample rate
        ampCoef = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));
        hpA = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 1500.0 / sr);
        clickEnv = 0.0f; clickMul = (float) std::exp (-1.0 / (0.005 * sr));
        clickDelay = juce::jlimit (0, ringSize - 1, juce::roundToInt (osLatency));
        ring.fill (0.0f); ringW = 0;
        outGain.reset (sr, 0.03);
        snapGain = true;
        if (taps != nullptr) taps->sampleRate.store (sr);
    }

    float getLatency() const { return osLatency; }

    void setParams (const RuthParams& p)
    {
        if (p.trigger != base.trigger) { local = -1.0; fadeKicks(); }
        base = p;
        applyModulation (clock);
        const float g = juce::Decibels::decibelsToGain (p.outputDb);
        if (snapGain) { outGain.setCurrentAndTargetValue (g); snapGain = false; } else outGain.setTargetValue (g);
        if (numHeld > 0) bassHz = ruth::noteHz (held[(size_t) numHeld - 1], params.fineCents);
    }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const HostTime& host)
    {
        bpm = host.bpm > 20.0 ? host.bpm : 150.0;
        inc = bpm / 60.0 / sampleRate;
        const bool wasPlaying = playing;
        playing = host.playing;
        if (playing)
        {
            // a jump (loop restart, seek, transport start) must not swallow the kick of the beat it lands on
            if (! wasPlaying || std::abs (host.ppq - clock) > 0.05)
                prevPpq = host.ppq - std::floor (host.ppq) < 0.02 ? std::floor (host.ppq) - 1.0 : host.ppq;
            clock = host.ppq;   // locked to the host every block; stopped = free-running
        }
        const int total = buffer.getNumSamples();
        int pos = 0;
        auto it = midi.begin();
        while (pos < total)
        {
            while (it != midi.end() && (*it).samplePosition <= pos) { nowSample = pos; handle ((*it).getMessage()); ++it; }
            int end = juce::jmin (total, pos + chunk);
            if (it != midi.end()) end = juce::jmin (end, (*it).samplePosition);
            if (end <= pos) end = pos + 1;
            render (buffer, pos, end - pos);
            pos = end;
        }
        while (it != midi.end()) { nowSample = total - 1; handle ((*it).getMessage()); ++it; }

        const bool sync = params.trigger == 0;
        guiPhase.store (sync ? (numHeld > 0 ? (float) (clock - std::floor (clock)) : -1.0f) : (float) local, std::memory_order_relaxed);
        guiBpm.store ((float) bpm, std::memory_order_relaxed);
        guiNote.store (numHeld > 0 ? held[(size_t) numHeld - 1] : lastNote, std::memory_order_relaxed);
        guiPlaying.store (playing, std::memory_order_relaxed);
        guiBeats.store (clock, std::memory_order_relaxed);
    }

    bool isSounding() const
    {
        for (auto& v : kicks) if (v.active) return true;
        return bassAmp > 1.0e-4f;
    }

    // for the GUI (any thread)
    std::atomic<float> guiPhase { -1.0f };   // 0..1 within the beat while it plays, -1 = idle
    std::atomic<float> guiBpm { 150.0f };
    std::atomic<int>   guiNote { 53 };
    std::atomic<int>   kicksFired { 0 };
    std::atomic<bool>  guiPlaying { false };
    std::atomic<double> guiBeats { 0.0 };
    std::atomic<float> guiKickHz { 0.0f }, guiKickMs { 0.0f };   // the newest kick voice now (0 = none)
    std::atomic<float> guiBassNow { 0.0f }, guiBassAmp { 0.0f };  // pitch of the bass now (0 = silent), its level
    std::atomic<float> guiCrushK { 0.0f }, guiCrushB { 0.0f };    // 0..1 how much the shapers flatten kick / bass
    std::atomic<float> guiPreClip { 0.0f };                       // peak before the output clipper, max-held until the editor reads it
    std::atomic<float> guiDc { 0.0f };
    std::atomic<int>   guiSeqStep { 0 };
    std::array<std::atomic<float>, 2> guiLfo {};                  // LFO values now, -1..1 (0 = off)
    std::array<std::atomic<float>, ruth::numDest> guiEff {};      // the targets as they are now: tail, drop, drive, click, tone, bass drive, bass level, rise, fine
    Taps* taps = nullptr;
    Sonic::OwnSound* own = nullptr;   // OWN SOUND: plays on every kick
    bool kickMuted = false;           // OWN SOUND in REPLACE: the synthesized kick + click are silent
    bool bassMuted = false;           // the editor's BASS key: the reverse bass is silent

private:
    struct Kick
    {
        bool active = false, fading = false;
        double t = 0.0, phase = 0.0, tailHz = 43.65, startHz = 1800.0, lenMs = 90.0, decayMs = 8.0, curHz = 0.0;
        float fade = 1.0f;
    };

    void fadeKicks() { for (auto& v : kicks) if (v.active) v.fading = true; }

    // `base` = the parameters as saved; `params` = what the sound uses right now (macros, LFOs, switches applied)
    void applyModulation (double clockBeats)
    {
        params = base;
        if (! base.riseOn) params.riseSt = 0.0f;
        float d[ruth::numDest] = {};
        if (base.macroOn)
        {
            const float m1 = base.macro[0], m2 = base.macro[1], m3 = base.macro[2];   // punch, weight, raw
            d[3] += 0.35f * m1; d[1] -= 0.25f * m1; d[2] += 0.20f * m1;
            d[0] += 0.20f * m2; d[6] += 0.20f * m2; d[4] -= 0.25f * m2;
            d[2] += 0.25f * m3; d[5] += 0.25f * m3; d[7] += 0.15f * m3; d[4] += 0.15f * m3;
        }
        for (int i = 0; i < 2; ++i)
        {
            const auto& L = base.lfo[i];
            float v = 0.0f;
            if (L.on && L.dest > 0)
            {
                v = ruth::lfoValue (L.shape, clockBeats, ruth::lfoDivBeats (L.rate));
                d[L.dest - 1] += L.depth * 0.5f * v;
            }
            guiLfo[(size_t) i].store (L.on ? v : 0.0f, std::memory_order_relaxed);
        }
        params.tail        = ruth::modulate (base.tail, 0, d[0]);
        params.kickSweepMs = ruth::modulate (base.kickSweepMs, 1, d[1]);
        params.kickDriveDb = ruth::modulate (base.kickDriveDb, 2, d[2]);
        params.click       = ruth::modulate (base.click, 3, d[3]);
        params.bassToneHz  = ruth::modulate (base.bassToneHz, 4, d[4]);
        params.bassDriveDb = ruth::modulate (base.bassDriveDb, 5, d[5]);
        params.bassLevel   = ruth::modulate (base.bassLevel, 6, d[6]);
        if (base.riseOn) params.riseSt = ruth::modulate (base.riseSt, 7, d[7]);
        params.fineCents   = ruth::modulate (base.fineCents, 8, d[8]);
        const float eff[ruth::numDest] = { params.tail, params.kickSweepMs, params.kickDriveDb, params.click, params.bassToneHz,
                                           params.bassDriveDb, params.bassLevel, params.riseSt, params.fineCents };
        for (int i = 0; i < ruth::numDest; ++i) guiEff[(size_t) i].store (eff[i], std::memory_order_relaxed);

        lpK1.set (7500.0f, (float) sampleRate, 0.54f); lpK2.set (7500.0f, (float) sampleRate, 1.31f);
        lpB1.set (params.bassToneHz, (float) sampleRate, 0.54f); lpB2.set (params.bassToneHz, (float) sampleRate, 1.31f);
        if (numHeld > 0) bassHz = ruth::noteHz (held[(size_t) numHeld - 1], params.fineCents);
    }

    void triggerKick()
    {
        fadeKicks();
        Kick* slot = &kicks[0];
        for (auto& v : kicks) if (! v.active) { slot = &v; break; }
        const double beatMs = 60000.0 / bpm;
        *slot = {};
        slot->active = true;
        slot->tailHz = bassHz;
        slot->startHz = juce::jmax ((double) params.kickStartHz, bassHz);
        slot->lenMs = ruth::tailLenMs (params, beatMs);
        slot->decayMs = ruth::tailDecayMs (params, beatMs);
        clickEnv = 1.0f;
        kicksFired.fetch_add (1, std::memory_order_relaxed);
        if (own != nullptr) own->trigger (nowSample, numHeld > 0 ? held[(size_t) numHeld - 1] : lastNote, 0.787f);
    }

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const bool first = numHeld == 0;
            const int n = m.getNoteNumber();
            for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] == n) { for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; break; }
            if (numHeld < (int) held.size()) held[(size_t) numHeld++] = n;
            lastNote = n;
            bassHz = ruth::noteHz (n, params.fineCents);
            if (params.trigger == 1) { triggerKick(); local = 0.0; }
            else if (first)
            {
                if (! playing) { clock = 0.0; prevPpq = -1.0e-9; }               // stopped: the free clock starts now
                else if (clock - std::floor (clock) < 0.02) { triggerKick(); prevPpq = clock; }   // pressed on the beat
                else prevPpq = clock;
            }
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] == n) { for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; break; }
            if (numHeld > 0) bassHz = ruth::noteHz (held[(size_t) numHeld - 1], params.fineCents);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            numHeld = 0; local = -1.0; fadeKicks();
            if (own != nullptr) own->release (nowSample, -1);
        }
    }

    float kickSample (Kick& v)
    {
        const double ms = v.t * 1000.0;
        const double x = juce::jmin (1.0, ms / juce::jmax (1.0f, params.kickSweepMs));
        const double f = v.tailHz * std::pow (v.startHz / v.tailHz, std::pow (1.0 - x, 3.0));
        v.curHz = f;
        const float s = (float) std::sin (v.phase * juce::MathConstants<double>::twoPi);
        v.phase += f / sampleRate;
        if (v.phase >= 1.0) v.phase -= 1.0;
        double e = 0.8 + 0.55 * std::exp (-ms / 70.0);
        if (ms > v.lenMs) e *= std::exp (-(ms - v.lenMs) / v.decayMs);
        e = juce::jmin (1.0, e) * juce::jmin (1.0, ms / 0.3);
        if (v.fading) { v.fade -= (float) (1.0 / (0.003 * sampleRate)); if (v.fade <= 0.0f) { v.active = false; v.fade = 0.0f; } }
        v.t += 1.0 / sampleRate;
        if (ms > v.lenMs + 10.0 * v.decayMs) v.active = false;
        return s * (float) e * 0.9f * v.fade;
    }

    void render (juce::AudioBuffer<float>& buffer, int start, int n)
    {
        applyModulation (clock);
        const bool sync = params.trigger == 0;
        const double stepB = ruth::stepBeats (params.pattern), stepSec = stepB * 60.0 / bpm;
        double lastBassF = 0.0;
        for (int i = 0; i < n; ++i)
        {
            nowSample = start + i;
            if (sync && numHeld > 0 && std::floor (clock) > std::floor (prevPpq)) triggerKick();
            prevPpq = clock;

            double ph = -1.0;
            if (sync) { if (numHeld > 0) ph = clock; }
            else if (local >= 0.0) ph = local;
            float target = 0.0f, riseU = 0.0f, seqSemi = 0.0f;
            if (ph >= 0.0)
            {
                const double q = ph / stepB;
                const int idx = (int) std::floor (q);
                target = ruth::swell (params, q - std::floor (q), stepSec, riseU);
                const int si = ((idx % 16) + 16) % 16;
                if (params.seqOn) { target *= params.seqLevel[si]; seqSemi = params.seqPitch[si]; }
                seqStep = si;
            }
            bassAmp += (target - bassAmp) * ampCoef;
            float b = 0.0f;
            if (bassAmp > 1.0e-5f)
            {
                const double f = bassHz * std::pow (2.0, (params.riseSt * riseU + seqSemi) / 12.0), dt = f / sampleRate;
                lastBassF = f;
                bassPhase += dt;
                if (bassPhase >= 1.0) bassPhase -= 1.0;
                const double saw = 2.0 * bassPhase - 1.0 - ruth::polyBlep (bassPhase, dt);
                b = (float) ((0.45 * saw + 0.7 * std::sin (bassPhase * juce::MathConstants<double>::twoPi)) * bassAmp);
            }
            float k = 0.0f;
            for (auto& v : kicks) if (v.active) k += kickSample (v);
            kbuf[(size_t) i] = k;
            bbuf[(size_t) i] = b;

            float c = 0.0f;   // the click: high-passed noise, 5 ms, added after the shapers (stays clean)
            if (clickEnv > 1.0e-4f)
            {
                rng = rng * 1664525u + 1013904223u;
                const float nz = (float) (rng >> 8) / 8388608.0f - 1.0f;
                hpY = hpA * (hpY + nz - hpX); hpX = nz;
                c = params.clickOn ? hpY * clickEnv * params.click * 0.7f : 0.0f;
                clickEnv *= clickMul;
            }
            cbuf[(size_t) i] = c;

            clock += inc;
            if (! sync && local >= 0.0) { local += inc; if (local >= 1.0) local = -1.0; }
        }

        float* chans[] = { kbuf.data(), bbuf.data() };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        const float gk = juce::Decibels::decibelsToGain (params.kickDriveDb), gb = juce::Decibels::decibelsToGain (params.bassDriveDb);
        float* uk = up.getChannelPointer (0);
        float* ub = up.getChannelPointer (1);
        double inK = 0.0, outK = 0.0, inB = 0.0, outB = 0.0;
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            const float xk = uk[i] * gk, xb = ub[i] * gb;
            uk[i] = ruth::shape (params.kickType, xk);
            ub[i] = std::tanh (xb);
            inK += (double) xk * xk; outK += (double) uk[i] * uk[i];
            inB += (double) xb * xb; outB += (double) ub[i] * ub[i];
        }
        os->processSamplesDown (block);
        crushK = inK > 1.0e-9 ? crushK * 0.8f + 0.2f * (float) (1.0 - std::sqrt (outK / inK)) : crushK * 0.95f;
        crushB = inB > 1.0e-9 ? crushB * 0.8f + 0.2f * (float) (1.0 - std::sqrt (outB / inB)) : crushB * 0.95f;

        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;
        float prePeak = 0.0f;
        double dcSum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float k = lpK2.lp (lpK1.lp (kbuf[(size_t) i])), b = lpB2.lp (lpB1.lp (bbuf[(size_t) i]));
            ring[(size_t) ringW] = cbuf[(size_t) i];   // the click skips the oversampler: delay it by its latency
            const float c = ring[(size_t) ((ringW - clickDelay + ringSize) % ringSize)];
            ringW = (ringW + 1) % ringSize;
            const float kPart = kickMuted ? 0.0f : k * params.kickLevel + c, bPart = bassMuted ? 0.0f : b * params.bassLevel;
            float y = kPart + bPart;
            dcSum += y;
            if (params.dcOn) { const float dc = y - dcX + dcR * dcY; dcX = y; dcY = dc; y = dc; }
            const float g = outGain.getNextValue();
            y *= g;
            tapK[(size_t) i] = kPart * g; tapB[(size_t) i] = bPart * g;
            prePeak = juce::jmax (prePeak, std::abs (y));
            if (params.clipMode == 1)      y = std::tanh (y);
            else if (params.clipMode == 2) y = juce::jlimit (-0.98f, 0.98f, y);
            L[i] = y;
            if (R != nullptr) R[i] = y;
        }
        if (taps != nullptr) taps->push (tapK.data(), tapB.data(), n);

        // numbers for the screens
        float cur = guiPreClip.load (std::memory_order_relaxed);
        while (prePeak > cur && ! guiPreClip.compare_exchange_weak (cur, prePeak, std::memory_order_relaxed)) {}
        guiDc.store ((float) (dcSum / (double) juce::jmax (1, n)), std::memory_order_relaxed);
        guiCrushK.store (crushK, std::memory_order_relaxed);
        guiCrushB.store (crushB, std::memory_order_relaxed);
        guiBassAmp.store (bassAmp, std::memory_order_relaxed);
        guiBassNow.store (bassAmp > 1.0e-4f ? (float) lastBassF : 0.0f, std::memory_order_relaxed);
        guiSeqStep.store (seqStep, std::memory_order_relaxed);
        const Kick* newest = nullptr;
        for (auto& v : kicks) if (v.active && (newest == nullptr || v.t < newest->t)) newest = &v;
        guiKickHz.store (newest != nullptr ? (float) newest->curHz : 0.0f, std::memory_order_relaxed);
        guiKickMs.store (newest != nullptr ? (float) (newest->t * 1000.0) : 0.0f, std::memory_order_relaxed);
    }

    static constexpr int chunk = 256, ringSize = 64;
    int nowSample = 0;   // block position of what happens now (for the OWN SOUND hook)
    double sampleRate = 44100.0, bpm = 150.0, inc = 0.0, clock = 0.0, prevPpq = -1.0, local = -1.0, bassHz = 43.65, bassPhase = 0.0;
    bool playing = false, snapGain = true;
    RuthParams base, params;
    std::array<Kick, 3> kicks {};
    std::array<int, 16> held {};
    int numHeld = 0, lastNote = 53, seqStep = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os { std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true) };
    float osLatency = 0.0f;
    ruth::Svf lpK1, lpK2, lpB1, lpB2;
    float bassAmp = 0.0f, ampCoef = 0.1f, dcX = 0, dcY = 0, dcR = 0.9987f, crushK = 0.0f, crushB = 0.0f;
    float clickEnv = 0.0f, clickMul = 0.99f, hpA = 0.8f, hpX = 0.0f, hpY = 0.0f;
    uint32_t rng = 1234u;
    std::array<float, ringSize> ring {};
    int ringW = 0, clickDelay = 0;
    std::array<float, chunk> kbuf {}, bbuf {}, cbuf {}, tapK {}, tapB {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain { 1.0f };
};
