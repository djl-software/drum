#pragma once
#include "BuiltinEffects.h"

namespace drum {
// UI-only graphs: no FFT or audio-buffer copying. Peak history reads atomics at 15 Hz.
class BuiltinGraph final : public juce::Component
{
public:
    explicit BuiltinGraph(BuiltinEffect& p):effect(p){setComponentID("builtin-graph");}
    void tick(){in[cursor]=effect.inputPeak.load();out[cursor]=effect.outputPeak.load();cursor=(cursor+1)%in.size();repaint();}
    void paint(juce::Graphics& g) override
    {
        auto a=plot();g.fillAll(juce::Colour(0xff10151f));g.setFont(juce::FontOptions(11));
        const bool nonlinear=effect.kind==BuiltinKind::saturation || effect.kind==BuiltinKind::clipper;
        const bool history=effect.kind==BuiltinKind::transient || effect.kind==BuiltinKind::utility;
        g.setColour(juce::Colour(0xff2b3648));for(int i=0;i<=6;++i){float f=i/6.f;g.drawHorizontalLine(juce::roundToInt(a.getBottom()-f*a.getHeight()),a.getX(),a.getRight());g.drawVerticalLine(juce::roundToInt(a.getX()+f*a.getWidth()),a.getY(),a.getBottom());}
        auto x=[&](float db){return a.getX()+(juce::jlimit(-60.f,6.f,db)+60)/66*a.getWidth();};
        auto y=[&](float db){return a.getBottom()-(juce::jlimit(-60.f,6.f,db)+60)/66*a.getHeight();};
        if(history){
            for(int lane=0;lane<2;++lane){juce::Path line;for(size_t i=0;i<in.size();++i){auto v=(lane?out:in)[(cursor+i)%in.size()];auto px=a.getX()+(float)i/(in.size()-1)*a.getWidth();auto py=y(juce::Decibels::gainToDecibels(v,-60.f));if(i==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}g.setColour(lane?juce::Colour(0xff67dfc5):juce::Colour(0xff8d9bb0));g.strokePath(line,juce::PathStrokeType(1.5f));}
        }else{
            juce::Path curve;for(int i=0;i<=200;++i){float f=i/200.f,px=a.getX()+f*a.getWidth(),py=0;
                if(nonlinear){float input=f*2-1,drive=juce::Decibels::decibelsToGain(value(0)),output=0;
                    if(effect.kind==BuiltinKind::saturation){float wet=std::tanh(input*drive)/std::tanh(drive);output=(input+(wet-input)*value(1)*.01f)*juce::Decibels::decibelsToGain(value(2));}
                    else {float ceiling=juce::Decibels::decibelsToGain(value(1)),v=input*drive/ceiling,soft=value(2)*.01f;output=((1-soft)*juce::jlimit(-1.f,1.f,v)+soft*std::tanh(v))*ceiling*juce::Decibels::decibelsToGain(value(3));}
                    py=a.getCentreY()-juce::jlimit(-1.f,1.f,output)*a.getHeight()*.5f;
                }else{float input=-60+f*66,output=input;
                    if(effect.kind==BuiltinKind::compressor){float over=input-value(0),knee=value(6),amount=juce::jmax(0.f,over);if(knee>0 && over> -knee*.5f && over<knee*.5f)amount=(over+knee*.5f)*(over+knee*.5f)/(2*knee);float gain=juce::Decibels::decibelsToGain(-amount*(1-1/value(1))+value(4));output=input+juce::Decibels::gainToDecibels(1+value(5)*.01f*(gain-1),-100.f);}
                    else if(effect.kind==BuiltinKind::limiter)output=juce::jmin(input+value(0),value(1));
                    else if(effect.kind==BuiltinKind::gate)output=input-(input<value(0)?value(1):0);
                    py=y(output);
                }
                if(i==0)curve.startNewSubPath(px,py);else curve.lineTo(px,py);
            }
            g.setColour(juce::Colour(0xff67dfc5));g.strokePath(curve,juce::PathStrokeType(2));
            if(dynamics()){
                const float threshold=value(thresholdIndex());g.setColour(juce::Colour(0xffe9ad70));
                if(effect.kind==BuiltinKind::limiter)g.drawHorizontalLine(juce::roundToInt(y(threshold)),a.getX(),a.getRight());else g.drawVerticalLine(juce::roundToInt(x(threshold)),a.getY(),a.getBottom());
                g.drawText((effect.kind==BuiltinKind::limiter?"Ceiling ":"Threshold ")+juce::String(threshold,1)+" dB (drag line)",a.toNearestInt().removeFromTop(18),juce::Justification::centredRight);
                g.setColour(juce::Colours::white);g.fillEllipse(x(juce::Decibels::gainToDecibels(effect.inputPeak.load(),-60.f))-3,y(juce::Decibels::gainToDecibels(effect.outputPeak.load(),-60.f))-3,6,6);
            }
        }
        g.setColour(juce::Colour(0xff9aabba));g.drawText(history?"Peak history: input grey / output green (8 seconds)":nonlinear?"Waveshaping: input -1 to +1 / output -1 to +1":"Transfer curve: input / output dBFS; white dot = measured peaks",8,0,getWidth()-16,18,juce::Justification::centredLeft);
        g.drawText(nonlinear?"-1                         Input amplitude                         +1":history?"Earlier                                                     Now":"-60                        Input dBFS                        +6",a.toNearestInt().withY(getHeight()-19).withHeight(18),juce::Justification::centred);
        if(!nonlinear)for(int db:{-60,-36,-12,0})g.drawText(juce::String(db),0,juce::roundToInt(y((float)db))-6,30,12,juce::Justification::centredRight);
    }
    void mouseDown(const juce::MouseEvent& e) override{if(dynamics() && plot().contains(e.position)){dragging=true;effect.controls[(size_t)thresholdIndex()]->beginChangeGesture();mouseDrag(e);}}
    void mouseDrag(const juce::MouseEvent& e) override{if(!dragging)return;auto a=plot();float f=effect.kind==BuiltinKind::limiter?(a.getBottom()-e.position.y)/a.getHeight():(e.position.x-a.getX())/a.getWidth();auto* p=effect.controls[(size_t)thresholdIndex()];p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(p->range.start,p->range.end,-60+66*f)));repaint();}
    void mouseUp(const juce::MouseEvent&) override{if(dragging){effect.controls[(size_t)thresholdIndex()]->endChangeGesture();dragging=false;}}
private:
    juce::Rectangle<float> plot() const{return {34,24,(float)getWidth()-46,(float)getHeight()-46};}
    bool dynamics() const{return effect.kind==BuiltinKind::compressor || effect.kind==BuiltinKind::limiter || effect.kind==BuiltinKind::gate;}
    int thresholdIndex() const{return effect.kind==BuiltinKind::limiter?1:0;}
    float value(int i) const{return effect.controls[(size_t)i]->get();}
    BuiltinEffect& effect;std::array<float,120> in{},out{};size_t cursor=0;bool dragging=false;
};
}
