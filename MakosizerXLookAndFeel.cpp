#include "MakosizerXLookAndFeel.h"

#include <cmath>

namespace
{
const juce::Colour pointerRed (0xffff3434);

void drawRedGlow (juce::Graphics& graphics, juce::Point<float> centre,
                  float radius, float intensity)
{
    for (int layer = 3; layer > 0; --layer)
    {
        const auto glowRadius = radius + 3.0f + static_cast<float> (layer) * 2.0f;
        const auto alpha = 0.025f * intensity * static_cast<float> (4 - layer);
        graphics.setColour (pointerRed.withAlpha (alpha));
        graphics.drawEllipse (centre.x - glowRadius, centre.y - glowRadius,
                              glowRadius * 2.0f, glowRadius * 2.0f,
                              static_cast<float> (layer + 1));
    }
}
}

void MakosizerXLookAndFeel::drawRotarySlider (juce::Graphics& graphics,
                                               int x, int y, int width, int height,
                                               float sliderPosition,
                                               float rotaryStartAngle,
                                               float rotaryEndAngle,
                                               juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                 static_cast<float> (y),
                                                 static_cast<float> (width),
                                                 static_cast<float> (height));
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    if (diameter <= 0.0f)
        return;

    const auto centre = bounds.getCentre();
    const auto isDestroy = slider.getName().equalsIgnoreCase ("Destroy");
    const auto knobRadius = diameter * (isDestroy ? 0.39f : 0.38f);
    const auto intensity = juce::jlimit (0.0f, 1.0f, sliderPosition);
    const auto outerRadius = knobRadius + (isDestroy ? 8.0f : 5.0f);

    if (isDestroy)
    {
        drawRedGlow (graphics, centre, outerRadius, 0.75f + intensity * 0.25f);
    }
    else if (intensity > 0.015f)
    {
        drawRedGlow (graphics, centre, outerRadius, intensity);
    }

    auto outerBounds = juce::Rectangle<float> (centre.x - outerRadius, centre.y - outerRadius,
                                                outerRadius * 2.0f, outerRadius * 2.0f);
    juce::ColourGradient steelRim (juce::Colour (0xffd3d1c5), centre.x, outerBounds.getY(),
                                   juce::Colour (0xff292d2a), centre.x, outerBounds.getBottom(),
                                   false);
    steelRim.addColour (0.42, juce::Colour (0xff777b74));
    graphics.setGradientFill (steelRim);
    graphics.fillEllipse (outerBounds);
    graphics.setColour (juce::Colour (0xff121512));
    graphics.drawEllipse (outerBounds, 1.5f);

    if (isDestroy)
    {
        graphics.setColour (pointerRed.withAlpha (0.62f + intensity * 0.30f));
        graphics.drawEllipse (outerBounds, 3.0f);
    }

    auto knobBounds = juce::Rectangle<float> (centre.x - knobRadius, centre.y - knobRadius,
                                               knobRadius * 2.0f, knobRadius * 2.0f);
    juce::ColourGradient brushedMetal (juce::Colour (0xffc4c4bb), centre.x, knobBounds.getY(),
                                       juce::Colour (0xff3c403b), centre.x,
                                       knobBounds.getBottom(), false);
    brushedMetal.addColour (0.22, juce::Colour (0xff858982));
    brushedMetal.addColour (0.48, juce::Colour (0xffb0b1a8));
    brushedMetal.addColour (0.72, juce::Colour (0xff686c65));
    graphics.setGradientFill (brushedMetal);
    graphics.fillEllipse (knobBounds);

    graphics.setColour (juce::Colour (0x66ffffff));
    graphics.drawEllipse (knobBounds.reduced (1.0f), 1.0f);

    for (int line = 0; line < 4; ++line)
    {
        const auto radius = knobRadius * (0.80f - static_cast<float> (line) * 0.08f);
        juce::Path brushMark;
        brushMark.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                 juce::MathConstants<float>::pi * 1.16f,
                                 juce::MathConstants<float>::pi * 1.84f, true);
        graphics.setColour (juce::Colour (line % 2 == 0 ? 0x38ffffff : 0x30000000));
        graphics.strokePath (brushMark, juce::PathStrokeType (0.8f));
    }

    const auto capRadius = knobRadius * 0.57f;
    auto capBounds = juce::Rectangle<float> (centre.x - capRadius, centre.y - capRadius,
                                              capRadius * 2.0f, capRadius * 2.0f);
    juce::ColourGradient darkCap (juce::Colour (0xff535650), centre.x, capBounds.getY(),
                                  juce::Colour (0xff101311), centre.x,
                                  capBounds.getBottom(), false);
    graphics.setGradientFill (darkCap);
    graphics.fillEllipse (capBounds);
    graphics.setColour (juce::Colour (0xff090b09));
    graphics.drawEllipse (capBounds, 1.2f);

    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    const auto pointerLength = knobRadius * (isDestroy ? 0.82f : 0.78f);
    const auto pointerX = centre.x + std::sin (angle) * pointerLength;
    const auto pointerY = centre.y - std::cos (angle) * pointerLength;
    graphics.setColour (pointerRed.withAlpha (0.28f));
    graphics.drawLine (centre.x, centre.y, pointerX, pointerY,
                       isDestroy ? 9.0f : 6.0f);
    graphics.setColour (pointerRed);
    graphics.drawLine (centre.x, centre.y, pointerX, pointerY,
                       isDestroy ? 4.0f : 2.8f);

    const auto hubRadius = knobRadius * 0.09f;
    graphics.setColour (juce::Colour (0xffaeb0a7));
    graphics.fillEllipse (centre.x - hubRadius, centre.y - hubRadius,
                          hubRadius * 2.0f, hubRadius * 2.0f);
    graphics.setColour (juce::Colour (0xff292c28));
    graphics.drawEllipse (centre.x - hubRadius, centre.y - hubRadius,
                          hubRadius * 2.0f, hubRadius * 2.0f, 1.0f);
}
