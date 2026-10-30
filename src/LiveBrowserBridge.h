#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// A Windows companion reads Live's drag origin outside the audio/host process.
class LiveBrowserBridge
{
public:
    explicit LiveBrowserBridge(juce::Component&);
    ~LiveBrowserBridge();
    void poll(const juce::String& context, juce::Rectangle<int> dropArea);
    std::function<void(const juce::var&)> onDrop;
private:
    juce::Component& owner;
    juce::ChildProcess process;
    juce::File folder;
    juce::String lastRequest;
    bool attempted=false;
};
