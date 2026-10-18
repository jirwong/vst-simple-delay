#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// Three rotary sliders, one per parameter. Each slider is kept in sync with its
// parameter by an AudioProcessorValueTreeState::SliderAttachment, which must
// live as long as the slider does — hence the unique_ptr members.
class SimpleDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SimpleDelayAudioProcessorEditor (SimpleDelayAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SimpleDelayAudioProcessor& processor;

    juce::Slider timeSlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;

    juce::Label timeLabel;
    juce::Label feedbackLabel;
    juce::Label mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleDelayAudioProcessorEditor)
};
