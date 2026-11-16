#pragma once
#include "PluginProcessor.h"
#include "platform/HostFloat.h"
#include <juce_gui_basics/juce_gui_basics.h>

class DrumAudioProcessorEditor final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
    explicit DrumAudioProcessorEditor(DrumAudioProcessor&,bool companion=false);
    void copyPresentationFrom(const DrumAudioProcessorEditor&);
    void setHostedWindowSuppressed(bool);
    ~DrumAudioProcessorEditor() override;
    void resized() override;
private:
    class Workspace;
    std::unique_ptr<Workspace> workspace, libraryWorkspace;
    DrumAudioProcessor& processor;
    bool showingLibrary=false,companionView=false;
    djl::HostFloat hostFloat;
    void timerCallback() override;
    void showLibraryScreen(bool);
#if JUCE_MAC
    bool placementRestored=false;
    void rememberPresentation();
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAudioProcessorEditor)
};
