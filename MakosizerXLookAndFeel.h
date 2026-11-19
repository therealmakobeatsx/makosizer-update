#pragma once

#include <JuceHeader.h>

class MakosizerXLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& graphics, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
};
