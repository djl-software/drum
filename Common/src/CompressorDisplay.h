#pragma once
#include "BuiltinEffects.h"
namespace drum {
// Actual audio history, not a synthetic illustration. Display switching is UI-only.
class CompressorDisplay final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit CompressorDisplay(BuiltinEffect& p):effect(p){
        setComponentID("compressor-display");
        for(int n=0;n<3;++n){auto& b=views[(size_t)n];b.setButtonText(n==0?"Meters":n==1?"Transfer":"Activity");b.setComponentID("compressor-view-"+juce::String(n));b.onClick=[this,n]{effect.compressorView.store(n);changed();};addAndMakeVisible(b);}
        gr.setButtonText("GR");gr.setComponentID("compressor-show-gr");gr.onClick=[this]{effect.compressorShowOutput.store(false);changed();};addAndMakeVisible(gr);
        output.setButtonText("Output");output.setComponentID("compressor-show-output");output.onClick=[this]{effect.compressorShowOutput.store(true);changed();};addAndMakeVisible(output);
        setTooltip("Select Meters, Transfer or Activity. Activity shows input peaks with gain reduction or output. Drag the threshold line; click a meter's peak reading to reset its maximum.");
        effect.compressorHistory.enabled.store(true);updateButtons();
    }
    ~CompressorDisplay() override{effect.compressorHistory.enabled.store(false);}
    void tick(){
        latest={};bool received=false;effect.compressorHistory.consume([&](auto frame){history[cursor]=frame;cursor=(cursor+1)%history.size();size=juce::jmin(history.size(),size+1);latest.inputL=juce::jmax(latest.inputL,frame.inputL);latest.inputR=juce::jmax(latest.inputR,frame.inputR);latest.outputL=juce::jmax(latest.outputL,frame.outputL);latest.outputR=juce::jmax(latest.outputR,frame.outputR);if(std::abs(frame.reduction)>std::abs(latest.reduction))latest.reduction=frame.reduction;received=true;peakIn=juce::jmax(peakIn,frame.inputL,frame.inputR);peakOut=juce::jmax(peakOut,frame.outputL,frame.outputR);if(std::abs(frame.reduction)>std::abs(peakGr))peakGr=frame.reduction;});
        if(!received)latest={};const auto now=juce::Time::getMillisecondCounterHiRes();const std::array<float,4> levels{latest.inputL,latest.inputR,latest.outputL,latest.outputR};for(size_t i=0;i<levels.size();++i)if(levels[i]>=heldBars[i] || now>=heldUntil[i]){heldBars[i]=levels[i];heldUntil[i]=now+1000;}updateButtons();repaint();
    }
    void resized() override{for(int n=0;n<3;++n)views[(size_t)n].setBounds(8+n*86,getHeight()-29,80,23);gr.setBounds(getWidth()-156,getHeight()-29,62,23);output.setBounds(getWidth()-88,getHeight()-29,80,23);}
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(0xff10151f));auto a=plot();g.setFont(juce::FontOptions(11));const int mode=effect.compressorView.load();
        if(mode==0){drawMeters(g,{40,12,(float)getWidth()-80,(float)getHeight()-54},true);return;}
        for(int db=-60;db<=0;db+=6){g.setColour(juce::Colour(0xff2b3648));g.drawHorizontalLine((int)y(a,(float)db),a.getX(),a.getRight());if(db%12==0){g.setColour(ink);g.drawText(juce::String(db),0,(int)y(a,(float)db)-6,30,12,juce::Justification::centredRight);}if(mode==1){g.setColour(juce::Colour(0xff2b3648));g.drawVerticalLine((int)x(a,(float)db),a.getY(),a.getBottom());}}
        const float threshold=effect.controls[0]->get();
        if(mode==1){juce::Path curve;for(int i=0;i<=300;++i){const float in=-60+60*i/300.f;const auto point=juce::Point<float>(x(a,in),y(a,transfer(in)));if(i==0)curve.startNewSubPath(point);else curve.lineTo(point);}g.setColour(ink.withAlpha(.35f));g.drawLine(a.getX(),a.getBottom(),a.getRight(),a.getY());g.setColour(mint);g.strokePath(curve,juce::PathStrokeType(2));g.setColour(amber);g.drawVerticalLine((int)x(a,threshold),a.getY(),a.getBottom());const float knee=effect.controls[6]->get();float dash[]{3,3};for(float edge:{threshold-knee*.5f,threshold+knee*.5f})g.drawDashedLine({{x(a,edge),a.getY()},{x(a,edge),a.getBottom()}},dash,2);g.setColour(juce::Colours::white);g.fillEllipse(x(a,toDb(juce::jmax(latest.inputL,latest.inputR)))-3,y(a,toDb(juce::jmax(latest.outputL,latest.outputR)))-3,6,6);}
        else {
            const bool showOut=effect.compressorShowOutput.load();for(int lane=0;lane<2;++lane){juce::Path line;for(size_t i=0;i<size;++i){const auto& f=history[(cursor+history.size()-size+i)%history.size()];const float px=a.getRight()-(size-1-i)*a.getWidth()/(history.size()-1);const float py=lane==0?y(a,toDb(juce::jmax(f.inputL,f.inputR))):showOut?y(a,toDb(juce::jmax(f.outputL,f.outputR))):(effect.controls[8]->get()>=1.5f?a.getBottom()-juce::jlimit(0.f,60.f,-f.reduction)/60*a.getHeight():a.getY()+juce::jlimit(0.f,60.f,f.reduction)/60*a.getHeight());if(i==0)line.startNewSubPath(px,py);else line.lineTo(px,py);}auto fill=line;fill.lineTo(a.getRight(),lane==1 && !showOut && effect.controls[8]->get()<1.5f?a.getY():a.getBottom());fill.lineTo(a.getRight()-(size?size-1:0)*a.getWidth()/(history.size()-1),lane==1 && !showOut && effect.controls[8]->get()<1.5f?a.getY():a.getBottom());fill.closeSubPath();g.setColour((lane==0?ink:showOut?mint:amber).withAlpha(.18f));g.fillPath(fill);g.setColour(lane==0?ink:showOut?mint:amber);g.strokePath(line,juce::PathStrokeType(1.4f));}
            g.setColour(blue);g.drawHorizontalLine((int)y(a,threshold),a.getX(),a.getRight());
        }
        g.setColour(ink);g.drawText(mode==1?"Input / output dBFS - drag threshold":"8 seconds - input / "+juce::String(effect.compressorShowOutput.load()?"output":effect.controls[8]->get()>=1.5f?"gain increase (up from 0 dB)":"gain reduction (down from 0 dB)"),34,0,(int)a.getWidth(),18,juce::Justification::centredLeft);drawMeters(g,{a.getRight()+18,12,(float)getWidth()-a.getRight()-26,(float)getHeight()-54},false);
    }
    void mouseDown(const juce::MouseEvent& e) override{
        const bool large=effect.compressorView.load()==0;const float meterX=large?40:plot().getRight()+18,meterWidth=large?getWidth()-80:getWidth()-meterX-8;
        if(e.position.y>=12 && e.position.y<32 && e.position.x>=meterX && e.position.x<meterX+meterWidth){const int column=juce::jlimit(0,2,(int)((e.position.x-meterX)/(meterWidth/3)));if(column==0)peakIn=0;else if(column==1)peakGr=0;else peakOut=0;repaint();return;}
        if(plot().contains(e.position) && effect.compressorView.load()!=0){dragging=true;effect.controls[0]->beginChangeGesture();mouseDrag(e);}
    }
    void mouseDrag(const juce::MouseEvent& e) override{if(!dragging)return;auto a=plot();float db=effect.compressorView.load()==1?-60+60*(e.position.x-a.getX())/a.getWidth():-60+60*(a.getBottom()-e.position.y)/a.getHeight();auto* p=effect.controls[0];p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(p->range.start,p->range.end,db)));repaint();}
    void mouseUp(const juce::MouseEvent&) override{if(dragging){effect.controls[0]->endChangeGesture();dragging=false;}}
