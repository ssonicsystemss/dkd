#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "OwnSound.h"   // OWN SOUND hook: the engine tells the user's sample when it starts a hit
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
    }

    float getLatency() const { return osLatency; }

    void setParams (const RuthParams& p)
    {
        if (p.trigger != params.trigger) { local = -1.0; fadeKicks(); }
        params = p;
        lpK1.set (7500.0f, (float) sampleRate, 0.54f); lpK2.set (7500.0f, (float) sampleRate, 1.31f);
        lpB1.set (p.bassToneHz, (float) sampleRate, 0.54f); lpB2.set (p.bassToneHz, (float) sampleRate, 1.31f);
        const float g = juce::Decibels::decibelsToGain (p.outputDb);
        if (snapGain) { outGain.setCurrentAndTargetValue (g); snapGain = false; } else outGain.setTargetValue (g);
        if (numHeld > 0) bassHz = ruth::noteHz (held[(size_t) numHeld - 1], p.fineCents);
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
    Sonic::OwnSound* own = nullptr;   // OWN SOUND: plays on every kick
    bool kickMuted = false;           // OWN SOUND in REPLACE: the synthesized kick + click are silent
    bool bassMuted = false;           // the editor's BASS key: the reverse bass is silent

private:
    struct Kick
    {
        bool active = false, fading = false;
        double t = 0.0, phase = 0.0, tailHz = 43.65, startHz = 1800.0, lenMs = 90.0, decayMs = 8.0;
        float fade = 1.0f;
    };

    void fadeKicks() { for (auto& v : kicks) if (v.active) v.fading = true; }

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
        const bool sync = params.trigger == 0;
        const double stepB = ruth::stepBeats (params.pattern), stepSec = stepB * 60.0 / bpm;
        for (int i = 0; i < n; ++i)
        {
            nowSample = start + i;
            if (sync && numHeld > 0 && std::floor (clock) > std::floor (prevPpq)) triggerKick();
            prevPpq = clock;

            double ph = -1.0;
            if (sync) { if (numHeld > 0) ph = clock; }
            else if (local >= 0.0) ph = local;
            float target = 0.0f, riseU = 0.0f;
            if (ph >= 0.0) { const double q = ph / stepB; target = ruth::swell (params, q - std::floor (q), stepSec, riseU); }
            bassAmp += (target - bassAmp) * ampCoef;
            float b = 0.0f;
            if (bassAmp > 1.0e-5f)
            {
                const double f = bassHz * std::pow (2.0, params.riseSt * riseU / 12.0), dt = f / sampleRate;
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
                c = hpY * clickEnv * params.click * 0.7f;
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
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            uk[i] = ruth::shape (params.kickType, uk[i] * gk);
            ub[i] = std::tanh (ub[i] * gb);
        }
        os->processSamplesDown (block);

        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float k = lpK2.lp (lpK1.lp (kbuf[(size_t) i])), b = lpB2.lp (lpB1.lp (bbuf[(size_t) i]));
            ring[(size_t) ringW] = cbuf[(size_t) i];   // the click skips the oversampler: delay it by its latency
            const float c = ring[(size_t) ((ringW - clickDelay + ringSize) % ringSize)];
            ringW = (ringW + 1) % ringSize;
            float y = (kickMuted ? 0.0f : k * params.kickLevel + c) + (bassMuted ? 0.0f : b * params.bassLevel);
            const float dc = y - dcX + dcR * dcY; dcX = y; dcY = dc; y = dc;
            y *= outGain.getNextValue();
            if (params.clipMode == 1)      y = std::tanh (y);
            else if (params.clipMode == 2) y = juce::jlimit (-0.98f, 0.98f, y);
            L[i] = y;
            if (R != nullptr) R[i] = y;
        }
    }

    static constexpr int chunk = 256, ringSize = 64;
    int nowSample = 0;   // block position of what happens now (for the OWN SOUND hook)
    double sampleRate = 44100.0, bpm = 150.0, inc = 0.0, clock = 0.0, prevPpq = -1.0, local = -1.0, bassHz = 43.65, bassPhase = 0.0;
    bool playing = false, snapGain = true;
    RuthParams params;
    std::array<Kick, 3> kicks {};
    std::array<int, 16> held {};
    int numHeld = 0, lastNote = 53;
    std::unique_ptr<juce::dsp::Oversampling<float>> os { std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true) };
    float osLatency = 0.0f;
    ruth::Svf lpK1, lpK2, lpB1, lpB2;
    float bassAmp = 0.0f, ampCoef = 0.1f, dcX = 0, dcY = 0, dcR = 0.9987f;
    float clickEnv = 0.0f, clickMul = 0.99f, hpA = 0.8f, hpX = 0.0f, hpY = 0.0f;
    uint32_t rng = 1234u;
    std::array<float, ringSize> ring {};
    int ringW = 0, clickDelay = 0;
    std::array<float, chunk> kbuf {}, bbuf {}, cbuf {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain { 1.0f };
};
