#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// A single-channel fractional delay line with feedback.
//
// The line is a circular buffer. Each slot does not hold "the signal"; it
// holds "the signal plus its own last echo". Reading before writing in
// processSample is the whole trick: the value read is fed back into the write,
// so a single tap and one multiply produce an unending repeat.
class DelayLine
{
public:
    void prepare (double sampleRate, float maxDelayMs)
    {
        // The extra 4 slots are deliberate headroom: together with the ceiling
        // in setDelaySamples they keep the interpolating read clear of the slot
        // being written this same sample. See setDelaySamples.
        bufferSize = (int) std::ceil (sampleRate * maxDelayMs / 1000.0) + 4;
        buffer.assign ((size_t) bufferSize, 0.0f);
        writeIndex = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void setDelaySamples (float samples)
    {
        // Because the buffer is sized + 4 and the delay is clamped 4 short of
        // its end, i1 == writeIndex can only happen when frac == 0. The read
        // therefore never weights the slot it is about to overwrite in the same
        // sample. min/max is used rather than std::clamp so that a call before
        // prepare() (lo > hi) is merely odd, not undefined.
        delaySamples = std::min (std::max (samples, 1.0f), (float) (bufferSize - 4));
    }

    void setFeedback (float fb)
    {
        feedback = std::clamp (fb, 0.0f, 0.95f);
    }

    float processSample (float input)
    {
        // Read before write. The read must see what was written one delay ago
        // and nothing newer, and writing last means the slot we just read is
        // overwritten only after we have used it. Feeding the read value back
        // (input + out * feedback) is what turns one tap into an unending echo.
        const float readIndexF = wrap ((float) writeIndex - delaySamples);
        const int   i0   = (int) readIndexF;
        // i0 can be the buffer's final slot whenever the fractional read
        // position lands there, for any write index, so i0 + 1 can reach
        // bufferSize and must wrap. At the closest approach (delay exactly 1.0)
        // i0 is the slot before the write head, which is the case that matters:
        // i1 is then the write index itself.
        const int   i1   = (i0 + 1) % bufferSize;
        const float frac = readIndexF - (float) i0;
        const float out  = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
        buffer[(size_t) writeIndex] = input + out * feedback;
        writeIndex = (writeIndex + 1) % bufferSize;
        return out;
    }

    int          getBufferSize() const noexcept { return bufferSize; }
    const float* bufferData()    const noexcept { return buffer.data(); }

private:
    float wrap (float value)
    {
        while (value < 0.0f)
            value += (float) bufferSize;
        return value;
    }

    std::vector<float> buffer;
    int                bufferSize = 0;
    int                writeIndex = 0;
    float              delaySamples = 0.0f;
    float              feedback = 0.0f;
};
