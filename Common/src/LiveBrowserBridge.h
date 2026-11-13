#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
// Native, in-process Live browser drop receiver; no helper executable.
class LiveBrowserBridge
{
public:
    explicit LiveBrowserBridge(juce::Component&);
    ~LiveBrowserBridge();
    void poll(const juce::String&,juce::Rectangle<int>);
    std::function<void(const juce::var&)> onDrop;
private:
    struct Impl;
    juce::Component& owner;
    std::unique_ptr<Impl> impl;
};