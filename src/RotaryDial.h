#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
inline void drawDrumRotary(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider)
{
    const juce::Colour border(0xff2b3648),accent(0xff67dfc5),text(0xffe5ebf5);
        const auto area=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height).reduced(4);
        const auto centre=area.getCentre();const float radius=juce::jmin(area.getWidth(),area.getHeight())*0.5f;
        if(radius<2)return;
        const auto alpha=slider.isEnabled()?1.0f:0.35f;
        juce::Path track,level;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        level.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+value*(end-start),true);
        g.setColour(border.withAlpha(alpha));g.strokePath(track,juce::PathStrokeType(2.5f));
        g.setColour(accent.withAlpha(alpha));g.strokePath(level,juce::PathStrokeType(2.5f));
        const float dial=radius-4;g.setColour(juce::Colour(0xff424d59).withAlpha(alpha));g.fillEllipse(centre.x-dial,centre.y-dial,dial*2,dial*2);
        g.setColour(juce::Colour(0xff83909b).withAlpha(alpha));g.drawEllipse(centre.x-dial,centre.y-dial,dial*2,dial*2,1);
        const auto angle=start+value*(end-start);const auto tip=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*(dial-2);
        g.setColour(text.withAlpha(alpha));g.drawLine(centre.x,centre.y,tip.x,tip.y,2);
}
