#pragma once
#include <juce_dsp/juce_dsp.h>
namespace drum {
// Single audio producer / single UI consumer. FFTs run only on the UI thread.
class SpectrumTap {
public:
    std::atomic<bool> enabled{false};std::atomic<double> rate{44100};
    void push(float l,float r) noexcept {if(!enabled.load(std::memory_order_relaxed))return;auto range=fifo.write(1);range.forEach([&](int i){left[(size_t)i]=l;right[(size_t)i]=r;});}
    template<class F> int consume(F&& fn){int count=fifo.getNumReady();auto range=fifo.read(count);range.forEach([&](int i){fn(left[(size_t)i],right[(size_t)i]);});return count;}
private:
    juce::AbstractFifo fifo{8192};std::array<float,8192> left{},right{};
};
class SpectrumAnalysis {
public:
    static constexpr int size=2048;
    SpectrumAnalysis(){db.fill(-100);}
    void clear(SpectrumTap& tap){tap.consume([](float,float){});left.fill(0);right.fill(0);db.fill(-100);cursor=filled=0;}
    bool update(SpectrumTap& tap){
        auto count=tap.consume([&](float l,float r){left[(size_t)cursor]=std::isfinite(l)?l:0;right[(size_t)cursor]=std::isfinite(r)?r:0;cursor=(cursor+1)%size;filled=juce::jmin(size,filled+1);});
        if(count==0){for(auto& v:db)v=juce::jmax(-100.f,v-3.f);return true;}if(filled<size)return false;
        for(int channel=0;channel<2;++channel){scratch.fill(0);for(int n=0;n<size;++n)scratch[(size_t)n]=(channel?right:left)[(size_t)((cursor+n)%size)];window.multiplyWithWindowingTable(scratch.data(),size);fft.performFrequencyOnlyForwardTransform(scratch.data());for(int n=0;n<=size/2;++n){float magnitude=scratch[(size_t)n]*2.f/size;if(channel==0)power[(size_t)n]=magnitude*magnitude;else {float level=juce::Decibels::gainToDecibels(std::sqrt((power[(size_t)n]+magnitude*magnitude)*.5f),-100.f);db[(size_t)n]=juce::jmax(level,db[(size_t)n]-3.f);}}}return true;
    }
    // Interpolate FFT bins, then average a narrow logarithmic frequency neighbourhood.
    // This removes stair steps without changing the measured FFT values.
    float levelAt(double hz,double rate) const {
        if(rate<=0 || hz>=rate*.5)return -100;
        auto interpolate=[&](double f){double bin=juce::jlimit(0.0,(double)size/2,f*size/rate);int lo=(int)bin,hi=juce::jmin(size/2,lo+1);return db[(size_t)lo]+(float)(bin-lo)*(db[(size_t)hi]-db[(size_t)lo]);};
        float sum=0,weight=0;for(int n=-4;n<=4;++n){float w=5.f-std::abs(n);sum+=w*interpolate(hz*std::pow(2.0,n/48.0));weight+=w;}return sum/weight;
    }
    std::array<float,size/2+1> db;
private:
    juce::dsp::FFT fft{11};juce::dsp::WindowingFunction<float> window{size,juce::dsp::WindowingFunction<float>::hann,true};
    std::array<float,size> left{},right{};std::array<float,size*2> scratch{};std::array<float,size/2+1> power{};int cursor=0,filled=0;
};
}