private:
    void changed(){updateButtons();effect.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));repaint();}
    void updateButtons(){for(int n=0;n<3;++n)views[(size_t)n].setToggleState(effect.compressorView.load()==n,juce::dontSendNotification);gr.setToggleState(!effect.compressorShowOutput.load(),juce::dontSendNotification);output.setToggleState(effect.compressorShowOutput.load(),juce::dontSendNotification);const bool activity=effect.compressorView.load()==2;gr.setVisible(activity);output.setVisible(activity);}
    static float toDb(float gain){return juce::Decibels::gainToDecibels(gain,-60.f);}
    static float x(juce::Rectangle<float> a,float db){return a.getX()+juce::jlimit(0.f,1.f,(db+60)/60)*a.getWidth();}
    static float y(juce::Rectangle<float> a,float db){return a.getBottom()-juce::jlimit(0.f,1.f,(db+60)/60)*a.getHeight();}
    juce::Rectangle<float> plot() const{return {34,22,(float)getWidth()-204,(float)getHeight()-62};}
    float transfer(float in) const{const auto& c=effect.controls;const int mode=(int)std::round(c[8]->get());const float change=CompressorDSP::change(in,c[0]->get(),c[1]->get(),c[6]->get(),mode),automatic=CompressorDSP::makeup(c[0]->get(),c[1]->get(),c[6]->get(),mode,c[12]->get()>=.5f,c[13]->get()>=.5f);const float gain=juce::Decibels::decibelsToGain(-change+c[4]->get()+automatic);return in+juce::Decibels::gainToDecibels(1+c[5]->get()*.01f*(gain-1),-100.f);}
    void drawMeters(juce::Graphics& g,juce::Rectangle<float> area,bool large){
        const float width=area.getWidth()/3;for(int column=0;column<3;++column){auto a=area.withX(area.getX()+column*width).withWidth(width).reduced(5,0);const bool reduction=column==1;g.setColour(ink);g.setFont(juce::FontOptions(large?13:10));const float held=column==0?toDb(peakIn):column==2?toDb(peakOut):peakGr;g.drawText(juce::String(held,1),a.removeFromTop(20).toNearestInt(),juce::Justification::centred);g.drawText(column==0?"In":reduction?"GR":"Out",a.removeFromBottom(18).toNearestInt(),juce::Justification::centred);
        auto bars=a.withSizeKeepingCentre(large?64.f:a.getWidth()-4,a.getHeight());for(int db=0;db<=60;db+=6){g.setColour(ink.withAlpha(.4f));const float py=bars.getY()+db/60.f*bars.getHeight();g.drawHorizontalLine((int)py,bars.getX()-3,bars.getRight()+3);if(large && db%12==0){g.setFont(juce::FontOptions(10));g.drawText(juce::String(reduction?db:-db),(int)bars.getX()-32,(int)py-6,26,12,juce::Justification::centredRight);}}for(int channel=0;channel<(reduction?1:2);++channel){auto bar=bars.withWidth(bars.getWidth()/(reduction?1:2)).translated(channel*bars.getWidth()/2,0).reduced(1,0);g.setColour(juce::Colour(0xff263141));g.fillRect(bar);float level=reduction?latest.reduction:column==0?(channel?latest.inputR:latest.inputL):(channel?latest.outputR:latest.outputL);const float fraction=reduction?juce::jlimit(0.f,1.f,std::abs(level)/60):juce::jlimit(0.f,1.f,(toDb(level)+60)/60);if(reduction){g.setColour(level<0?mint:amber);g.fillRect(level<0?bar.withTop(bar.getBottom()-bar.getHeight()*fraction):bar.withHeight(bar.getHeight()*fraction));}else{for(int segment=0;segment<60;++segment)if(segment/60.f<fraction){g.setColour(segment>=57?red:segment>=48?amber:mint);g.fillRect(bar.getX(),bar.getBottom()-(segment+1)*bar.getHeight()/60,bar.getWidth(),juce::jmax(1.f,bar.getHeight()/60-1));}g.setColour(juce::Colours::white.withAlpha(.85f));g.drawHorizontalLine((int)y(bar,toDb(heldBars[(size_t)(column==0?channel:channel+2)])),bar.getX(),bar.getRight());}}}
    }
    BuiltinEffect& effect;std::array<juce::TextButton,3> views;juce::TextButton gr,output;
    std::array<CompressorHistory::Frame,1600> history{};size_t cursor=0,size=0;CompressorHistory::Frame latest;float peakIn=0,peakOut=0,peakGr=0;bool dragging=false;std::array<float,4> heldBars{};std::array<double,4> heldUntil{};
    const juce::Colour ink{0xffb9c8d6},mint{0xff67dfc5},amber{0xffffba69},blue{0xff68b6ff},red{0xffff687d};
};
}
