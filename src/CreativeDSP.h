#pragma once
#include "BuiltinKind.h"
#include "Equalizer.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace drum {
// Prepared fixed storage. All methods below are allocation-free during playback.
class CreativeDSP {
    struct Filter {EqCoefficients c;double x1=0,x2=0;float run(float x){double y=c.b0*x+x1;x1=c.b1*x-c.a1*y+x2;x2=c.b2*x-c.a2*y;return (float)y;}void clear(){x1=x2=0;}};
    std::array<Filter,2> dynamicFilter,detectorFilter;
    juce::AudioBuffer<float> memory;
    juce::Reverb reverb;
    std::array<double,63> hilbert{};
    int write=0,updateCount=0;double hz=44100,phase=0,holdPhase=1,holdFrames=0;
    float heldL=0,heldR=0,env=0,duckGain=1,lowL=0,lowR=0,highL=0,highR=0;
    std::array<double,2> res1{},res2{};
public:
    float reduction=0,detectorPeak=0;double delayMs=0;
    void prepare(BuiltinKind kind,double rate){hz=rate;int length=kind==BuiltinKind::tempoDelay?(int)std::ceil(rate*12)+4:kind==BuiltinKind::room?(int)std::ceil(rate*.1)+4:kind==BuiltinKind::frequencyShifter?64:4;memory.setSize(2,length);reverb.setSampleRate(rate);
        for(int n=0;n<63;++n){const int m=n-31;hilbert[(size_t)n]=m && std::abs(m)%2?2.0/(juce::MathConstants<double>::pi*m)*(.5-.5*std::cos(juce::MathConstants<double>::twoPi*n/62)):0;}reset();}
    void reset(){memory.clear();reverb.reset();write=updateCount=0;phase=0;holdPhase=1;holdFrames=0;heldL=heldR=env=lowL=lowR=highL=highR=0;duckGain=1;reduction=detectorPeak=0;res1={};res2={};for(auto* filters:{&dynamicFilter,&detectorFilter})for(auto& f:*filters)f.clear();}
    void begin(BuiltinKind kind,const std::vector<juce::AudioParameterFloat*>& controls){reduction=detectorPeak=0;if(kind==BuiltinKind::room){juce::Reverb::Parameters p;p.roomSize=controls[1]->get();p.damping=controls[2]->get();p.wetLevel=1;p.dryLevel=0;p.width=1;p.freezeMode=0;reverb.setParameters(p);}}
    void process(BuiltinKind kind,float& l,float& r,float scL,float scR,const std::array<float,16>& p,double bpm){
        auto gain=[](float db){return juce::Decibels::decibelsToGain(db);};
        auto follow=[&](float target,float attack,float release){float c=std::exp(-1.f/((float)hz*.001f*(target>env?attack:release)));env=c*env+(1-c)*target;return env;};
        const float dryL=l,dryR=r;const int length=memory.getNumSamples();
        auto read=[&](int channel,double samples){double at=write-samples;while(at<0)at+=length;at=std::fmod(at,(double)length);const int first=(int)at;return memory.getSample(channel,first)+(memory.getSample(channel,(first+1)%length)-memory.getSample(channel,first))*(float)(at-first);};
        switch(kind){
        case BuiltinKind::dynamicEq:{
            if(updateCount++%32==0){EqBand detector;detector.enabled=true;detector.type=4;detector.frequency=p[0];detector.q=p[1];for(auto& f:detectorFilter)f.c=eqCoefficients(detector,hz);
                float over=juce::Decibels::gainToDecibels(env,-120.f)-p[2];float amount=p[3]*juce::jlimit(0.f,1.f,over/12.f);EqBand band=detector;band.type=3;band.gain=amount;for(auto& f:dynamicFilter)f.c=eqCoefficients(band,hz);reduction=amount;}
            const float bandL=l-detectorFilter[0].run(l),bandR=r-detectorFilter[1].run(r);follow(juce::jmax(std::abs(bandL),std::abs(bandR)),p[4],p[5]);
            l=dynamicFilter[0].run(l);r=dynamicFilter[1].run(r);l=dryL+(l-dryL)*p[6]*.01f;r=dryR+(r-dryR)*p[6]*.01f;break;}
        case BuiltinKind::bitcrusher:{holdPhase+=juce::jmin((double)p[1],hz)/hz;if(holdPhase>=1){holdPhase-=std::floor(holdPhase);const float steps=std::pow(2.f,std::round(p[0])-1),drive=gain(p[2]);heldL=std::round(juce::jlimit(-1.f,1.f,l*drive)*steps)/steps;heldR=std::round(juce::jlimit(-1.f,1.f,r*drive)*steps)/steps;}const float mix=p[4]*.01f,out=gain(p[3]);l=(l+(heldL-l)*mix)*out;r=(r+(heldR-r)*mix)*out;break;}
        case BuiltinKind::tempoDelay:{constexpr double beats[]{.0625,.125,.25,.5,.75,1,1.5,2,4};delayMs=p[1]>=.5f?60000/std::clamp(bpm,20.0,999.0)*beats[juce::jlimit(0,8,(int)std::round(p[2]))]:p[0];const double frames=std::clamp(delayMs*.001*hz,1.0,(double)length-2);
            float wetL=read(0,frames),wetR=read(1,frames);float hc=1-std::exp(-juce::MathConstants<float>::twoPi*juce::jmin(p[5],(float)hz*.45f)/(float)hz),lc=1-std::exp(-juce::MathConstants<float>::twoPi*p[4]/(float)hz);highL+=hc*(wetL-highL);highR+=hc*(wetR-highR);lowL+=lc*(highL-lowL);lowR+=lc*(highR-lowR);wetL=highL-lowL;wetR=highR-lowR;
            const float swap=p[6],feedback=p[3]*.01f;memory.setSample(0,write,l+((1-swap)*wetL+swap*wetR)*feedback);memory.setSample(1,write,r+((1-swap)*wetR+swap*wetL)*feedback);write=(write+1)%length;l+= (wetL-l)*p[7]*.01f;r+=(wetR-r)*p[7]*.01f;break;}
        case BuiltinKind::ducker:{const float peak=juce::jmax(std::abs(scL),std::abs(scR));detectorPeak=juce::jmax(detectorPeak,peak);if(peak>gain(p[0]))holdFrames=p[3]*.001*hz;const bool duck=peak>gain(p[0]) || holdFrames-- >0;holdFrames=juce::jmax(0.0,holdFrames);const float target=duck?gain(-p[1]):1.f,c=std::exp(-1.f/((float)hz*.001f*(target<duckGain?p[2]:p[4])));duckGain=c*duckGain+(1-c)*target;const float g=1+(duckGain-1)*p[5]*.01f;l*=g;r*=g;reduction=juce::jmax(reduction,-juce::Decibels::gainToDecibels(g,-100.f));break;}
        case BuiltinKind::room:{const double frames=juce::jlimit(0.0,(double)length-2,p[0]*.001*hz);memory.setSample(0,write,l);memory.setSample(1,write,r);float wetL=read(0,frames),wetR=read(1,frames);write=(write+1)%length;const float c=1-std::exp(-juce::MathConstants<float>::twoPi*p[3]/(float)hz);lowL+=c*(wetL-lowL);lowR+=c*(wetR-lowR);wetL-=lowL;wetR-=lowR;reverb.processStereo(&wetL,&wetR,1);l+=(wetL-l)*p[4]*.01f;r+=(wetR-r)*p[4]*.01f;break;}
        case BuiltinKind::resonator:{const double radius=std::exp(-6.90775527898/(hz*p[1]*.001)),w=juce::MathConstants<double>::twoPi*juce::jmin((double)p[0],hz*.4)/hz;const float c=1-std::exp(-juce::MathConstants<float>::twoPi*juce::jmin(p[2],(float)hz*.45f)/(float)hz);lowL+=c*(l-lowL);lowR+=c*(r-lowR);float wet[2];for(int channel=0;channel<2;++channel){const double input=std::tanh((channel?lowR:lowL)*gain(p[3])),y=input+2*radius*std::cos(w)*res1[(size_t)channel]-radius*radius*res2[(size_t)channel];res2[(size_t)channel]=res1[(size_t)channel];res1[(size_t)channel]=y;wet[channel]=(float)(y*2*(1-radius)*std::sin(w));}l+=(wet[0]-l)*p[4]*.01f;r+=(wet[1]-r)*p[4]*.01f;break;}
        case BuiltinKind::frequencyShifter:{memory.setSample(0,write,l);memory.setSample(1,write,r);float wet[2];for(int channel=0;channel<2;++channel){double quadrature=0;for(int n=0;n<63;++n)quadrature+=hilbert[(size_t)n]*memory.getSample(channel,(write-n+64)%64);wet[channel]=(float)(read(channel,31)*std::cos(phase)-quadrature*std::sin(phase));}phase=std::remainder(phase+juce::MathConstants<double>::twoPi*p[0]/hz,juce::MathConstants<double>::twoPi);write=(write+1)%length; // Dry also shares the Hilbert group delay.
            l=(read(0,32)+(wet[0]-read(0,32))*p[2]*.01f)*gain(p[1]);r=(read(1,32)+(wet[1]-read(1,32))*p[2]*.01f)*gain(p[1]);break;}
        default:break;
        }
    }
};
}
