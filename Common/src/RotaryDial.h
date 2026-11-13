#pragma once
#include "DjlRotaryControl.h"
inline void drawDrumRotary(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider)
{drawDjlRotary(g,x,y,width,height,value,start,end,slider);}
