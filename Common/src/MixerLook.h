#pragma once
#include "ResetSlider.h"

class DrumMixerLook final : public ValueBarLook
{
public:
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float minimum,float maximum,juce::Slider::SliderStyle style,juce::Slider& slider) override
    {
        if(bool(slider.getProperties()["mixer-send"])){
            auto area=slider.getLocalBounds().toFloat().reduced(.5f);g.setColour(juce::Colour(0xff111b26));g.fillRect(area);
            auto fill=area.withWidth(area.getWidth()*(float)slider.valueToProportionOfLength(slider.getValue()));if(bool(slider.getProperties()["mixer-pan"])){float centre=area.getCentreX(),value=area.getX()+area.getWidth()*(float)slider.valueToProportionOfLength(slider.getValue());fill={juce::jmin(centre,value),area.getY(),std::abs(centre-value),area.getHeight()};}g.setColour(juce::Colour(0xff397b79));g.fillRect(fill);
            g.setColour(slider.hasKeyboardFocus(true)?juce::Colour(0xff67dfc5):juce::Colour(0xff526273));g.drawRect(area,1);g.setColour(juce::Colour(0xffeef3f8));g.setFont(juce::FontOptions(10));g.drawFittedText(slider.getTextFromValue(slider.getValue()),slider.getLocalBounds().reduced(1,0),juce::Justification::centred,1);return;
        }
        if(!bool(slider.getProperties()["mixer-fader"])){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimum,maximum,style,slider);return;}
        auto tint=slider.findColour(juce::Slider::thumbColourId);const float rail=(float)(x+width-23);
        auto at=[&](double db){return (float)y+(1.f-(float)slider.valueToProportionOfLength(juce::jlimit(slider.getMinimum(),slider.getMaximum(),db)))*height;};
        // A fixed dB colour scale: green below -6, yellow from -6 to 0,
        // red at/above 0. Lower green LEDs remain green when peaks rise.
        for(int lane=0;lane<2;++lane){float mx=(float)(x+38+lane*9);float peak=(float)slider.getProperties()[lane==0?"peak-l":"peak-r"];float db=juce::Decibels::gainToDecibels(peak,-100.f),top=at(db);g.setColour(juce::Colour(0xff0c1219));g.fillRect(mx,(float)y,6.f,(float)height);
            auto light=[&](float bottom,float zoneTop,juce::Colour colour){float start=juce::jmax(top,zoneTop);if(bottom>start){g.setColour(colour);g.fillRect(mx,start,6.f,bottom-start);}};
            light((float)(y+height),at(-6),juce::Colour(0xff54cf78));light(at(-6),at(0),juce::Colour(0xfff1cf59));light(at(0),(float)y,juce::Colour(0xffed5d61));
            g.setColour(juce::Colour(0xff10151f));for(int py=y;py<y+height;py+=4)g.drawHorizontalLine(py,mx,mx+6);
            float hold=(float)slider.getProperties()[lane==0?"hold-l":"hold-r"];if(hold>0){g.setColour(juce::Colour(0xfff1f4f8));float hy=at(juce::Decibels::gainToDecibels(hold,-100.f));g.fillRect(mx-1,hy-1,8.f,2.f);}
        }
        float font=juce::jlimit(6.f,9.f,(float)height*6.f/(float)(slider.getMaximum()-slider.getMinimum())-1.f);g.setFont(juce::FontOptions(font));
        for(int db=12;db>=slider.getMinimum();db-=3){float ty=at(db);g.setColour(db==0?juce::Colour(0xffe5ebf5):juce::Colour(0xff8d9bb0));if(db%6==0)g.drawText(juce::String(db),x,juce::roundToInt(ty)-5,31,10,juce::Justification::centredRight);g.drawHorizontalLine(juce::roundToInt(ty),x+33.f,x+36.f);}
        // Recessed console fader, with a wide metal cap and a true position mark.
        const juce::Rectangle<float> surround(rail-19,(float)y-2,38,(float)height+4);
        g.setColour(juce::Colour(0xff080d14).withAlpha(.42f));g.fillRoundedRectangle(surround,5);
        g.setColour(juce::Colour(0xff536174).withAlpha(.3f));g.drawRoundedRectangle(surround,5,1);
        juce::Rectangle<float> slot(rail-5,(float)y,10,(float)height);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff020407),rail-5,0,juce::Colour(0xff29323d),rail+5,0,false));g.fillRoundedRectangle(slot,4);
        g.setColour(juce::Colours::black);g.fillRoundedRectangle(slot.reduced(3,1),2);
        for(int db=12;db>=slider.getMinimum();db-=3){const float ty=at(db);g.setColour(tint.withAlpha(db==0?.85f:.25f));g.drawHorizontalLine(juce::roundToInt(ty),rail-15,rail-9);}
        const juce::Rectangle<float> cap(rail-17,position-13,34,26);
        g.setColour(juce::Colours::black.withAlpha(.55f));g.fillRoundedRectangle(cap.translated(0,3).expanded(2,1),4);
        juce::ColourGradient metal(juce::Colour(0xffe7edf3),rail,cap.getY(),juce::Colour(0xff697787),rail,cap.getBottom(),false);
        metal.addColour(.18,juce::Colour(0xffa5b1bd));metal.addColour(.46,juce::Colour(0xffd2dae2));metal.addColour(.52,juce::Colour(0xff97a4b1));metal.addColour(.85,juce::Colour(0xffb0bcc7));
        g.setGradientFill(metal);g.fillRoundedRectangle(cap,3);
        g.setColour(juce::Colour(0xff111923));g.drawRoundedRectangle(cap,3,1);
        g.setColour(juce::Colours::white.withAlpha(.65f));g.drawHorizontalLine(juce::roundToInt(cap.getY()+2),cap.getX()+4,cap.getRight()-4);
        for(float dy:{-8.f,-5.f,5.f,8.f}){g.setColour(juce::Colour(0xff485564).withAlpha(.65f));g.fillRect(rail-11,position+dy,22.f,.7f);g.setColour(juce::Colours::white.withAlpha(.32f));g.fillRect(rail-11,position+dy+1,22.f,.6f);}
        g.setColour(juce::Colour(0xff17222d));g.fillRect(cap.getX()+2,position-1,cap.getWidth()-4,2.f);
        g.setColour(tint);g.fillRect(cap.getX()+2,position-.5f,5.f,1.f);
        if(slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true)){g.setColour(tint.withAlpha(.7f));g.drawRoundedRectangle(cap.expanded(.5f),3,1);}
    }
};
