#pragma once

#include <JuceHeader.h>

class LCDDisplay final : public juce::Component
{
public:
    struct State
    {
        juce::String presetName { "INIT" };
        float drive = 0.0f;
        float crush = 0.0f;
        float noise = 0.0f;
        float wow = 0.0f;
        float flutter = 0.0f;
        float outputLevel = 0.0f;
    };

    LCDDisplay();

    void setState (State newState);
    const State& getState() const noexcept { return state; }

    void paint (juce::Graphics& graphics) override;

private:
    static float clampParameter (float value) noexcept;

    State state;
};
