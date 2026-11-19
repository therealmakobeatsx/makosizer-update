#pragma once

#include <JuceHeader.h>

#include <atomic>

class LedMeter final : public juce::Component,
                       private juce::Timer
{
public:
    LedMeter();

    void setLevels (float leftLevel, float rightLevel) noexcept;
    void setMonoLevel (float level) noexcept;

    void paint (juce::Graphics& graphics) override;

private:
    struct ChannelState
    {
        float level = 0.0f;
        float peak = 0.0f;
        double peakHoldUntilMs = 0.0;
    };

    static float clampLevel (float level) noexcept;
    static bool updateChannel (ChannelState& channel, float targetLevel,
                               double nowMs, float elapsedSeconds) noexcept;
    void timerCallback() override;

    std::atomic<float> targetLeftLevel { 0.0f };
    std::atomic<float> targetRightLevel { 0.0f };
    ChannelState leftChannel;
    ChannelState rightChannel;
    double lastUpdateTimeMs = 0.0;
};
