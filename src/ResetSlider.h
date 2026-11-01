#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ValueBarLook : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* box=juce::LookAndFeel_V4::createSliderTextBox(slider);
        box->setJustificationType(juce::Justification::centredRight);
        box->setFont(juce::FontOptions(12));
        box->setBorderSize(juce::BorderSize<int>(0,5,0,7));
        return box;
    }
};

inline void useValueBar(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setSliderSnapsToMousePosition(false);
    slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff67dfc5).withAlpha(.3f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe5ebf5));
    slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff2b3648));
}

// JUCE's value label (and its live text editor) consumes double clicks before
// they reach Slider. Keep single-click typing and route just that gesture back.
class ResetSlider : public juce::Slider,public juce::AsyncUpdater
{
public:
    ResetSlider():listener(*this) {addMouseListener(&listener,true);}
    ~ResetSlider() override {cancelPendingUpdate();removeMouseListener(&listener);}
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if(e.eventComponent==this){juce::Slider::mouseDoubleClick(e);return;}
        if(isEnabled())triggerAsyncUpdate();
    }
private:
    void handleAsyncUpdate() override
    {
        if(!isEnabled())return;
        hideTextBox(true);
        const auto now=juce::Time::getCurrentTime();
        juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(),{},juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,this,this,now,{},now,2,false);
        juce::Slider::mouseDoubleClick(click);
    }
    struct Listener final : juce::MouseListener
    {
        explicit Listener(ResetSlider& s):owner(s) {}
        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            if(e.eventComponent!=&owner)owner.mouseDoubleClick(e);
        }
        ResetSlider& owner;
    } listener;
};
