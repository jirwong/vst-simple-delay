#include "Check.h"
#include "DelayLine.h"

#include <algorithm>
#include <vector>

static constexpr double kSampleRate = 48000.0;
static constexpr float  kMaxDelayMs = 2000.0f;

// Sends an impulse (1.0 then zeros) through the line and returns the output.
static std::vector<float> impulseResponse (DelayLine& line, int numSamples)
{
    std::vector<float> out ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        out[(size_t) i] = line.processSample (i == 0 ? 1.0f : 0.0f);
    return out;
}

int main()
{
    // --- the delay is the length it claims ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 400);

        for (int i = 0; i < 100; ++i)
            CHECK_NEAR (out[(size_t) i], 0.0, 1e-9);
        CHECK_NEAR (out[100], 1.0, 1e-6);
    }

    // --- feedback math produces a geometric decay, not just "several echoes" ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.5f);
        auto out = impulseResponse (line, 500);

        CHECK_NEAR (out[100], 1.0,  1e-6);   // the impulse itself
        CHECK_NEAR (out[200], 0.5,  1e-6);
        CHECK_NEAR (out[300], 0.25, 1e-6);
        CHECK_NEAR (out[400], 0.125, 1e-6);
    }

    // --- an out-of-range delay time cannot read outside the buffer ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (1.0e9f);            // absurd, as automation might send
        line.setFeedback (0.5f);
        auto out = impulseResponse (line, 1000);
        for (auto v : out) CHECK (std::isfinite (v));

        line.setDelaySamples (0.0f);              // and the other extreme
        auto out2 = impulseResponse (line, 1000);
        for (auto v : out2) CHECK (std::isfinite (v));
    }

    // --- feedback above the cap is clamped, and the cap is real ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (5.0f);                  // a bad automation value
        auto out = impulseResponse (line, 500);
        CHECK_NEAR (out[100], 1.0,  1e-6);
        CHECK_NEAR (out[200], 0.95, 1e-6);        // clamped, not 5.0
    }

    // --- reset clears state ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.8f);
        impulseResponse (line, 2000);

        line.reset();
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 300);
        for (int i = 0; i < 300; ++i)
            CHECK_NEAR (out[(size_t) i], i == 100 ? 1.0 : 0.0, 1e-9);
    }

    // --- nothing allocates in the audio path ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        const float* before = line.bufferData();
        const int    sizeBefore = line.getBufferSize();
        for (int i = 0; i < 1000; ++i)
            line.processSample (0.1f);
        CHECK (line.bufferData() == before);
        CHECK (line.getBufferSize() == sizeBefore);
    }

    // --- a fractional delay splits energy across the two adjacent samples ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.5f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 300);

        CHECK_NEAR (out[100], 0.5, 1e-3);
        CHECK_NEAR (out[101], 0.5, 1e-3);
        CHECK_NEAR (out[100] + out[101], 1.0, 1e-3);   // energy is preserved
        CHECK_NEAR (out[99],  0.0, 1e-3);
        CHECK_NEAR (out[102], 0.0, 1e-3);
    }

    // --- the delay is the same number of samples regardless of sample rate ---
    {
        DelayLine line;
        line.prepare (96000.0, kMaxDelayMs);
        line.setDelaySamples (1000.0f);
        line.setFeedback (0.0f);
        auto out = impulseResponse (line, 2000);

        CHECK_NEAR (out[999],  0.0, 1e-9);
        CHECK_NEAR (out[1000], 1.0, 1e-6);
    }

    // --- maximum feedback decays instead of running away ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setDelaySamples (100.0f);
        line.setFeedback (0.95f);

        float peak = 0.0f, tail = 0.0f;
        int nonFinite = 0;
        const int total = (int) (kSampleRate * 10.0);   // ten seconds
        for (int i = 0; i < total; ++i)
        {
            const float v = line.processSample (i == 0 ? 1.0f : 0.0f);
            peak = std::max (peak, std::fabs (v));
            tail = std::fabs (v);
            if (! std::isfinite (v))
                ++nonFinite;
        }
        CHECK (std::isfinite (peak));
        CHECK (peak < 2.0f);
        CHECK (tail < 1e-4f);
        CHECK (nonFinite == 0);
    }

    // --- moving the delay time mid-run produces no NaN or infinity ---
    {
        DelayLine line;
        line.prepare (kSampleRate, kMaxDelayMs);
        line.setFeedback (0.5f);
        for (int i = 0; i < 48000; ++i)
        {
            line.setDelaySamples (1.0f + (float) i);     // sweep across the whole range
            CHECK (std::isfinite (line.processSample (0.5f)));
        }
    }

    return reportAndExit();
}
