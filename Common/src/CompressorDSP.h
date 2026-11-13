#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
namespace drum {
// Independent implementation of the familiar compressor controls. No allocation
// or locks in process(); parameter indices 0-7 preserve existing DRUM projects.
class CompressorDSP {
public:
    struct Result {float left,right,dryL,dryR,change,detector;};
    void prepare(double hz,int lookahead){rate=hz;delay.setSize(2,(int)std::ceil(rate*.01)+2);blend.reset(rate,.005);listenBlend.reset(rate,.005);reset(lookahead);}
    void reset(int lookahead){delay.clear();write=0;activeDelay=oldDelay=samples(lookahead);blend.setCurrentAndTargetValue(1);listenBlend.setCurrentAndTargetValue(0);env=power=programme=gainDb=lowL=lowR=0;lastMask=-1;transition=0;lastGain=1;ic1={};ic2={};}
    int samples(int choice) const{return juce::roundToInt(rate*(choice==1?.001:choice==2?.01:0));}
    static float amount(float over,float knee){if(knee>0 && over>-knee*.5f && over<knee*.5f)return (over+knee*.5f)*(over+knee*.5f)/(2*knee);return juce::jmax(0.f,over);}
    static float change(float inputDb,float threshold,float ratio,float knee,int mode){float a=amount(inputDb-threshold,knee);return mode==2?-juce::jmin(36.f,a*(ratio-1)):a*(1-1/ratio);}
    static float makeup(float threshold,float ratio,float knee,int mode,bool automatic,bool external){return automatic && !external && mode!=2?juce::jlimit(0.f,24.f,change(0,threshold,ratio,knee,mode)):0;}
    Result process(float l,float r,float externalL,float externalR,const std::array<float,32>& p,bool listen){
        const int mode=juce::jlimit(0,2,(int)std::round(p[8])),requested=samples(juce::jlimit(0,2,(int)std::round(p[11])));
        if(requested!=activeDelay){oldDelay=activeDelay;activeDelay=requested;blend.setCurrentAndTargetValue(0);blend.setTargetValue(1);}
        delay.setSample(0,write,l);delay.setSample(1,write,r);const float fade=blend.getNextValue();
        auto read=[&](int c){auto tap=[&](int d){return delay.getSample(c,(write-d+delay.getNumSamples())%delay.getNumSamples());};return tap(oldDelay)+(tap(activeDelay)-tap(oldDelay))*fade;};const float dryL=read(0),dryR=read(1);write=(write+1)%delay.getNumSamples();
        const bool external=p[13]>=.5f;const float scGain=juce::Decibels::decibelsToGain(p[14]),scMix=p[15]*.01f;
        float dl=external?l+(externalL*scGain-l)*scMix:l,dr=external?r+(externalR*scGain-r)*scMix:r;
        if(p[7]>0){const float hp=1-std::exp(-juce::MathConstants<float>::twoPi*p[7]/(float)rate);lowL+=hp*(dl-lowL);lowR+=hp*(dr-lowR);dl-=lowL;dr-=lowR;}
        if(p[16]>=.5f){const float g=std::tan(juce::MathConstants<float>::pi*juce::jlimit(10.f,(float)rate*.45f,p[18])/(float)rate),k=1/p[19],den=1/(1+g*(g+k));auto filter=[&](float in,int c){const float v1=(ic1[(size_t)c]+g*(in-ic2[(size_t)c]))*den,v2=ic2[(size_t)c]+g*v1;ic1[(size_t)c]=2*v1-ic1[(size_t)c];ic2[(size_t)c]=2*v2-ic2[(size_t)c];const float high=in-k*v1-v2;switch((int)std::round(p[17])){case 0:return high;case 1:return v2;case 2:return v1;default:return high+v2;}};dl=filter(dl,0);dr=filter(dr,1);}
        const float peak=juce::jmax(std::abs(dl),std::abs(dr));power+=(float)(1-std::exp(-1/(rate*.01)))*(dl*dl+dr*dr-power*2)*.5f;const float detector=mode==1?std::sqrt(juce::jmax(0.f,power)):peak;
        programme+=(float)(1-std::exp(-1/(rate*.3)))*(detector-programme);float release=p[9]>=.5f?50+450*juce::jlimit(0.f,1.f,programme/juce::jmax(.001f,env)):p[3];
        auto coefficient=[&](float ms){return std::exp(-1.f/(.001f*juce::jmax(.01f,ms)*(float)rate));};
        const float c=coefficient(detector>env?p[2]:release);env=c*env+(1-c)*detector;
        const float desired=change(juce::Decibels::gainToDecibels(detector,-120.f),p[0],p[1],p[6],mode);const float logRelease=release/(1+std::abs(gainDb)/12);const float gc=coefficient(std::abs(desired)>std::abs(gainDb)?p[2]:logRelease);gainDb=gc*gainDb+(1-gc)*desired;
        const float reduction=p[10]>=.5f?gainDb:change(juce::Decibels::gainToDecibels(env,-120.f),p[0],p[1],p[6],mode);
        float gain=juce::Decibels::decibelsToGain(juce::jlimit(-120.f,60.f,-reduction+p[4]+makeup(p[0],p[1],p[6],mode,p[12]>=.5f,external))),mix=p[5]*.01f;
        const int mask=mode+4*(p[10]>=.5f)+8*(p[12]>=.5f)+16*external+32*(p[16]>=.5f)+64*(int)std::round(p[17]);if(lastMask>=0 && mask!=lastMask){transition=juce::jmax(1,(int)(rate*.005));transitionLength=transition;transitionFrom=lastGain;}lastMask=mask;if(transition>0){const float t=1-(float)--transition/transitionLength;gain=transitionFrom+(gain-transitionFrom)*t;}lastGain=gain;
        float ol=dryL*(1+mix*(gain-1)),orr=dryR*(1+mix*(gain-1));listenBlend.setTargetValue(listen?1.f:0.f);const float audition=listenBlend.getNextValue();ol+=(dl-ol)*audition;orr+=(dr-orr)*audition;
        return {ol,orr,dryL,dryR,reduction,detector};
    }
private:
    double rate=44100;juce::AudioBuffer<float> delay;int write=0,activeDelay=0,oldDelay=0;juce::SmoothedValue<float> blend,listenBlend;
    int lastMask=-1,transition=0,transitionLength=1;float lastGain=1,transitionFrom=1;
    float env=0,power=0,programme=0,gainDb=0,lowL=0,lowR=0;std::array<float,2> ic1{},ic2{};
};
}
