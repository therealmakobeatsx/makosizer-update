#include "MakosizerXLookAndFeel.h"

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
    juce::ignoreUnused (sliderPosition, rotaryStartAngle, rotaryEndAngle, slider);
    if (slider.getName().equalsIgnoreCase ("Destroy"))
        return;

    graphics.drawImage (knobImage, x, y, width, height,
                        0, 0, knobImage.getWidth(), knobImage.getHeight());
}
