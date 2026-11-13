#pragma once
#include "EnvelopeShape.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace drum
{
// Fixed storage; legacy projects keep JUCE's original ADSR path.
class VoiceEnvelope
{
public:
    void setSampleRate(double value) {rate=value;legacy.setSampleRate(value);bypassMix.reset(value,.005);}
    void setParameters(juce::ADSR::Parameters p) {
        if(!parametersKnown || p.attack!=parameters.attack || p.decay!=parameters.decay || p.sustain!=parameters.sustain || p.release!=parameters.release){legacy.setParameters(p);parameters=p;parametersKnown=true;}
    }
    void setBypassed(bool value) {bypassed=value;bypassMix.setTargetValue(value?1.f:0.f);}
    void setShape(const EnvelopeShape& value)
    {
        if(shape==value)return;
        shape=value;segment=1;
        if(running) {transitionStart=last;transition=transitionLength=std::max(1,(int)(rate*0.005));}
    }
    void reset() {bypassMix.setCurrentAndTargetValue(bypassed?1.f:0.f);legacy.reset();running=false;released=false;elapsed=releaseElapsed=last=0;segment=1;transition=0;}
    void noteOn() {bypassMix.setCurrentAndTargetValue(bypassed?1.f:0.f);legacy.noteOn();running=true;released=false;elapsed=releaseElapsed=last=0;segment=1;transition=0;}
    void noteOff() {legacy.noteOff();if(!released) {released=true;releaseElapsed=0;releaseFrom=last;segment=shape.hold+1;}}
    bool isActive() const {
        if(bypassed || bypassMix.getCurrentValue()>0)return true;
        return bypassMix.getCurrentValue()<1 && (shape.count==0?legacy.isActive():running);
    }
    float getNextSample()
    {
        const float shaped=nextShapedSample();
        const float neutral=1.f;
        const float mix=bypassMix.getNextValue();return shaped+(neutral-shaped)*mix;
    }
private:
    float nextShapedSample()
    {
        const auto legacyValue=legacy.getNextSample();elapsed+=1000/rate;if(released)releaseElapsed+=1000/rate;
        if(shape.count==0) {last=legacyValue;return legacyValue;}
        if(!running)return 0;
        // A free curve is a timeline over the sample, not a MIDI gate or
        // a playback-length control. Hold its final value beyond the last point.
        const auto holdMs=shape.points[(size_t)shape.hold].ms;double value;
        if(shape.hold==shape.count-1)value=envelopeValue(shape,elapsed,segment);
        else if(!released)value=envelopeValue(shape,std::min(elapsed,holdMs),segment);
        else
        {
            const double time=holdMs+releaseElapsed,end=shape.points[(size_t)shape.count-1].ms;
            if(time>=end+2) {running=false;value=0;}
            else if(time>end)value=(shape.hold==shape.count-1?releaseFrom:shape.points[(size_t)shape.count-1].level)*(1-(time-end)/2);
            else value=envelopeValue(shape,time,segment,shape.hold,releaseFrom);
        }
        if(transition>0) {const double fraction=1.0-(double)--transition/transitionLength;value=transitionStart+(value-transitionStart)*fraction;}
        last=value;return (float)value;
    }
private:
    juce::ADSR legacy;
    juce::ADSR::Parameters parameters;
    juce::SmoothedValue<float> bypassMix;
    bool parametersKnown=false,bypassed=false;
    EnvelopeShape shape;
    double rate=44100,elapsed=0,releaseElapsed=0,releaseFrom=0,last=0,transitionStart=0;
    int segment=1,transition=0,transitionLength=1;
    bool running=false,released=false;
};
}
