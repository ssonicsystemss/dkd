#pragma once
#include <array>
#include <atomic>
#include <cstdint>

// What the screens see of the sound: the kick and the bass as they leave the engine, as lock-free rings the audio
// thread writes and the editor reads (never the other way round).
//   raw    : the last 8192 samples of each, for the spectra
//   bucket : min / max of every 64 samples, 2048 buckets (about 3 s), for the waveform of one beat
struct Taps
{
    static constexpr int rawSize = 8192, buckets = 2048, bucketLen = 64;

    std::array<std::atomic<float>, rawSize> rawK {}, rawB {};
    std::atomic<int> rawW { 0 };
    std::array<std::atomic<float>, buckets> kMin {}, kMax {}, bMin {}, bMax {};
    std::atomic<int> bucketW { 0 };
    std::atomic<double> sampleRate { 48000.0 };

    // audio thread
    void push (const float* k, const float* b, int n)
    {
        int w = rawW.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            rawK[(size_t) w].store (k[i], std::memory_order_relaxed);
            rawB[(size_t) w].store (b[i], std::memory_order_relaxed);
            w = (w + 1) & (rawSize - 1);
        }
        rawW.store (w, std::memory_order_release);

        for (int i = 0; i < n; ++i)
        {
            if (accN == 0) { aKmin = aKmax = k[i]; aBmin = aBmax = b[i]; }
            else
            {
                aKmin = k[i] < aKmin ? k[i] : aKmin; aKmax = k[i] > aKmax ? k[i] : aKmax;
                aBmin = b[i] < aBmin ? b[i] : aBmin; aBmax = b[i] > aBmax ? b[i] : aBmax;
            }
            if (++accN == bucketLen)
            {
                const int bw = bucketW.load (std::memory_order_relaxed);
                kMin[(size_t) bw].store (aKmin, std::memory_order_relaxed); kMax[(size_t) bw].store (aKmax, std::memory_order_relaxed);
                bMin[(size_t) bw].store (aBmin, std::memory_order_relaxed); bMax[(size_t) bw].store (aBmax, std::memory_order_relaxed);
                bucketW.store ((bw + 1) % buckets, std::memory_order_release);
                accN = 0;
            }
        }
    }

    // editor: the last n samples (n <= rawSize), oldest first
    void copyRaw (float* k, float* b, int n) const
    {
        const int w = rawW.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
        {
            const size_t idx = (size_t) ((w - n + i + 2 * rawSize) & (rawSize - 1));
            k[i] = rawK[idx].load (std::memory_order_relaxed);
            b[i] = rawB[idx].load (std::memory_order_relaxed);
        }
    }

    // editor: the bucket that was written `ago` buckets before the newest one (0 = newest)
    bool bucketAgo (int ago, float& kmin, float& kmax, float& bmin, float& bmax) const
    {
        if (ago < 0 || ago >= buckets - 2) return false;
        const int w = bucketW.load (std::memory_order_acquire);
        const size_t idx = (size_t) ((w - 1 - ago + 2 * buckets) % buckets);
        kmin = kMin[idx].load (std::memory_order_relaxed); kmax = kMax[idx].load (std::memory_order_relaxed);
        bmin = bMin[idx].load (std::memory_order_relaxed); bmax = bMax[idx].load (std::memory_order_relaxed);
        return true;
    }

private:
    float aKmin = 0, aKmax = 0, aBmin = 0, aBmax = 0;
    int accN = 0;
};
