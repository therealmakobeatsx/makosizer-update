#include "MakosizerXLookAndFeel.h"

#include <cmath>

void MakosizerXLookAndFeel::drawRotarySlider (juce::Graphics& graphics,
                                               int x, int y, int width, int height,
                                               float sliderPosition,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                 static_cast<float> (y),
                                                 static_cast<float> (width),
                                                 static_cast<float> (height));
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    if (diameter <= 0.0f)
        return;

    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    const auto pointerLength = diameter * 0.34f;
    const auto pointerX = centre.x + std::sin (angle) * pointerLength;
    const auto pointerY = centre.y - std::cos (angle) * pointerLength;

    graphics.setColour (juce::Colour (0x66ff3434));
    graphics.drawLine (centre.x, centre.y, pointerX, pointerY, 5.0f);
    graphics.setColour (juce::Colour (0xffff3434));
    graphics.drawLine (centre.x, centre.y, pointerX, pointerY, 2.2f);
}
