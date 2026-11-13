#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
namespace drum {
inline void retainMaximum(std::atomic<float>& target,float peak) noexcept {float previous=target.load(std::memory_order_relaxed);while(peak>previous && !target.compare_exchange_weak(previous,peak,std::memory_order_relaxed)){} }
struct MeterPeakHold {
    float value=0;double elapsed=0;
    float process(float peak,int frames,double rate,double seconds) noexcept {
        if(peak>=value){value=peak;elapsed=0;}
        else {double before=elapsed;elapsed+=frames/rate;double falling=juce::jmax(0.0,elapsed-seconds)-juce::jmax(0.0,before-seconds);if(falling>0)value=juce::jmax(peak,value*(float)std::exp(-falling/.35));}
        return value;
    }
};
}
