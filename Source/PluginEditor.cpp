#include "PluginEditor.h"

namespace
{
    void setUpSlider (juce::Slider& slider, int numDecimalPlaces)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setNumDecimalPlacesToDisplay (numDecimalPlaces);
    }

    void setUpLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
    }
}

SimpleDelayAudioProcessorEditor::SimpleDelayAudioProcessorEditor (SimpleDelayAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setUpSlider (timeSlider,     2);
    setUpSlider (feedbackSlider, 3);
    setUpSlider (mixSlider,      3);

    setUpLabel (timeLabel,     "Time");
    setUpLabel (feedbackLabel, "Feedback");
    setUpLabel (mixLabel,      "Mix");

    addAndMakeVisible (timeLabel);
    addAndMakeVisible (feedbackLabel);
    addAndMakeVisible (mixLabel);

    addAndMakeVisible (timeSlider);
    addAndMakeVisible (feedbackSlider);
    addAndMakeVisible (mixSlider);

    timeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, SimpleDelayAudioProcessor::timeID, timeSlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, SimpleDelayAudioProcessor::feedbackID, feedbackSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, SimpleDelayAudioProcessor::mixID, mixSlider);

    setSize (420, 200);
}

void SimpleDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void SimpleDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    const int labelHeight = 20;
    const int columnWidth = area.getWidth() / 3;

    juce::Label* labels[]   = { &timeLabel, &feedbackLabel, &mixLabel };
    juce::Slider* sliders[] = { &timeSlider, &feedbackSlider, &mixSlider };

    for (int i = 0; i < 3; ++i)
    {
        // The last column takes whatever width is left, so rounding loses nothing.
        auto column = (i == 2) ? area : area.removeFromLeft (columnWidth);

        labels[i]->setBounds (column.removeFromTop (labelHeight));
        sliders[i]->setBounds (column.reduced (4));
    }
}
