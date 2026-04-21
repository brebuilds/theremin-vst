#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "mapping/GestureMapper.h"

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
    buffer.clear();

    const int channel = static_cast<int>(apvts.getRawParameterValue(ThereminParams::channel)->load());
    const bool enabled = apvts.getRawParameterValue(ThereminParams::enabled)->load() > 0.5f;

    if (!enabled) {
        midiEmitter.panic(midi, channel);
        return;
    }

    theremin::GestureParams p;
    p.octaves = static_cast<int>(apvts.getRawParameterValue(ThereminParams::octaves)->load());
    p.magnetism = apvts.getRawParameterValue(ThereminParams::magnetism)->load();
    p.scaleKind = static_cast<theremin::ScaleKind>(
        static_cast<int>(apvts.getRawParameterValue(ThereminParams::scale)->load()));
    p.rootKey = static_cast<int>(apvts.getRawParameterValue(ThereminParams::key)->load());

    theremin::MidiState state;
    if (debugHandPresent.load()) {
        state = theremin::GestureMapper::map(debugX.load(), debugY.load(), p);
    } else {
        state = theremin::GestureMapper::inactive();
    }

    midiEmitter.apply(state, midi, channel);
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
