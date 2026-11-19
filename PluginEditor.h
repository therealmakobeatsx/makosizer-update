#pragma once

#include "../GUI/LCDDisplay.h"
#include "../GUI/LedMeter.h"
#include "../GUI/MakosizerXLookAndFeel.h"

#include <JuceHeader.h>

#include <array>
#include <memory>

class MakosizerXAudioProcessor;

class MakosizerXAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Slider::Listener,
                                             private juce::Timer
{
public:
    explicit MakosizerXAudioProcessorEditor (MakosizerXAudioProcessor& processor);
    ~MakosizerXAudioProcessorEditor() override;

    void paint (juce::Graphics& graphics) override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void sliderDragStarted (juce::Slider* slider) override;
    void timerCallback() override;
    void updateLCDDisplay();
    void updatePresetSelection();
    void updateOversamplingSelection();
    void loadPreset (const juce::String& presetName);
    void openPresetSaveDialog();
    void openPresetLoadDialog();
    void savePresetToFile (const juce::File& file);
    void loadPresetFromFile (const juce::File& file);
    void setOversamplingMode (int choiceIndex);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    MakosizerXAudioProcessor& audioProcessor;
    juce::Image backgroundImage;
    MakosizerXLookAndFeel rotaryLookAndFeel;
    std::array<juce::Slider, 14> sliders;
    std::array<std::unique_ptr<SliderAttachment>, 14> attachments;
    std::array<std::unique_ptr<juce::Button>, 9> presetButtons;
    std::array<std::unique_ptr<juce::Button>, 2> presetFileButtons;
    std::array<std::unique_ptr<juce::Button>, 4> oversamplingButtons;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    LCDDisplay lcdDisplay;
    LedMeter ledMeter;
    float outputLevel = 0.0f;
};
