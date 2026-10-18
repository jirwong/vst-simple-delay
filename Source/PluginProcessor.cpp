#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

juce::AudioProcessorValueTreeState::ParameterLayout SimpleDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { timeID, 1 },
        "Time",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.01f, 0.25f),
        375.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackID, 1 },
        "Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f),
        0.40f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixID, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.35f));

    return { params.begin(), params.end() };
}

bool SimpleDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet().size() <= layouts.getMainOutputChannelSet().size();
}

void SimpleDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // TEMPORARY scaffold: replaced by the delay line in Task 4.
    const auto timeMs = apvts.getRawParameterValue (timeID)->load();
    const auto gain   = juce::jlimit (0.0f, 1.0f, timeMs / 1000.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.applyGain (ch, 0, buffer.getNumSamples(), gain);
}

juce::AudioProcessorEditor* SimpleDelayAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleDelayAudioProcessor();
}
