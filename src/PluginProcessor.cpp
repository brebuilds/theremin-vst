#include "PluginProcessor.h"
#include "PluginEditor.h"

ThereminAudioProcessor::ThereminAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ThereminAudioProcessor::prepareToPlay(double, int) {}
void ThereminAudioProcessor::releaseResources() {}

void ThereminAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();  // zero audio; we only emit MIDI
    juce::ignoreUnused(midi);
}

juce::AudioProcessorEditor* ThereminAudioProcessor::createEditor()
{
    return new ThereminAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThereminAudioProcessor();
}
