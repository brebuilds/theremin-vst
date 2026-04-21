#include "PluginEditor.h"

ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);
}

void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(32.0f)));
    g.drawFittedText("THEREMIN", getLocalBounds(), juce::Justification::centred, 1);
}

void ThereminAudioProcessorEditor::resized() {}
