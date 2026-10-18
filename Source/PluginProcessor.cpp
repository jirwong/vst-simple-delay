#include "PluginProcessor.h"

#include "PluginEditor.h"

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

void SimpleDelayAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    delayLines.resize ((size_t) numChannels);
    for (auto& line : delayLines)
        line.prepare (sampleRate, 2000.0f);        // spec §5.1: 2000 ms of capacity

    timeSmoother.reset (sampleRate, 0.02);         // 20 ms ramp
    mixSmoother.reset (sampleRate, 0.02);
}

void SimpleDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Channels with no input channel aliased to them hold stale data. Clear them.
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const float timeMs   = apvts.getRawParameterValue (timeID)->load();
    const float feedback = apvts.getRawParameterValue (feedbackID)->load();
    const float mix      = apvts.getRawParameterValue (mixID)->load();

    timeSmoother.setTargetValue (timeMs * 0.001f * (float) currentSampleRate);
    mixSmoother.setTargetValue (mix);

    const int numActive = juce::jmin (numIn, numOut);
    for (auto& line : delayLines)
        line.setFeedback (feedback);

    for (int i = 0; i < numSamples; ++i)
    {
        const float delayNow = timeSmoother.getNextValue();
        const float mixNow   = mixSmoother.getNextValue();

        for (int ch = 0; ch < numActive; ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            delayLines[(size_t) ch].setDelaySamples (delayNow);
            const float wet = delayLines[(size_t) ch].processSample (x[i]);
            x[i] = x[i] * (1.0f - mixNow) + wet * mixNow;
        }
    }
}

void SimpleDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SimpleDelayAudioProcessor::createEditor()
{
    return new SimpleDelayAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleDelayAudioProcessor();
}
