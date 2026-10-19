#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DelayLine.h"

#include <limits>
#include <vector>

class SimpleDelayAudioProcessor : public juce::AudioProcessor
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SimpleDelayAudioProcessor()
        : juce::AudioProcessor (BusesProperties()
                                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
    {
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // A feedback delay's tail is unbounded in principle, so report a genuine
    // infinity: JUCE maps only an infinite value to the VST3 infinite-tail
    // flag, while a large finite value is read as a sample count (and so would
    // still let hosts truncate offline renders and cut the echoes dead).
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* timeID     = "time";
    static constexpr const char* feedbackID = "feedback";
    static constexpr const char* mixID      = "mix";

private:
    std::vector<DelayLine> delayLines;
    juce::SmoothedValue<float> timeSmoother;   // in samples, not ms
    juce::SmoothedValue<float> mixSmoother;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleDelayAudioProcessor)
};
