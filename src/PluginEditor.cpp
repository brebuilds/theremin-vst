#include "PluginEditor.h"

ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);

    xSlider.setRange(0.0, 1.0, 0.001);
    xSlider.setValue(0.5);
    xSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    xSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    xSlider.onValueChange = [this] {
        processorRef.debugX.store(static_cast<float>(xSlider.getValue()));
    };
    addAndMakeVisible(xSlider);
    addAndMakeVisible(xLabel);
    xLabel.setColour(juce::Label::textColourId, juce::Colour(0xff22d3ee));

    ySlider.setRange(0.0, 1.0, 0.001);
    ySlider.setValue(0.5);
    ySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    ySlider.onValueChange = [this] {
        processorRef.debugY.store(static_cast<float>(ySlider.getValue()));
    };
    addAndMakeVisible(ySlider);
    addAndMakeVisible(yLabel);
    yLabel.setColour(juce::Label::textColourId, juce::Colour(0xff22d3ee));

    handToggle.setButtonText("Hand present (debug)");
    handToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff22d3ee));
    handToggle.onStateChange = [this] {
        processorRef.debugHandPresent.store(handToggle.getToggleState());
    };
    addAndMakeVisible(handToggle);
}

void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(32.0f)));
    g.drawFittedText("THEREMIN", getLocalBounds().removeFromTop(80),
                     juce::Justification::centred, 1);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawFittedText("Debug wiring — camera comes later.",
                     getLocalBounds().removeFromTop(110).withTrimmedTop(80),
                     juce::Justification::centred, 1);
}

void ThereminAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(120); // title area

    auto xRow = bounds.removeFromTop(30);
    xLabel.setBounds(xRow.removeFromLeft(140));
    xSlider.setBounds(xRow);
    bounds.removeFromTop(10);

    auto yRow = bounds.removeFromTop(30);
    yLabel.setBounds(yRow.removeFromLeft(140));
    ySlider.setBounds(yRow);
    bounds.removeFromTop(20);

    handToggle.setBounds(bounds.removeFromTop(30));
}
