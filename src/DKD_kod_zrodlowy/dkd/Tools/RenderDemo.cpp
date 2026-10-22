// Offline render + GUI screenshot test for rutherford (kick + reverse bass).
// Usage: RutherfordRender <out.wav> [gui.png]
// Checks: clean output, a kick on every beat, a silent gap before every kick, the bass in between at the right pitch
// (F -> 43.65 Hz, G -> 49 Hz), MIDI Note mode is a one-shot, the free clock with the transport stopped, note-off
// ends the bass, presets, state, user presets + window size, level independent of the sample rate.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "PluginEditor.h"
#include "OwnSoundTest.h"

using namespace juce;

namespace
{
    constexpr double sr = 48000.0, bpm = 150.0, beat = 60.0 / bpm;
    constexpr int block = 512, F3 = 53, G3 = 55;

    struct FakeHost : AudioPlayHead
    {
        bool playing = true;
        double ppq = 0.0;
        Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setIsPlaying (playing);
            p.setPpqPosition (ppq);
            p.setTimeSignature (TimeSignature { 4, 4 });
            return p;
        }
    };

    void setParam (RutherfordProcessor& proc, const char* id, float realValue)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }

    AudioBuffer<float> render (RutherfordProcessor& proc, double seconds, std::vector<std::pair<double, MidiMessage>> evs, bool playing)
    {
        FakeHost host;
        host.playing = playing;
        proc.setPlayHead (&host);
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        const int total = (int) (seconds * sr);
        AudioBuffer<float> out (2, total), buf (2, block);
        size_t ei = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = jmin (block, total - pos);
            host.ppq = pos / sr / beat;
            MidiBuffer midi;
            while (ei < evs.size() && evs[ei].first * sr < pos + n)
            {
                midi.addEvent (evs[ei].second, jlimit (0, n - 1, (int) (evs[ei].first * sr) - pos));
                ++ei;
            }
            AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
            proc.processBlock (view, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, view, ch, 0, n);
        }
        proc.setPlayHead (nullptr);
        return out;
    }

    double energy (const AudioBuffer<float>& b, double fromSec, double toSec)
    {
        double e = 0;
        const int a = jmax (0, (int) (fromSec * sr)), z = jmin (b.getNumSamples(), (int) (toSec * sr));
        for (int i = a; i < z; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
        return e / jmax (1, z - a);
    }

    float peakIn (const AudioBuffer<float>& b, double fromSec, double toSec)
    {
        float p = 0.0f;
        const int a = jmax (0, (int) (fromSec * sr)), z = jmin (b.getNumSamples(), (int) (toSec * sr));
        for (int i = a; i < z; ++i) p = jmax (p, std::abs (b.getSample (0, i)));
        return p;
    }

    // the strongest frequency between 30 and 70 Hz (Hann-windowed Goertzel scan in 0.05 Hz steps): the fundamental of
    // the distorted saw (zero crossings would also count its strong 2nd harmonic)
    double pitchIn (const AudioBuffer<float>& b, double fromSec, double toSec)
    {
        const int a = jmax (0, (int) (fromSec * sr)), z = jmin (b.getNumSamples(), (int) (toSec * sr)), n = z - a;
        double best = 0.0, bestMag = -1.0;
        for (double f = 30.0; f <= 70.0; f += 0.05)
        {
            const double w = 2.0 * std::cos (MathConstants<double>::twoPi * f / sr);
            double q1 = 0, q2 = 0;
            for (int i = 0; i < n; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (n - 1));
                const double q0 = w * q1 - q2 + b.getSample (0, a + i) * win;
                q2 = q1; q1 = q0;
            }
            const double mag = q1 * q1 + q2 * q2 - w * q1 * q2;
            if (mag > bestMag) { bestMag = mag; best = f; }
        }
        return best;
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String wavPath = argc > 1 ? argv[1] : "demo.wav";
    const String pngPath = argc > 2 ? argv[2] : "";
    bool ok = true;
    auto check = [&] (bool c, const String& what) { std::printf ("  [%s] %s\n", c ? " ok " : "FAIL", what.toRawUTF8()); ok = ok && c; };

    // ---- 1. the demo: F held for 16 beats, host playing
    RutherfordProcessor proc;
    const auto demo = render (proc, 16 * beat + 1.0, { { 0.0, MidiMessage::noteOn (1, F3, 0.9f) }, { 16 * beat, MidiMessage::noteOff (1, F3) } }, true);
    {
        int nan = 0; double sum = 0; float pk = 0.0f;
        for (int i = 0; i < demo.getNumSamples(); ++i)
        {
            const float v = demo.getSample (0, i);
            if (! std::isfinite (v)) { ++nan; continue; }
            pk = jmax (pk, std::abs (v)); sum += (double) v * v;
        }
        const double rms = std::sqrt (sum / demo.getNumSamples());
        std::printf ("Demo: peak %.3f rms %.3f NaN %d\n", pk, rms, nan);
        check (nan == 0 && pk < 1.0f && rms > 0.02, "demo clean and audible");
        int kicks = 0, gaps = 0, bass = 0;
        for (int b = 1; b < 15; ++b)
        {
            if (peakIn (demo, b * beat, b * beat + 0.015) > 0.3f) ++kicks;
            const double body = energy (demo, (b + 0.6) * beat, (b + 0.85) * beat), gap = energy (demo, (b + 1) * beat - 0.012, (b + 1) * beat - 0.002);
            if (body > 1.0e-4) ++bass;
            if (gap < body * 0.05) ++gaps;
        }
        check (kicks == 14, "a kick on every beat (" + String (kicks) + "/14)");
        check (bass == 14, "the reverse bass swells between the kicks (" + String (bass) + "/14)");
        check (gaps == 14, "a quiet gap before every kick (" + String (gaps) + "/14)");

        File f = File::getCurrentWorkingDirectory().getChildFile (wavPath);
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        if (w != nullptr) { w->writeFromAudioSampleBuffer (demo, 0, demo.getNumSamples()); std::printf ("WAV written: %s\n", f.getFullPathName().toRawUTF8()); }
    }

    // ---- 2. the bass pitch (a long, flat swell so the pitch can be counted): F -> 43.65 Hz, G -> 49.00 Hz
    for (auto [note, want] : { std::pair<int, double> { F3, 43.65 }, { G3, 49.0 } })
    {
        RutherfordProcessor pp;
        setParam (pp, PID::tail, 0.1f); setParam (pp, PID::bassStart, 0.3f); setParam (pp, PID::bassEnd, 1.0f);
        setParam (pp, PID::rise, 0.0f); setParam (pp, PID::curve, 0.8f);
        const auto b = render (pp, 5 * beat, { { 0.0, MidiMessage::noteOn (1, note, 0.9f) } }, true);
        const double hz = pitchIn (b, 3.35 * beat, 3.97 * beat);
        check (std::abs (hz - want) < 2.0, "bass pitch of note " + String (note) + ": " + String (hz, 2) + " Hz (want " + String (want, 2) + ")");
    }

    // ---- 3. MIDI Note mode: one kick + one beat of bass, then silence
    {
        RutherfordProcessor pp;
        setParam (pp, PID::trigger, 1.0f);
        const auto b = render (pp, 3 * beat, { { 0.0, MidiMessage::noteOn (1, F3, 0.9f) }, { 2 * beat, MidiMessage::noteOff (1, F3) } }, false);
        check (peakIn (b, 0.0, 0.015) > 0.3f && energy (b, 0.6 * beat, 0.85 * beat) > 1.0e-4 && energy (b, 1.3 * beat, 1.9 * beat) < 1.0e-6,
               "MIDI Note mode: one kick + one swell, then silence");
    }

    // ---- 4. host sync with the transport stopped: a free clock at the host tempo from the key press
    {
        RutherfordProcessor pp;
        const auto b = render (pp, 4.2 * beat, { { 0.05, MidiMessage::noteOn (1, F3, 0.9f) } }, false);
        int k = 0;
        for (int i = 0; i < 4; ++i) if (peakIn (b, 0.05 + i * beat, 0.05 + i * beat + 0.015) > 0.3f) ++k;
        check (k == 4, "stopped transport: kicks every beat from the key press (" + String (k) + "/4)");
    }

    // ---- 5. note-off ends it (the last kick fades on its own, the bass stops at once)
    {
        RutherfordProcessor pp;
        const auto b = render (pp, 4 * beat, { { 0.0, MidiMessage::noteOn (1, F3, 0.9f) }, { 2.5 * beat, MidiMessage::noteOff (1, F3) } }, true);
        check (energy (b, 2.6 * beat, 3.9 * beat) < 1.0e-6, "silence after note-off");
    }

    // ---- 6. presets: every ID exists, every preset clean
    bool presetsOk = true;
    {
        RutherfordProcessor idCheck;
        int missing = 0;
        for (auto& fp : factoryPresets())
            for (auto& [pid, v] : fp.values) if (idCheck.apvts.getParameter (pid) == nullptr) { ++missing; std::printf ("  unknown ID %s in %s\n", pid, fp.name); }
        check (missing == 0, "preset parameter IDs");
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            RutherfordProcessor pp;
            applyFactoryPreset (pp.apvts, i);
            const auto b = render (pp, 8 * beat, { { 0.0, MidiMessage::noteOn (1, F3, 0.9f) } }, true);
            int nan = 0; float pk = 0.0f; double sum = 0;
            for (int s = 0; s < b.getNumSamples(); ++s) { const float v = b.getSample (0, s); if (! std::isfinite (v)) ++nan; else { pk = jmax (pk, std::abs (v)); sum += (double) v * v; } }
            const double rms = std::sqrt (sum / b.getNumSamples());
            const bool good = nan == 0 && pk < 1.0f && rms > 0.01;
            presetsOk = presetsOk && good;
            std::printf ("  Preset %-18s peak %.3f rms %.3f %s\n", factoryPresets()[(size_t) i].name, pk, rms, good ? "OK" : "FAILED");
        }
    }

    // ---- 7. state save / restore
    {
        RutherfordProcessor a, b;
        setParam (a, PID::tail, 0.44f); setParam (a, PID::pattern, 2.0f); setParam (a, PID::kickDrive, 31.0f);
        MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        for (auto* q : a.getParameters())
            if (auto* r = dynamic_cast<RangedAudioParameter*> (q))
                same = same && std::abs (r->getValue() - b.apvts.getParameter (r->paramID)->getValue()) < 1.0e-5f;
        check (same, "state save / restore");
    }

    // ---- 8. user presets + resizable window
    {
        RutherfordProcessor up;
        auto* prm = up.apvts.getParameter (PID::kickDrive);
        prm->setValueNotifyingHost (0.123f);
        const float v0 = prm->getValue();
        const auto file = File::getSpecialLocation (File::tempDirectory).getChildFile ("fission_user_preset_test.fnpreset");
        bool upOk = Fiss::UserPresets::saveTo (up, file);
        prm->setValueNotifyingHost (0.9f);
        up.apvts.state.setProperty ("uiScale", 1.2, nullptr);
        upOk = upOk && Fiss::UserPresets::loadFrom (up, file, [&up] { return up.apvts.state; });
        upOk = upOk && std::abs (prm->getValue() - v0) < 1.0e-4f && std::abs ((double) up.apvts.state.getProperty ("uiScale") - 1.2) < 1.0e-6;
        file.deleteFile();
        up.apvts.state.setProperty ("uiScale", 1.0, nullptr);
        int w100 = 0, w75 = 0;
        { std::unique_ptr<AudioProcessorEditor> e (up.createEditor()); w100 = e->getWidth(); }
        up.apvts.state.setProperty ("uiScale", 0.75, nullptr);
        { std::unique_ptr<AudioProcessorEditor> e (up.createEditor()); w75 = e->getWidth(); upOk = upOk && e->isResizable(); }
        check (upOk && w100 > 0 && std::abs (w75 - 0.75 * w100) <= 2.0, "user preset save/load + 75 % window (" + String (w100) + " / " + String (w75) + " px)");
    }

    // ---- 9. GUI screenshot: F held, the beat 62 % through (the neutron on the inner circle)
    if (! Sonic::ownSoundSelfTest<RutherfordProcessor> (sr, block, 53, 2.0f)) { std::printf ("RESULT: FAIL\n"); return 1; }

    if (pngPath.isNotEmpty())
    {
        FakeHost host;
        proc.setPlayHead (&host);
        proc.prepareToPlay (sr, block);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        if (auto* e = dynamic_cast<DsLiteEditor*> (ed.get())) e->prepareScreenshot();
        AudioBuffer<float> buf (2, block);
        MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, F3, 0.9f), 0);
        const int blocks = (int) ((2.62 * beat) * sr / block);
        for (int i = 0; i < blocks; ++i) { host.ppq = i * block / sr / beat; proc.processBlock (buf, m); m.clear(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        proc.setPlayHead (nullptr);
        File pf = File::getCurrentWorkingDirectory().getChildFile (pngPath);
        pf.deleteFile();
        FileOutputStream os (pf);
        PNGImageFormat png;
        if (! png.writeImageToStream (img, os)) { std::printf ("ERROR: png\n"); return 3; }
        std::printf ("GUI screenshot: %s (%dx%d)\n", pf.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        if (auto* de = dynamic_cast<DsLiteEditor*> (ed.get()))   // the other touch-screen pages: name-page2.png ...
            for (int pg = 1; pg < 5; ++pg)
            {
                de->showPage (pg);
                auto pimg = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                File pp = File::getCurrentWorkingDirectory().getChildFile (pngPath.upToLastOccurrenceOf (".", false, false) + "-page" + String (pg + 1) + ".png");
                pp.deleteFile();
                FileOutputStream pos (pp);
                png.writeImageToStream (pimg, pos);
            }
    }

    // ---- 10. level independent of the sample rate
    {
        auto levelAt = [] (double rate)
        {
            RutherfordProcessor rp;
            rp.setPlayConfigDetails (0, 2, rate, 512);
            rp.prepareToPlay (rate, 512);
            const int len = (int) (1.6 * rate);
            AudioBuffer<float> rb (2, 512);
            double sum = 0.0; int cnt = 0;
            for (int pos = 0; pos < len; pos += 512)
            {
                MidiBuffer m;
                if (pos == 0) m.addEvent (MidiMessage::noteOn (1, F3, 0.9f), 0);
                rb.clear();
                rp.processBlock (rb, m);
                for (int i = 0; i < jmin (512, len - pos); ++i) { sum += (double) rb.getSample (0, i) * rb.getSample (0, i); ++cnt; }
            }
            return Decibels::gainToDecibels (std::sqrt (sum / jmax (1, cnt)));
        };
        const double l44 = levelAt (44100.0), l48 = levelAt (48000.0), l96 = levelAt (96000.0);
        std::printf ("Level vs sample rate: 44.1k %.2f, 48k %.2f, 96k %.2f dB\n", l44, l48, l96);
        check (std::abs (l44 - l48) < 0.5 && std::abs (l96 - l48) < 0.5, "level independent of the sample rate (+-0.5 dB)");
    }

    const bool all = ok && presetsOk;
    std::printf ("RESULT: %s\n", all ? "PASS" : "FAIL");
    return all ? 0 : 1;
}
