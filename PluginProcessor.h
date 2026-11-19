#pragma once

#include "../DSP/MakosizerXDSP.h"
#include "../PresetManager.h"

#include <JuceHeader.h>

#include <atomic>

class MakosizerXAudioProcessor final : public juce::AudioProcessor
{
public:
    using ParameterState = juce::AudioProcessorValueTreeState;

    struct OutputLevels
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    MakosizerXAudioProcessor();
    ~MakosizerXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    ParameterState& getParameterState() noexcept { return parameterState; }
    const ParameterState& getParameterState() const noexcept { return parameterState; }
    PresetManager& getPresetManager() noexcept { return presetManager; }
    int getOversamplingFactor() const noexcept;
    OutputLevels getAndResetOutputLevels() noexcept;

private:
    static ParameterState::ParameterLayout createParameterLayout();
    static void accumulatePeak (std::atomic<float>& peak, float level) noexcept;

    ParameterState parameterState;
    PresetManager presetManager;
    std::array<std::atomic<float>*, 14> parameterValues;
    std::atomic<float>* oversamplingValue = nullptr;
    MakosizerX::MakosizerXDSP dsp;
    std::atomic<float> leftOutputPeak { 0.0f };
    std::atomic<float> rightOutputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MakosizerXAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
