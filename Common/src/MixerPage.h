#pragma once
#include "PluginProcessor.h"
#include "MixerLook.h"

class DrumMixerPage final : public juce::Component, private juce::Timer
{
public:
    explicit DrumMixerPage(DrumAudioProcessor&);
    ~DrumMixerPage() override;
    void open();
    std::function<void(juce::String)> onOpenInstrument;
    juce::String selectedGroup() const{return selected;}
    void selectGroup(const juce::String& id){chooseGroup(id);}
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class Strip;
    void timerCallback() override;
    void refresh();
    void chooseGroup(const juce::String&);
    DrumAudioProcessor& processor;
    DrumMixerLook look;
    juce::ComboBox view;
    juce::TextButton close{"Back to kit"},help{"Ableton sends..."};
    juce::Viewport viewport;
    juce::Component canvas;
    std::vector<std::unique_ptr<Strip>> strips;
    juce::String selected,topology;
    uint64_t revision=UINT64_MAX;
};
