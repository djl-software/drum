#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class DrumAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DrumAudioProcessorEditor(DrumAudioProcessor&);
    ~DrumAudioProcessorEditor() override;
    void resized() override;
private:
    class Workspace;
    std::unique_ptr<Workspace> workspace, libraryWorkspace;
    DrumAudioProcessor& processor;
    bool showingLibrary=false;
    void showLibraryScreen(bool);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAudioProcessorEditor)
};
