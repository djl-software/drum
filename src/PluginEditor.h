#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class DrumAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DrumAudioProcessorEditor(DrumAudioProcessor&);
    void paint(juce::Graphics&) override;
private:
    juce::Image logo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAudioProcessorEditor)
};
