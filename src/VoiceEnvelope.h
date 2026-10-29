#pragma once
#include "EnvelopeShape.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace drum
{
// Fixed storage; legacy projects keep JUCE's original ADSR path.
class VoiceEnvelope
{
public:
    void setSampleRate(double value) {rate=value;legacy.setSampleRate(value);}
    void setParameters(juce::ADSR::Parameters p) {legacy.setParameters(p);}
    void setShape(const EnvelopeShape& value)
    {
        if(shape==value)return;
        shape=value;segment=1;
        if(running) {transitionStart=last;transition=transitionLength=std::max(1,(int)(rate*0.005));}
    }
    void reset() {legacy.reset();running=false;released=false;elapsed=releaseElapsed=last=0;segment=1;transition=0;}
    void noteOn() {legacy.noteOn();running=true;released=false;elapsed=releaseElapsed=last=0;segment=1;transition=0;}
    void noteOff() {legacy.noteOff();if(!released) {released=true;releaseElapsed=0;releaseFrom=last;segment=shape.hold+1;}}
    bool isActive() const {return shape.count==0?legacy.isActive():running;}
    float getNextSample()
    {
        const auto legacyValue=legacy.getNextSample();elapsed+=1000/rate;if(released)releaseElapsed+=1000/rate;
        if(shape.count==0) {last=legacyValue;return legacyValue;}
        if(!running)return 0;
        const auto holdMs=shape.points[(size_t)shape.hold].ms;double value;
        if(!released)value=envelopeValue(shape,std::min(elapsed,holdMs),segment);
        else
        {
            const double time=holdMs+releaseElapsed,end=shape.points[(size_t)shape.count-1].ms;
            if(time>=end+2) {running=false;value=0;}
            else if(time>end)value=shape.points[(size_t)shape.count-1].level*(1-(time-end)/2);
            else value=envelopeValue(shape,time,segment,shape.hold,releaseFrom);
        }
        if(transition>0) {const double fraction=1.0-(double)--transition/transitionLength;value=transitionStart+(value-transitionStart)*fraction;}
        last=value;return (float)value;
    }
private:
    juce::ADSR legacy;
    EnvelopeShape shape;
    double rate=44100,elapsed=0,releaseElapsed=0,releaseFrom=0,last=0,transitionStart=0;
    int segment=1,transition=0,transitionLength=1;
    bool running=false,released=false;
};
}
