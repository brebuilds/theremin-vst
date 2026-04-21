#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
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

    juce::Slider xSlider;
    juce::Slider ySlider;
    juce::ToggleButton handToggle;
    juce::Label xLabel { {}, "Debug X (pitch)" };
    juce::Label yLabel { {}, "Debug Y (volume)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessorEditor)
};
