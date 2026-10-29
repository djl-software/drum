#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace drum
{
constexpr int maxEnvelopePoints=32;
struct EnvelopePoint
{
    double ms=0,level=0,bend=0,pivot=0.5;
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
        if(!std::isfinite(p.ms) || !std::isfinite(p.level) || !std::isfinite(p.bend) || !std::isfinite(p.pivot)
           || p.ms<previous || p.ms>80000 || p.level<0 || p.level>1 || std::abs(p.bend)>1 || p.pivot<0.0001 || p.pivot>0.9999)return false;
        previous=p.ms;
    }
    return true;
}
inline EnvelopeShape adsrShape(double a,double d,double s,double r)
{
    EnvelopeShape result;result.count=4;result.hold=2;
    result.points[0]={0,0};result.points[1]={a,1};result.points[2]={a+d,s};result.points[3]={a+d+r,0};return result;
}
inline double envelopeParameter(double x,double pivot)
{return x/(pivot+std::sqrt(std::max(0.0,pivot*pivot+(1-2*pivot)*x)));}
inline double envelopeControl(double a,const EnvelopePoint& b)
{return std::clamp(a+(b.level-a)*b.pivot+b.bend,0.0,1.0);}
inline double envelopeSegment(double a,const EnvelopePoint& b,double t)
{
    t=std::clamp(t,0.0,1.0);
    if(b.bend==0)return a+(b.level-a)*t;
    const double u=envelopeParameter(t,b.pivot),v=1-u;
    return v*v*a+2*v*u*envelopeControl(a,b)+u*u*b.level;
}
inline bool insertEnvelopePoint(EnvelopeShape& s,int right,double ms)
{
    if(s.count>=maxEnvelopePoints || right<1 || right>=s.count)return false;
    const auto a=s.points[(size_t)right-1],b=s.points[(size_t)right];
    if(b.ms-a.ms<=0.02)return false;
    ms=std::clamp(ms,a.ms+0.01,b.ms-0.01);
    const double x=(ms-a.ms)/(b.ms-a.ms),u=envelopeParameter(x,b.pivot),control=envelopeControl(a.level,b);
    const double leftX=u*b.pivot,leftY=a.level+u*(control-a.level);
    const double rightX=b.pivot+u*(1-b.pivot),rightY=control+u*(b.level-control);
    const double level=leftY+u*(rightY-leftY);
    EnvelopePoint added{ms,level};added.pivot=std::clamp(leftX/x,0.0001,0.9999);added.bend=leftY-(a.level+(level-a.level)*added.pivot);
    auto changed=b;changed.pivot=std::clamp((rightX-x)/(1-x),0.0001,0.9999);changed.bend=rightY-(level+(b.level-level)*changed.pivot);
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
