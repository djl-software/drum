#pragma once
#include "PluginProcessor.h"
#include "MixerLook.h"

class DrumMixerPage final : public juce::Component, private juce::Timer
{
public:
    explicit DrumMixerPage(DrumAudioProcessor&);
    ~DrumMixerPage() override;
    void open();
    int widthForAllStrips() const{return (int)strips.size()*136+36;}
    std::function<void(juce::String)> onOpenInstrument;
    std::function<void()> onBack,onExit;
    juce::String selectedGroup() const{return selected;}
    void selectGroup(const juce::String& id){chooseGroup(id);}
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class Strip;
    void timerCallback() override;
    void refresh();
    void openGroupProcessing(const juce::String&);
    std::unique_ptr<juce::DocumentWindow> groupWindow;
    void chooseGroup(const juce::String&);
    DrumAudioProcessor& processor;
    DrumMixerLook look;
    juce::ComboBox view;
    juce::TextButton back{"Back"},close{"Exit"},help{"Ableton sends..."};
    juce::Viewport viewport;
    juce::Component canvas;
    std::vector<std::unique_ptr<Strip>> strips;
    std::vector<juce::String> viewHistory;std::vector<size_t> viewSessions;
    juce::String selected,topology;
    uint64_t revision=UINT64_MAX;
};
