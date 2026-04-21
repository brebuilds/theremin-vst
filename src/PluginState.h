#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ThereminParams {
    inline constexpr const char* octaves   = "octaves";    // int, 1..5
    inline constexpr const char* magnetism = "magnetism";  // float, 0..1
    inline constexpr const char* scale     = "scale";      // choice
    inline constexpr const char* key       = "key";        // choice, 0..11 (C..B)
    inline constexpr const char* channel   = "channel";    // int, 1..16
    inline constexpr const char* enabled   = "enabled";    // bool

    inline const juce::StringArray scaleNames {
        "Chromatic", "Major", "Minor", "Pentatonic Major", "Pentatonic Minor", "Blues"
    };

    inline const juce::StringArray keyNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> params;

        params.push_back(std::make_unique<AudioParameterInt>(
            ParameterID{ octaves, 1 }, "Octaves", 1, 5, 2));

        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{ magnetism, 1 }, "Magnetism",
            NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

        params.push_back(std::make_unique<AudioParameterChoice>(
            ParameterID{ scale, 1 }, "Scale", scaleNames, 0));

        params.push_back(std::make_unique<AudioParameterChoice>(
            ParameterID{ key, 1 }, "Key", keyNames, 0));

        params.push_back(std::make_unique<AudioParameterInt>(
            ParameterID{ channel, 1 }, "MIDI Channel", 1, 16, 1));

        params.push_back(std::make_unique<AudioParameterBool>(
            ParameterID{ enabled, 1 }, "Enabled", true));

        return { params.begin(), params.end() };
    }
}
