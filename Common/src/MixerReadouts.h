#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class MixerPeakReading final : public juce::TextButton
{
public:
    bool clipped=false;
    void paintButton(juce::Graphics& g,bool hover,bool) override {g.setColour(clipped?juce::Colour(0xfff1727f):juce::Colour(hover?0xffe5ebf5:0xff8d9bb0));g.setFont(juce::FontOptions(11));g.drawText(getButtonText(),getLocalBounds(),juce::Justification::centred);}
};
class MixerLevelReading final : public juce::Label,private juce::AsyncUpdater
{
public:
    std::function<void()> reset;
    MixerLevelReading():listener(*this){setFont(juce::FontOptions(11));setJustificationType(juce::Justification::centred);setEditable(true,false,false);setColour(juce::Label::textColourId,juce::Colour(0xffe5ebf5));setBorderSize({0,0,0,0});addMouseListener(&listener,true);}
    ~MixerLevelReading() override {cancelPendingUpdate();removeMouseListener(&listener);}
    void mouseDoubleClick(const juce::MouseEvent&) override {if(isBeingEdited())triggerAsyncUpdate();else if(reset)reset();}
private:
    void handleAsyncUpdate() override {hideEditor(true);if(reset)reset();}
    struct Listener final :juce::MouseListener {MixerLevelReading& owner;explicit Listener(MixerLevelReading& value):owner(value){}void mouseDoubleClick(const juce::MouseEvent& e) override {if(e.eventComponent!=&owner)owner.mouseDoubleClick(e);}} listener;
};
