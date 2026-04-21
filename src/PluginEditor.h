#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ThereminAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ThereminAudioProcessorEditor(ThereminAudioProcessor& p);
    ~ThereminAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ThereminAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessorEditor)
};
