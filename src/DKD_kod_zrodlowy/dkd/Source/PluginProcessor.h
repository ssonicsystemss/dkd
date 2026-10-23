#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamCache.h"
#include "Params.h"
#include "RutherfordEngine.h"
#include "OwnSound.h"

// lock-free-ish ring buffer of the output (the footer meter)
struct ScopeBuffer
{
    static constexpr int size = 8192;
    std::array<std::atomic<float>, size> data {};
    std::atomic<int> writePos { 0 };

    void push (const float* l, int n)
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) { data[(size_t) w].store (l[i], std::memory_order_relaxed); w = (w + 1) % size; }
        writePos.store (w, std::memory_order_release);
    }

    // the last n samples, oldest first (n <= size); for the editor's spectrum
    void copy (float* dst, int n) const
    {
        const int w = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dst[i] = data[(size_t) ((w - n + i + 2 * size) % size)].load (std::memory_order_relaxed);
    }

    float peak (int n) const
    {
        const int w = writePos.load (std::memory_order_acquire);
        float p = 0.0f;
        for (int i = 1; i <= n; ++i) p = juce::jmax (p, std::abs (data[(size_t) ((w - i + size) % size)].load (std::memory_order_relaxed)));
        return p;
    }
};

class RutherfordProcessor : public juce::AudioProcessor
{
public:
    RutherfordProcessor();
    ~RutherfordProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override { prepared.store (false); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return Branding::productName; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "U-150 Classic"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    RuthParams readParams() const;
    RutherfordEngine engine;

    juce::AudioProcessorValueTreeState apvts;
    Sonic::OwnSound own { { true, 60 } };   // OWN SOUND: the user's own sample
    ParamCache params { apvts };   // atomic<float>* of every parameter, resolved once (ParamCache.h)
    ScopeBuffer scope;
    Taps taps;   // the kick and the bass on their own, for the screens (Analysis.h)
    // editor keys, not saved: kick / bass mute while tweaking, TEST = a note F while the key is held
    std::atomic<bool> muteKick { false }, muteBass { false }, testHeld { false };

private:
    double lastBpm = 150.0;
    bool testOn = false;
    std::atomic<bool> prepared { false };   // processBlock is silent until prepareToPlay ran (hosts may call it early / during teardown)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RutherfordProcessor)
};
