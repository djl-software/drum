#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

inline void drawDjlButton3D(juce::Graphics& g,juce::Button& button,juce::Colour colour,bool hover,bool down)
{
    const bool selected=button.getToggleState(),pressed=down || selected;
    const bool navigation=bool(button.getProperties()["toolbar-3d"]);
    const float alpha=button.isEnabled()?1.f:.35f;
    const juce::Colour accent(0xff67dfc5);
    auto base=selected?button.findColour(juce::TextButton::buttonOnColourId):colour;
    if(navigation && selected)base=juce::Colour(0xff17272e);
    if(hover)base=base.brighter(.12f);
    auto area=button.getLocalBounds().toFloat().reduced(.5f);
    const float radius=juce::jmin(5.f,area.getHeight()*.2f);
    g.setColour(juce::Colours::black.withAlpha(.55f*alpha));g.fillRoundedRectangle(area.withTrimmedTop(2),radius);
    auto face=area.withTrimmedBottom(pressed?1.f:3.f).translated(0,pressed?1.5f:0.f);
    g.setGradientFill(juce::ColourGradient(base.brighter(pressed?.02f:.25f).withMultipliedAlpha(alpha),0,face.getY(),base.darker(pressed?.18f:.22f).withMultipliedAlpha(alpha),0,face.getBottom(),false));g.fillRoundedRectangle(face,radius);
    g.setColour((navigation && selected?accent:base.brighter(.45f)).withMultipliedAlpha(alpha*.65f));g.drawRoundedRectangle(face.reduced(.5f),radius,1);
    g.setColour((pressed?juce::Colours::black:juce::Colours::white).withAlpha(alpha*(pressed?.35f:.2f)));g.drawHorizontalLine(juce::roundToInt(face.getY()+2),face.getX()+4,face.getRight()-4);
    g.setColour(juce::Colours::black.withAlpha(.25f*alpha));g.drawHorizontalLine(juce::roundToInt(face.getBottom()-2),face.getX()+4,face.getRight()-4);
}
