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
    juce::AbstractFifo fifo{32768};std::vector<float> left=std::vector<float>(32768),right=std::vector<float>(32768);
};
class SpectrumAnalysis {
public:
    static constexpr int size=8192;
    SpectrumAnalysis(){db.fill(-100);}
    void clear(SpectrumTap& tap){tap.consume([](float,float){});left.fill(0);right.fill(0);db.fill(-100);signalPeakDb=-100;cursor=filled=hop=peakHold=0;}
    bool update(SpectrumTap& tap){
        // Decay once per screen update, not once per FFT. Scan every overlapping
        // window in the queue so a hit cannot be discarded by a later silent tail.
        for(auto& v:db)v=juce::jmax(-100.f,v-3.f);
        if(peakHold>0)--peakHold;else signalPeakDb=juce::jmax(-100.f,signalPeakDb-3.f);
        float incomingPeak=0;
        bool transformed=false;
        auto transform=[&]{
            for(int channel=0;channel<2;++channel){scratch.fill(0);for(int n=0;n<size;++n)scratch[(size_t)n]=(channel?right:left)[(size_t)((cursor+n)%size)];window.multiplyWithWindowingTable(scratch.data(),size);fft.performFrequencyOnlyForwardTransform(scratch.data());for(int n=0;n<=size/2;++n){float magnitude=scratch[(size_t)n]*2.f/size;float level=juce::Decibels::gainToDecibels(magnitude,-100.f);db[(size_t)n]=juce::jmax(level,db[(size_t)n]);}}
            transformed=true;
        };
        const auto count=tap.consume([&](float l,float r){l=std::isfinite(l)?l:0;r=std::isfinite(r)?r:0;incomingPeak=juce::jmax(incomingPeak,juce::jmax(std::abs(l),std::abs(r)));left[(size_t)cursor]=l;right[(size_t)cursor]=r;cursor=(cursor+1)%size;filled=juce::jmin(size,filled+1);if(++hop==size/4){hop=0;transform();}});
        const float incomingDb=juce::Decibels::gainToDecibels(incomingPeak,-100.f);
        if(incomingDb>=signalPeakDb && incomingDb>-100){signalPeakDb=incomingDb;peakHold=15;} // One second at the EQ display's 15 Hz.
        // Give short hits a display before a complete 8192-sample window arrives.
        if(count>0 && !transformed)transform();
        return true;
    }
    // A narrow 1/12-octave peak envelope preserves narrow tones on a log axis.
    // Averaging dB values here drags high-frequency peaks towards neighbouring
    // noise-floor bins, because that same log interval spans more FFT bins.
    float levelAt(double hz,double rate) const {
        if(rate<=0 || hz<=0 || hz>=rate*.5)return -100;
        const auto binAt=[&](double f){return juce::jlimit(0.0,(double)size/2,f*size/rate);};
        const auto interpolate=[&](double bin){int lo=(int)bin,hi=juce::jmin(size/2,lo+1);return db[(size_t)lo]+(float)(bin-lo)*(db[(size_t)hi]-db[(size_t)lo]);};
        const double low=binAt(hz*std::pow(2.0,-1.0/24)),high=binAt(hz*std::pow(2.0,1.0/24));
        float peak=juce::jmax(interpolate(low),interpolate(high));
        for(int bin=(int)std::ceil(low);bin<=(int)std::floor(high);++bin)peak=juce::jmax(peak,db[(size_t)bin]);
        return peak;
    }
    float signalPeakDb=-100;
    std::array<float,size/2+1> db;
private:
    juce::dsp::FFT fft{13};juce::dsp::WindowingFunction<float> window{size,juce::dsp::WindowingFunction<float>::hann,true};
    std::array<float,size> left{},right{};std::array<float,size*2> scratch{};int cursor=0,filled=0,hop=0,peakHold=0;
};
}
