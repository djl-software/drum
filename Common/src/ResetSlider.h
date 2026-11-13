#pragma once
#include "DjlRotaryControl.h"
#include "ThreeDControls.h"

inline void drawControlLabel(juce::Graphics& g,const juce::String& name,juce::Rectangle<int> area)
{
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14));
    g.drawFittedText(name,area,juce::Justification::centredRight,1);
}

class ValueBarLook : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool hover,bool down) override {drawDjlButton3D(g,b,colour,hover,down);}
    void drawTickBox(juce::Graphics& g,juce::Component&,float x,float y,float w,float h,bool ticked,bool enabled,bool hover,bool down) override {
        const float alpha=enabled?1.f:.35f;auto area=juce::Rectangle<float>(x,y,w,h).reduced(1.5f);auto colour=juce::Colour(ticked?0xff23584f:0xff253243);if(hover)colour=colour.brighter(.12f);
        g.setGradientFill(juce::ColourGradient(colour.darker(.3f).withAlpha(alpha),x,y,colour.brighter(down?0.f:.25f).withAlpha(alpha),x,y+h,false));g.fillRoundedRectangle(area,3);g.setColour(juce::Colour(ticked?0xff67dfc5:0xff728094).withAlpha(alpha));g.drawRoundedRectangle(area,3,1);
        if(ticked){juce::Path tick;tick.startNewSubPath(area.getX()+area.getWidth()*.2f,area.getCentreY());tick.lineTo(area.getX()+area.getWidth()*.43f,area.getY()+area.getHeight()*.73f);tick.lineTo(area.getX()+area.getWidth()*.82f,area.getY()+area.getHeight()*.25f);g.setColour(juce::Colour(0xffe5fff8).withAlpha(alpha));g.strokePath(tick,juce::PathStrokeType(1.8f));}
    }
    void drawComboBox(juce::Graphics& g,int width,int height,bool down,int,int,int,int,juce::ComboBox& box) override {
        const auto colour=box.findColour(juce::ComboBox::backgroundColourId);auto area=juce::Rectangle<float>(.5f,.5f,width-1.f,height-1.f);g.setGradientFill(juce::ColourGradient(colour.brighter(down?0.f:.18f),0,0,colour.darker(.2f),0,(float)height,false));g.fillRoundedRectangle(area,3);g.setColour(box.findColour(juce::ComboBox::outlineColourId));g.drawRoundedRectangle(area,3,1);g.setColour(juce::Colours::white.withAlpha(box.isEnabled()?.7f:.25f));juce::Path arrow;float x=width-15.f,y=height*.5f;arrow.startNewSubPath(x-4,y-2);arrow.lineTo(x,y+2);arrow.lineTo(x+4,y-2);g.strokePath(arrow,juce::PathStrokeType(1.5f));
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float v,float a,float b,juce::Slider& slider) override{drawDjlRotary(g,x,y,w,h,v,a,b,slider);}
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* box=juce::LookAndFeel_V4::createSliderTextBox(slider);
        box->setJustificationType(isDjlRotary(slider)?juce::Justification::centred:juce::Justification::centredRight);
        box->setFont(juce::FontOptions(12));
        box->setBorderSize(isDjlRotary(slider)?juce::BorderSize<int>(0,3,0,3):juce::BorderSize<int>(0,5,0,7));
        return box;
    }
};

inline void useValueBar(juce::Slider& slider)
{
    configureDjlRotary(slider);
    slider.setSliderSnapsToMousePosition(false);
    slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff67dfc5).withAlpha(.3f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe5ebf5));
    slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff2b3648));
}

// JUCE's value label (and its live text editor) consumes double clicks before
// they reach Slider. Keep single-click typing and route just that gesture back.
class ResetSlider : public DjlAdaptiveSlider,public juce::AsyncUpdater
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
