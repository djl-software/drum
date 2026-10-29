#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace drum
{
constexpr int maxEnvelopePoints=32;
struct EnvelopePoint
{
    double ms=0,level=0,bend=0,pivot=0.5,weight=1;
    bool operator==(const EnvelopePoint&) const = default;
};
struct EnvelopeShape
{
    int count=0,hold=0; // Zero count retains the original ADSR implementation.
    std::array<EnvelopePoint,maxEnvelopePoints> points {};
    bool operator==(const EnvelopeShape&) const = default;
};
inline bool validEnvelope(const EnvelopeShape& s)
{
    if(s.count==0)return s.hold==0;
    if(s.count<2 || s.count>maxEnvelopePoints || s.hold<0 || s.hold>=s.count-1)return false;
    double previous=0;
    for(int n=0;n<s.count;++n)
    {
        const auto& p=s.points[(size_t)n];
        if(!std::isfinite(p.ms) || !std::isfinite(p.level) || !std::isfinite(p.bend) || !std::isfinite(p.pivot) || !std::isfinite(p.weight)
           || p.ms<previous || p.ms>80000 || p.level<0 || p.level>1 || std::abs(p.bend)>1 || p.pivot<0.0001 || p.pivot>0.9999 || p.weight<1 || p.weight>10000)return false;
        previous=p.ms;
    }
    return true;
}
inline EnvelopeShape adsrShape(double a,double d,double s,double r)
{
    EnvelopeShape result;result.count=4;result.hold=2;
    result.points[0]={0,0};result.points[1]={a,1};result.points[2]={a+d,s};result.points[3]={a+d+r,0};return result;
}
inline double envelopeParameter(double x,double pivot,double weight=1)
{
    if(x<=0)return 0;if(x>=1)return 1;
    if(weight==1)return x/(pivot+std::sqrt(std::max(0.0,pivot*pivot+(1-2*pivot)*x)));
    // Invert the rational quadratic's monotonic time coordinate. The stable
    // quadratic formula avoids cancellation for curves pulled near an edge.
    const double a=1-2*weight*pivot-x*(2-2*weight),b=2*weight*pivot-2*x*(weight-1);
    if(std::abs(a)<1e-12)return x/b;
    const double q=-0.5*(b+std::copysign(std::sqrt(std::max(0.0,b*b+4*a*x)),b));
    const double first=q/a,second=-x/q;
    return std::clamp(first>=0 && first<=1?first:second,0.0,1.0);
}
inline double envelopeControl(double a,const EnvelopePoint& b)
{return std::clamp(a+(b.level-a)*b.pivot+b.bend,0.0,1.0);}
inline double envelopeSegment(double a,const EnvelopePoint& b,double t)
{
    t=std::clamp(t,0.0,1.0);
    if(b.bend==0)return a+(b.level-a)*t;
    const double u=envelopeParameter(t,b.pivot,b.weight),v=1-u;
    return (v*v*a+2*v*u*b.weight*envelopeControl(a,b)+u*u*b.level)/(v*v+2*v*u*b.weight+u*u);
}
inline void bendEnvelopeSegment(double left,EnvelopePoint& right,double fraction,double level)
{
    const double t=std::clamp(fraction,0.02,0.98),target=std::clamp(level,0.0001,0.9999);
    const double u=std::sqrt(t)/(std::sqrt(t)+std::sqrt(1-t)),v=1-u;
    const double base=v*v*left+u*u*right.level,ends=v*v+u*u,cross=2*u*v;
    // A weighted control point lets the grabbed part follow the pointer through
    // the full level range, without an out-of-range control or clipped plateau.
    double weight=1;
    if(base>target*ends)weight=std::max(weight,(base-target*ends)/(target*cross));
    else weight=std::max(weight,(target*ends-base)/((1-target)*cross));
    right.weight=std::clamp(weight,1.0,10000.0);right.pivot=t;
    const double control=std::clamp(target+(target*ends-base)/(cross*right.weight),0.0,1.0);
    right.bend=control-(left+(right.level-left)*t);
}
inline bool insertEnvelopePoint(EnvelopeShape& s,int right,double ms)
{
    if(s.count>=maxEnvelopePoints || right<1 || right>=s.count)return false;
    const auto a=s.points[(size_t)right-1],b=s.points[(size_t)right];
    if(b.ms-a.ms<=0.02)return false;
    ms=std::clamp(ms,a.ms+0.01,b.ms-0.01);
    const double x=(ms-a.ms)/(b.ms-a.ms),u=envelopeParameter(x,b.pivot,b.weight),v=1-u,control=envelopeControl(a.level,b);
    const double leftW=v+u*b.weight,rightW=v*b.weight+u,middleW=v*leftW+u*rightW;
    const double leftX=u*b.weight*b.pivot/leftW,leftY=(v*a.level+u*b.weight*control)/leftW;
    const double rightX=(v*b.weight*b.pivot+u)/rightW,rightY=(v*b.weight*control+u*b.level)/rightW;
    const double level=(v*leftW*leftY+u*rightW*rightY)/middleW;
    EnvelopePoint added{ms,level};added.pivot=std::clamp(leftX/x,0.0001,0.9999);added.bend=leftY-(a.level+(level-a.level)*added.pivot);added.weight=std::clamp(leftW/std::sqrt(middleW),1.0,10000.0);
    auto changed=b;changed.pivot=std::clamp((rightX-x)/(1-x),0.0001,0.9999);changed.bend=rightY-(level+(b.level-level)*changed.pivot);changed.weight=std::clamp(rightW/std::sqrt(middleW),1.0,10000.0);
    for(int n=s.count;n>right;--n)s.points[(size_t)n]=s.points[(size_t)n-1];
    s.points[(size_t)right]=added;s.points[(size_t)right+1]=changed;++s.count;if(right<=s.hold)++s.hold;return true;
}
inline double envelopeValue(const EnvelopeShape& s,double ms,int& segment,int overrideIndex=-1,double overrideLevel=0)
{
    if(s.count<2)return 0;
    if(ms<s.points[0].ms)return s.points[0].level*ms/std::max(0.00001,s.points[0].ms);
    if(ms>=s.points[(size_t)s.count-1].ms)return s.points[(size_t)s.count-1].level;
    segment=std::clamp(segment,1,s.count-1);
    while(segment<s.count-1 && ms>=s.points[(size_t)segment].ms)++segment;
    while(segment>1 && ms<s.points[(size_t)segment-1].ms)--segment;
    const auto& a=s.points[(size_t)segment-1];const auto& b=s.points[(size_t)segment];
    return envelopeSegment(segment-1==overrideIndex?overrideLevel:a.level,b,(ms-a.ms)/std::max(0.00001,b.ms-a.ms));
}
inline double envelopeValue(const EnvelopeShape& s,double ms) {int segment=1;return envelopeValue(s,ms,segment);}
}
