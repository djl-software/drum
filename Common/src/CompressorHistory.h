#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
namespace drum {
// Single audio producer / editor consumer. Fixed storage and 200 Hz peak buckets
// preserve transients independently of host block size and UI refresh rate.
class CompressorHistory {
public:
    struct Frame {float inputL=0,inputR=0,outputL=0,outputR=0,reduction=0;};
    std::atomic<bool> enabled{false};
    void prepare(double rate){stride=juce::jmax(1,juce::roundToInt(rate/200));count=0;bucket={};fifo.reset();}
    void push(float il,float ir,float ol,float or_,float gr) noexcept {
        if(!enabled.load(std::memory_order_relaxed)){count=0;bucket={};return;}
        bucket.inputL=juce::jmax(bucket.inputL,std::abs(il));bucket.inputR=juce::jmax(bucket.inputR,std::abs(ir));
        bucket.outputL=juce::jmax(bucket.outputL,std::abs(ol));bucket.outputR=juce::jmax(bucket.outputR,std::abs(or_));if(std::abs(gr)>std::abs(bucket.reduction))bucket.reduction=gr;
        if(++count<stride)return;
        int a,n,b,m;fifo.prepareToWrite(1,a,n,b,m);if(n){frames[(size_t)a]=bucket;fifo.finishedWrite(1);}count=0;bucket={};
    }
    template<class F> void consume(F&& f){int a,n,b,m;fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);for(int i=0;i<n;++i)f(frames[(size_t)(a+i)]);for(int i=0;i<m;++i)f(frames[(size_t)(b+i)]);fifo.finishedRead(n+m);}
private:
    juce::AbstractFifo fifo{4096};std::array<Frame,4096> frames{};Frame bucket;int count=0,stride=240;
};
}
