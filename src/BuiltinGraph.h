#pragma once
#include "BuiltinEffects.h"

namespace drum {
// UI-only drawing. Saturation reads a bounded audio FIFO; history reads peak atomics at 15 Hz.
class BuiltinGraph final : public juce::Component
{
public:
    explicit BuiltinGraph(BuiltinEffect& p):effect(p){setComponentID("builtin-graph");}
    ~BuiltinGraph() override{if(effect.waveform)effect.waveform->enabled.store(false);}
    void tick(){if(effect.waveform){effect.waveform->enabled.store(isShowing());effect.waveform->consume([&](float l,float r){waveL[waveCursor]=std::isfinite(l)?l:0;waveR[waveCursor]=std::isfinite(r)?r:0;waveCursor=(waveCursor+1)%waveL.size();});}in[cursor]=effect.inputPeak.load();out[cursor]=effect.outputPeak.load();cursor=(cursor+1)%in.size();repaint();}
    void paint(juce::Graphics& g) override
    {
        auto a=plot();g.fillAll(juce::Colour(0xff10151f));g.setFont(juce::FontOptions(11));
        const bool nonlinear=effect.kind==BuiltinKind::saturation || effect.kind==BuiltinKind::clipper;
        const bool history=effect.kind==BuiltinKind::transient || effect.kind==BuiltinKind::utility;
        g.setColour(juce::Colour(0xff2b3648));for(int i=0;i<=6;++i){float f=i/6.f;g.drawHorizontalLine(juce::roundToInt(a.getBottom()-f*a.getHeight()),a.getX(),a.getRight());g.drawVerticalLine(juce::roundToInt(a.getX()+f*a.getWidth()),a.getY(),a.getBottom());}
        if(dynamics()){
            float f=juce::jlimit(0.f,1.f,(value(thresholdIndex())+60)/66);
            g.setColour(juce::Colour(0xffffba69).withAlpha(.07f));
            if(effect.kind==BuiltinKind::limiter)g.fillRect(a.withHeight(a.getHeight()*(1-f)));
            else g.fillRect(a.withTrimmedLeft(a.getWidth()*f));
        }
        auto x=[&](float db){return a.getX()+(juce::jlimit(-60.f,6.f,db)+60)/66*a.getWidth();};
        auto y=[&](float db){return a.getBottom()-(juce::jlimit(-60.f,6.f,db)+60)/66*a.getHeight();};
        if(history){
            for(int lane=0;lane<2;++lane){juce::Path line;for(size_t i=0;i<in.size();++i){auto v=(lane?out:in)[(cursor+i)%in.size()];auto px=a.getX()+(float)i/(in.size()-1)*a.getWidth();auto py=y(juce::Decibels::gainToDecibels(v,-60.f));if(i==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}g.setColour(lane?juce::Colour(0xff67dfc5):juce::Colour(0xff68b6ff));g.strokePath(line,juce::PathStrokeType(1.5f));}
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
            auto fill=curve;fill.lineTo(a.getRight(),a.getBottom());fill.lineTo(a.getX(),a.getBottom());fill.closeSubPath();
            auto accent=nonlinear?juce::Colour(0xffbd91ff):juce::Colour(0xff67dfc5);
            g.setGradientFill(juce::ColourGradient(accent.withAlpha(.24f),a.getX(),a.getY(),accent.withAlpha(.015f),a.getX(),a.getBottom(),false));g.fillPath(fill);
            g.setColour(juce::Colour(0xff68b6ff).withAlpha(.4f));g.drawLine(a.getX(),a.getBottom(),a.getRight(),a.getY(),1);
            g.setColour(accent);g.strokePath(curve,juce::PathStrokeType(2));
            if(dynamics()){
                const float threshold=value(thresholdIndex());g.setColour(juce::Colour(0xffe9ad70));
                if(effect.kind==BuiltinKind::limiter)g.drawHorizontalLine(juce::roundToInt(y(threshold)),a.getX(),a.getRight());else g.drawVerticalLine(juce::roundToInt(x(threshold)),a.getY(),a.getBottom());
                g.drawText((effect.kind==BuiltinKind::limiter?"Ceiling ":"Threshold ")+juce::String(threshold,1)+" dB (drag line)",a.toNearestInt().removeFromTop(18),juce::Justification::centredRight);
                g.setColour(juce::Colours::white);g.fillEllipse(x(juce::Decibels::gainToDecibels(effect.inputPeak.load(),-60.f))-3,y(juce::Decibels::gainToDecibels(effect.outputPeak.load(),-60.f))-3,6,6);
            }
        }
        g.setColour(juce::Colour(0xff9aabba));g.drawText(effect.waveform?"Saturation curve":history?"Peak history: input blue / output green (8 seconds)":nonlinear?"Waveshaping: input -1 to +1 / output -1 to +1":"Transfer curve: input / output dBFS; white dot = measured peaks",8,0,effect.waveform?(int)a.getRight()-8:getWidth()-16,18,juce::Justification::centredLeft);
        g.drawText(nonlinear?"-1     Input     +1":history?"Earlier                                                     Now":"-60                        Input dBFS                        +6",a.toNearestInt().withY(getHeight()-19).withHeight(18),juce::Justification::centred);
        if(effect.waveform)drawWaveform(g);
        if(!nonlinear)for(int db:{-60,-36,-12,0})g.drawText(juce::String(db),0,juce::roundToInt(y((float)db))-6,30,12,juce::Justification::centredRight);
    }
    void mouseDown(const juce::MouseEvent& e) override{if(dynamics() && plot().contains(e.position)){dragging=true;effect.controls[(size_t)thresholdIndex()]->beginChangeGesture();mouseDrag(e);}}
    void mouseDrag(const juce::MouseEvent& e) override{if(!dragging)return;auto a=plot();float f=effect.kind==BuiltinKind::limiter?(a.getBottom()-e.position.y)/a.getHeight():(e.position.x-a.getX())/a.getWidth();auto* p=effect.controls[(size_t)thresholdIndex()];p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(p->range.start,p->range.end,-60+66*f)));repaint();}
    void mouseUp(const juce::MouseEvent&) override{if(dragging){effect.controls[(size_t)thresholdIndex()]->endChangeGesture();dragging=false;}}
private:
    juce::Rectangle<float> plot() const{return {34,24,effect.waveform?(getWidth()-58)*.43f:(float)getWidth()-46,(float)getHeight()-46};}
    void drawWaveform(juce::Graphics& g){
        auto a=plot();juce::Rectangle<float> scope(a.getRight()+22,a.getY(),getWidth()-a.getRight()-34,a.getHeight());
        g.setColour(juce::Colour(0xff9aabba));g.drawText("Output waveform: L blue / R amber",(int)scope.getX(),0,(int)scope.getWidth(),18,juce::Justification::centredLeft);
        // Start at a rising zero crossing so periodic sounds remain readable.
        size_t start=waveCursor;for(size_t n=0;n<512;++n){size_t i=(waveCursor+n)%waveL.size(),next=(i+1)%waveL.size();if(waveL[i]<=0 && waveL[next]>0){start=next;break;}}
        constexpr int shown=512;
        for(int channel=0;channel<2;++channel){auto lane=scope.withHeight(scope.getHeight()*.5f).translated(0,channel*scope.getHeight()*.5f);float centre=lane.getCentreY(),scale=lane.getHeight()*.40f;
            g.setColour(juce::Colour(0xff2b3648));g.drawHorizontalLine((int)centre,lane.getX(),lane.getRight());
            for(int n=0;n<=4;++n)g.drawVerticalLine((int)(lane.getX()+n*lane.getWidth()/4),lane.getY(),lane.getBottom());
            juce::Path line;for(int n=0;n<shown;++n){float value=(channel?waveR:waveL)[(start+n)%waveL.size()],px=lane.getX()+n*lane.getWidth()/(shown-1),py=centre-juce::jlimit(-1.2f,1.2f,value)*scale;if(n==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}
            auto colour=channel?juce::Colour(0xffffbd69):juce::Colour(0xff68b6ff);auto fill=line;fill.lineTo(lane.getRight(),centre);fill.lineTo(lane.getX(),centre);fill.closeSubPath();g.setColour(colour.withAlpha(.17f));g.fillPath(fill);g.setColour(colour);g.strokePath(line,juce::PathStrokeType(1.4f));
        }
        g.setColour(juce::Colour(0xff9aabba));g.drawText(juce::String(shown*1000.0/effect.waveform->rate.load(),1)+" ms  |  fixed +/-1 scale",scope.toNearestInt().withY(getHeight()-19).withHeight(18),juce::Justification::centred);
    }
    bool dynamics() const{return effect.kind==BuiltinKind::compressor || effect.kind==BuiltinKind::limiter || effect.kind==BuiltinKind::gate;}
    int thresholdIndex() const{return effect.kind==BuiltinKind::limiter?1:0;}
    float value(int i) const{return effect.controls[(size_t)i]->get();}
    BuiltinEffect& effect;std::array<float,120> in{},out{};size_t cursor=0;bool dragging=false;std::array<float,2048> waveL{},waveR{};size_t waveCursor=0;
};
}
