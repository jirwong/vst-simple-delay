#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

class DelayLine
{
public:
    void prepare (double sampleRate, float maxDelayMs)
    {
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
        delaySamples = std::clamp (samples, 1.0f, (float) (bufferSize - 4));
    }

    void setFeedback (float fb)
    {
        feedback = std::clamp (fb, 0.0f, 0.95f);
    }

    float processSample (float input)
    {
        const float readIndexF = wrap ((float) writeIndex - delaySamples);
        const int   i0   = (int) readIndexF;
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
