#include "Check.h"
#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>

// Sets a parameter through its own normalisable range. This matters for the
// skewed "time" range (skew 0.25): a guessed normalised value would not land on
// the millisecond value we ask for.
static void setParamValue (SimpleDelayAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    CHECK (param != nullptr);

    if (param != nullptr)
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
}

// Reads the raw (denormalised) value the DSP actually sees.
static float rawValue (SimpleDelayAudioProcessor& p, const char* id)
{
    auto* v = p.apvts.getRawParameterValue (id);
    CHECK (v != nullptr);
    return v != nullptr ? v->load() : 0.0f;
}

int main()
{
    // Must be the first statement: this binary links the GUI modules through
    // juce_audio_utils, so JUCE's subsystems have to be started before any
    // AudioProcessor is constructed.
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --- mono in, stereo out: the channel with no input stays silent ---
    {
        SimpleDelayAudioProcessor p;
        p.setPlayConfigDetails (1, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();

        // Signal on the left only, and deliberately leave stale data on the
        // right, exactly as a host can hand us after a previous stereo block.
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample (0, i, 0.5f);
            buf.setSample (1, i, 0.7f);
        }

        juce::MidiBuffer midi;
        p.processBlock (buf, midi);

        for (int i = 0; i < 512; ++i)
            CHECK_NEAR (buf.getSample (1, i), 0.0, 1e-6);
    }

    // --- mix at 0 is dry passthrough (also catches a delay wired backwards) ---
    {
        SimpleDelayAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        setParamValue (p, SimpleDelayAudioProcessor::mixID, 0.0f);

        const float constant = 0.25f;
        juce::MidiBuffer midi;

        juce::AudioBuffer<float> buf (2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, constant);

        p.processBlock (buf, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                CHECK_NEAR (buf.getSample (ch, i), constant, 1e-6);
    }

    // --- mix at 1 with silence in gives silence out ---
    {
        SimpleDelayAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        setParamValue (p, SimpleDelayAudioProcessor::mixID, 1.0f);

        juce::MidiBuffer midi;
        const int blockSize = 512;
        const int numBlocks = (int) std::ceil (48000.0 / (double) blockSize);   // one second

        float peak = 0.0f;
        bool nonFinite = false;

        for (int b = 0; b < numBlocks; ++b)
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            buf.clear();
            p.processBlock (buf, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v))
                        nonFinite = true;
                    peak = std::max (peak, std::fabs (v));
                }
        }

        CHECK (! nonFinite);
        CHECK_NEAR (peak, 0.0, 1e-6);
    }

    // --- repeated prepareToPlay at a new sample rate ---
    {
        SimpleDelayAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 44100.0, 512);
        setParamValue (p, SimpleDelayAudioProcessor::mixID, 1.0f);       // fully wet
        setParamValue (p, SimpleDelayAudioProcessor::feedbackID, 0.0f);  // single echo

        // "time" is skewed, so go through its own range rather than guessing a
        // normalised value.
        auto* timeParam = dynamic_cast<juce::AudioParameterFloat*> (p.apvts.getParameter ("time"));
        CHECK (timeParam != nullptr);
        timeParam->setValueNotifyingHost (timeParam->range.convertTo0to1 (100.0f));

        p.prepareToPlay (44100.0, 512);

        juce::MidiBuffer midi;

        // Let the parameter smoothers settle before measuring.
        {
            juce::AudioBuffer<float> warm (2, 2048);
            warm.clear();
            p.processBlock (warm, midi);
        }

        // Impulse at 44.1 kHz: a 100 ms echo is 4410 samples.
        {
            juce::AudioBuffer<float> buf (2, 8192);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            p.processBlock (buf, midi);

            int peakIndex = 0;
            float peak = 0.0f;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                CHECK (std::isfinite (buf.getSample (0, i)));
                CHECK (std::isfinite (buf.getSample (1, i)));
                const float v = std::fabs (buf.getSample (0, i));
                if (v > peak) { peak = v; peakIndex = i; }
            }

            CHECK (peak > 0.5f);
            CHECK (std::abs (peakIndex - 4410) <= 4);
        }

        // Host calls prepareToPlay again at a different rate.
        p.prepareToPlay (96000.0, 512);

        // Let the smoothers settle again at the new rate.
        {
            juce::AudioBuffer<float> warm (2, 4096);
            warm.clear();
            p.processBlock (warm, midi);
        }

        // Impulse at 96 kHz: the same 100 ms is now 9600 samples.
        {
            juce::AudioBuffer<float> buf (2, 12000);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            p.processBlock (buf, midi);

            int peakIndex = 0;
            float peak = 0.0f;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                CHECK (std::isfinite (buf.getSample (0, i)));
                CHECK (std::isfinite (buf.getSample (1, i)));
                const float v = std::fabs (buf.getSample (0, i));
                if (v > peak) { peak = v; peakIndex = i; }
            }

            CHECK (peak > 0.5f);
            CHECK (std::abs (peakIndex - 9600) <= 4);
        }
    }

    // --- state round-trip: save on one processor, restore on a fresh one ---
    {
        SimpleDelayAudioProcessor a;
        setParamValue (a, SimpleDelayAudioProcessor::timeID, 123.0f);
        setParamValue (a, SimpleDelayAudioProcessor::feedbackID, 0.5f);
        setParamValue (a, SimpleDelayAudioProcessor::mixID, 0.7f);

        const float time     = rawValue (a, SimpleDelayAudioProcessor::timeID);
        const float feedback = rawValue (a, SimpleDelayAudioProcessor::feedbackID);
        const float mix      = rawValue (a, SimpleDelayAudioProcessor::mixID);

        // Sanity: we really did set the identifiable values.
        CHECK_NEAR (time,     123.0, 1e-3);
        CHECK_NEAR (feedback, 0.5,   1e-3);
        CHECK_NEAR (mix,      0.7,   1e-3);

        juce::MemoryBlock state;
        a.getStateInformation (state);
        CHECK (state.getSize() > 0);

        SimpleDelayAudioProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());

        CHECK_NEAR (rawValue (b, SimpleDelayAudioProcessor::timeID),     time,     1e-6);
        CHECK_NEAR (rawValue (b, SimpleDelayAudioProcessor::feedbackID), feedback, 1e-6);
        CHECK_NEAR (rawValue (b, SimpleDelayAudioProcessor::mixID),      mix,      1e-6);
    }

    return reportAndExit();
}
