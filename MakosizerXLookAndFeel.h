#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MakosizerXLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MakosizerXLookAndFeel();

    void drawRotarySlider (juce::Graphics& graphics, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

private:
    juce::Image knobImage;
};
