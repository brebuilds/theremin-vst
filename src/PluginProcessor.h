#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "PluginState.h"
#include "midi/MidiEmitter.h"

class ThereminAudioProcessor : public juce::AudioProcessor
{
public:
    ThereminAudioProcessor();
    ~ThereminAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> debugX { 0.5f };
    std::atomic<float> debugY { 0.5f };
    std::atomic<bool>  debugHandPresent { false };

private:
    theremin::MidiEmitter midiEmitter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessor)
};
