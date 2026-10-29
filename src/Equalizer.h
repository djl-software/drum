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
    bool operator==(const EqBand&) const = default;
};
struct Equalizer
{
    bool enabled=false;
    double output=0;
    std::array<EqBand,8> bands=[] {
        std::array<EqBand,8> b{};
        constexpr double frequencies[]{40,100,250,600,1500,3500,8000,16000};
        for(int n=0;n<8;++n)b[(size_t)n].frequency=frequencies[n];
        return b;
    }();
    bool operator==(const Equalizer&) const = default;
};
inline bool validEqualizer(const Equalizer& e)
{
    if(!std::isfinite(e.output) || e.output < -24 || e.output > 24)return false;
    for(auto& b:e.bands)if(b.type<0 || b.type>7 || !std::isfinite(b.frequency) || b.frequency<20 || b.frequency>20000
        || !std::isfinite(b.gain) || b.gain < -24 || b.gain>24 || !std::isfinite(b.q) || b.q<0.1 || b.q>18)return false;
    return true;
}
inline bool eqHasGain(int type) {return type==2 || type==3 || type==5;}
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
    constexpr double butterworth[]{0.5097955791,0.6013448869,0.8999762231,2.5629154477};
    const bool steep=b.type==1 || b.type==7;
    const double q=steep?butterworth[stage]*(stage==3?b.q/0.70710678:1.0):b.q,alpha=s/(2*q),A=std::pow(10.0,b.gain/40),beta=2*std::sqrt(A)*alpha;
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
inline int eqStages(const EqBand& b) {return b.enabled?(b.type==1 || b.type==7?4:1):0;}
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
        EqCoefficients c;
        std::array<double,2> z1{},z2{};
        double process(double input,int channel)
        {
            const auto n=(size_t)channel;const double y=c.b0*input+z1[n];
            z1[n]=c.b1*input-c.a1*y+z2[n];z2[n]=c.b2*input-c.a2*y;
            return y;
        }
    };
    struct Bank
    {
        std::array<Section,32> sections{};int count=0;double gain=1;
        void configure(const Equalizer& eq,double rate)
        {
            *this={};if(!eq.enabled)return;
            gain=std::pow(10.0,eq.output/20);
            for(auto& b:eq.bands)for(int stage=0;stage<eqStages(b);++stage)sections[(size_t)count++].c=eqCoefficients(b,rate,stage);
        }
        double process(double input,int channel)
        {for(int n=0;n<count;++n)input=sections[(size_t)n].process(input,channel);return input*gain;}
    };
public:
    void reset(const Equalizer& eq,double rate) {settings=eq;current.configure(eq,rate);remaining=0;}
    void update(const Equalizer& eq,double rate)
    {
        if(eq==settings)return;
        previous=current;current.configure(eq,rate);settings=eq;remaining=length=std::max(1,(int)(rate*0.005));
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
