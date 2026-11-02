#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>

namespace drum
{
// Fixed storage throughout: coefficient changes and processing allocate nothing.
enum class EqType { lowCut12,lowCut48,lowShelf,bell,notch,highShelf,highCut12,highCut48 };
struct EqBand
{
    bool enabled=false;
    int type=(int)EqType::bell;
    double frequency=1000,gain=0,q=0.70710678;
    int slope=0; // 0 preserves legacy type-specific 12/48 dB slopes.
    bool operator==(const EqBand&) const = default;
};
struct Equalizer
{
    bool enabled=false;
    double output=0;
    std::array<EqBand,16> bands=[] {
        std::array<EqBand,16> b{};
        constexpr double frequencies[]{40,100,250,600,1500,3500,8000,16000,60,160,400,950,2300,5300,11000,19000};
        for(int n=0;n<16;++n)b[(size_t)n].frequency=frequencies[n];
        return b;
    }();
    bool operator==(const Equalizer&) const = default;
};
inline bool validEqualizer(const Equalizer& e)
{
    if(!std::isfinite(e.output) || e.output < -24 || e.output > 24)return false;
    for(auto& b:e.bands)if(b.type<0 || b.type>7 || !std::isfinite(b.frequency) || b.frequency<20 || b.frequency>20000
        || !std::isfinite(b.gain) || b.gain < -60 || b.gain>24 || !std::isfinite(b.q) || b.q<0.1 || b.q>18 || (b.slope!=0 && (b.slope<6 || b.slope>72 || b.slope%6!=0)))return false;
    return true;
}
inline bool eqHasGain(int type) {return type==2 || type==3 || type==5;}
inline bool eqIsCut(int type){return type==0 || type==1 || type==6 || type==7;}
inline int eqSlope(const EqBand& b){return b.slope?b.slope:(b.type==1 || b.type==7?48:12);}
inline int eqStages(const EqBand& b){return b.enabled?(eqIsCut(b.type)?(eqSlope(b)/6+1)/2:1):0;}
struct EqCoefficients
{
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    double magnitude(double frequency,double rate) const
    {
        const auto z=std::polar(1.0,-6.283185307179586*frequency/rate);
        return std::abs((b0+b1*z+b2*z*z)/(1.0+a1*z+a2*z*z));
    }
};
inline EqCoefficients eqCoefficients(const EqBand& b,double rate,int stage=0)
{
    if(!b.enabled)return {};
    const double w=6.283185307179586*std::clamp(b.frequency,10.0,rate*0.45)/rate,c=std::cos(w),s=std::sin(w);
    const int order=eqSlope(b)/6, pairs=order/2;
    if(eqIsCut(b.type) && order%2 && stage==pairs){
        const double k=std::tan(w/2), norm=1/(1+k);
        if(b.type==0 || b.type==1)return {norm,-norm,0,(k-1)*norm,0};
        return {k*norm,k*norm,0,(k-1)*norm,0};
    }
    const double sectionQ=order==2?b.q:(1/(2*std::cos((2*stage+1+order%2)*3.141592653589793/(2*order))))*(stage==pairs-1?b.q/0.70710678:1.0);
    const double q=eqIsCut(b.type)?sectionQ:b.q,alpha=s/(2*q),A=std::pow(10.0,b.gain/40),beta=2*std::sqrt(A)*alpha;
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    switch((EqType)b.type)
    {
        case EqType::lowCut12:case EqType::lowCut48: b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case EqType::highCut12:case EqType::highCut48:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case EqType::bell:b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;break;
        case EqType::notch:b0=1;b1=-2*c;b2=1;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
        case EqType::lowShelf:
            b0=A*((A+1)-(A-1)*c+beta);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-beta);
            a0=(A+1)+(A-1)*c+beta;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-beta;break;
        case EqType::highShelf:
            b0=A*((A+1)+(A-1)*c+beta);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-beta);
            a0=(A+1)-(A-1)*c+beta;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-beta;break;
    }
    return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
inline double eqMagnitude(const Equalizer& e,double frequency,double rate)
{
    if(!e.enabled)return 1;
    double magnitude=std::pow(10.0,e.output/20);
    for(auto& b:e.bands)for(int stage=0;stage<eqStages(b);++stage)magnitude*=eqCoefficients(b,rate,stage).magnitude(frequency,rate);
    return magnitude;
}
class VoiceEqualizer
{
    struct Section
    {
        EqCoefficients c,target;int remaining=0;
        std::array<double,2> z1{},z2{};
        double process(double input,int channel)
        {
            if(channel==0 && remaining>0){const double f=1.0/remaining--;c.b0+=(target.b0-c.b0)*f;c.b1+=(target.b1-c.b1)*f;c.b2+=(target.b2-c.b2)*f;c.a1+=(target.a1-c.a1)*f;c.a2+=(target.a2-c.a2)*f;}
            const auto n=(size_t)channel;const double y=c.b0*input+z1[n];
            z1[n]=c.b1*input-c.a1*y+z2[n];z2[n]=c.b2*input-c.a2*y;
            return y;
        }
    };
    struct Bank
    {
        std::array<Section,96> sections{};int count=0,gainRemaining=0;double gain=1,targetGain=1;
        void configure(const Equalizer& eq,double rate)
        {
            *this={};if(!eq.enabled)return;
            gain=std::pow(10.0,eq.output/20);
            for(auto& b:eq.bands)for(int stage=0;stage<eqStages(b);++stage){auto& section=sections[(size_t)count++];section.c=section.target=eqCoefficients(b,rate,stage);}
        }
        void retarget(const Equalizer& eq,double rate){int n=0;const int length=std::max(1,(int)(rate*.02));if(eq.enabled)for(auto& b:eq.bands)for(int stage=0;stage<eqStages(b);++stage){auto& section=sections[(size_t)n++];section.target=eqCoefficients(b,rate,stage);section.remaining=length;}targetGain=eq.enabled?std::pow(10.0,eq.output/20):1;gainRemaining=length;}
        double process(double input,int channel)
        {if(channel==0 && gainRemaining>0)gain+=(targetGain-gain)/gainRemaining--;for(int n=0;n<count;++n)input=sections[(size_t)n].process(input,channel);return input*gain;}
    };
public:
    void reset(const Equalizer& eq,double rate) {settings=eq;current.configure(eq,rate);remaining=0;}
    void update(const Equalizer& eq,double rate)
    {
        if(eq==settings)return;
        bool topology=eq.enabled==settings.enabled;if(eq.enabled)for(size_t n=0;n<eq.bands.size();++n){const auto& before=settings.bands[n];const auto& after=eq.bands[n];topology&=before.enabled==after.enabled && before.type==after.type && eqStages(before)==eqStages(after);}
        if(topology)current.retarget(eq,rate);else{previous=current;current.configure(eq,rate);remaining=length=std::max(1,(int)(rate*0.01));}settings=eq;
    }
    void process(float& left,float& right)
    {
        // Extreme stacked resonances can exceed float range. Keep the host and
        // subsequent voice sums finite; this guard is far above usable audio.
        auto finite=[](double v){return std::isfinite(v)?(float)std::clamp(v,-1.0e12,1.0e12):0.0f;};
        const double l=current.process(left,0),r=current.process(right,1);
        if(remaining>0)
        {
            const double mix=1.0-(double)remaining--/length;
            left=finite(previous.process(left,0)*(1-mix)+l*mix);right=finite(previous.process(right,1)*(1-mix)+r*mix);
        }
        else {left=finite(l);right=finite(r);}
    }
private:
    Equalizer settings;Bank current,previous;int remaining=0,length=1;
};
}
