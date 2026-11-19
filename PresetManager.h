#pragma once

#include <JuceHeader.h>

#include <vector>

class PresetManager final
{
public:
    using ParameterState = juce::AudioProcessorValueTreeState;

    struct FactoryPresetInfo
    {
        juce::String name;
        juce::String description;
    };

    explicit PresetManager (ParameterState& state) noexcept;

    static std::vector<FactoryPresetInfo> getFactoryPresets();

    juce::Result loadFactoryPreset (const juce::String& presetName);
    void markCurrentPresetCustom();
    juce::Result savePreset (const juce::File& file, const juce::String& presetName);
    juce::Result loadPreset (const juce::File& file);

    juce::String getCurrentPresetName() const;

    static constexpr const char* presetFileExtension = ".makopreset";

private:
    ParameterState& parameterState;
};
