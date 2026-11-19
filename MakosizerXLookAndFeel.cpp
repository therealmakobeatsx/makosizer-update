#include "MakosizerXLookAndFeel.h"

#include <BinaryData.h>

#include <algorithm>
#include <cmath>

MakosizerXLookAndFeel::MakosizerXLookAndFeel()
{
    knobImage = juce::ImageCache::getFromMemory (
        BinaryData::dark_metal_knob_png,
        BinaryData::dark_metal_knob_pngSize);
    jassert (knobImage.isValid());
}

void MakosizerXLookAndFeel::drawRotarySlider (juce::Graphics& graphics,
                                               int x, int y, int width, int height,
                                               float sliderPosition,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider)
{
    const auto isDestroyKnob = slider.getName().equalsIgnoreCase ("Destroy");
    if (! isDestroyKnob)
        graphics.drawImage (knobImage, x, y, width, height,
                            0, 0, knobImage.getWidth(), knobImage.getHeight());

    const auto diameter = static_cast<float> (std::min (width, height));
    if (diameter <= 0.0f)
        return;

    auto centre = juce::Point<float> (x + width * 0.5f, y + height * 0.5f);
    if (isDestroyKnob)
        centre.y -= diameter * (8.0f / 220.0f);

    const auto position = std::isfinite (sliderPosition)
        ? juce::jlimit (0.0f, 1.0f, sliderPosition)
        : 0.0f;
    const auto angle = rotaryStartAngle
                       + position * (rotaryEndAngle - rotaryStartAngle)
                       - juce::MathConstants<float>::halfPi;
    const auto innerRadius = diameter * 0.275f;
    const auto outerRadius = diameter * 0.415f;
    const auto markerWidth = juce::jmax (2.0f, diameter * 0.034f);
    const juce::Point<float> markerStart (
        centre.x + std::cos (angle) * innerRadius,
        centre.y + std::sin (angle) * innerRadius);
    const juce::Point<float> markerEnd (
        centre.x + std::cos (angle) * outerRadius,
        centre.y + std::sin (angle) * outerRadius);

    juce::Path indicator;
    indicator.startNewSubPath (markerStart);
    indicator.lineTo (markerEnd);

    graphics.setColour (juce::Colour (0xff17130d));
    graphics.strokePath (indicator, juce::PathStrokeType (
        markerWidth + 1.6f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
    graphics.setColour (juce::Colour (0xffffd477));
    graphics.strokePath (indicator, juce::PathStrokeType (
        markerWidth, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
}
