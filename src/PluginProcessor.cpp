#include "PluginProcessor.h"
#include "PluginEditor.h"

ThereminAudioProcessor::ThereminAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Theremin", ThereminParams::createLayout())
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

void ThereminAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ThereminAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThereminAudioProcessor();
}
