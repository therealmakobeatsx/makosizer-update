#pragma once

#include <JuceHeader.h>

class LCDDisplay final : public juce::Component
{
public:
    struct State
    {
        juce::String presetName { "INIT" };
        juce::String sampleRate { "44.1 kHz" };
        int oversamplingFactor = 1;
        juce::String bitDepth { "32-bit float" };
        juce::String status { "READY" };
    };

    LCDDisplay();

    void setState (State newState);
    const State& getState() const noexcept { return state; }

    void paint (juce::Graphics& graphics) override;

private:
    State state;
};
