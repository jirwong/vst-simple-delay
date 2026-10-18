#include "Check.h"
#include "DelayLine.h"

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

    return reportAndExit();
}
