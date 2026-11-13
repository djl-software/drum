#include "TonalAnalysis.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace drum
{
namespace
{
const char* names[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
double correlate(const float* data,int length,int lag)
{
    double dot=0,a=0,b=0;
    for(int n=0;n<length;++n){const double x=data[n],y=data[n+lag];dot+=x*y;a+=x*x;b+=y*y;}
    return a*b>1e-18?dot/std::sqrt(a*b):0;
}
}
juce::String musicalNote(double midi)
{
    if(!std::isfinite(midi) || midi<0 || midi>127)return "Uncertain";
    const int n=(int)std::round(midi);return juce::String(names[n%12])+juce::String(n/12-1);
}
juce::String musicalKey(int key) {return key<0 || key>=24?"Uncertain":juce::String(names[key%12])+(key>=12?" minor":" major");}
juce::String camelotCode(int key)
{
    constexpr int major[]{8,3,10,5,12,7,2,9,4,11,6,1},minor[]{5,12,7,2,9,4,11,6,1,8,3,10};
    return key<0 || key>=24?"--":juce::String(key>=12?minor[key%12]:major[key%12])+(key>=12?"A":"B");
}
int strongestNote(const TonalAnalysis& a,double transpose)
{
    std::array<double,12> power{};const double shift=std::fmod(transpose,12.0);const int lower=(int)std::floor(shift);const double fraction=shift-lower;
    for(int n=0;n<12;++n){power[(size_t)((n+lower+24)%12)]+=a.noteEnergy[(size_t)n]*(1-fraction);power[(size_t)((n+lower+25)%12)]+=a.noteEnergy[(size_t)n]*fraction;}
    const auto peak=std::max_element(power.begin(),power.end());return *peak>0?(int)std::distance(power.begin(),peak):-1;
}
juce::String spectrumKey(const TonalAnalysis& a,double transpose)
{
    constexpr const char* labels[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const int note=strongestNote(a,transpose);return note>=0?juce::String(labels[note]):juce::String("No signal");
}
juce::String tileKey(const TonalAnalysis& a,double transpose)
{
    if(!a.complete)return "Analysing...";
    const int note=strongestNote(a,transpose);
    return note>=0?spectrumKey(a,transpose)+"  "+camelotCode(note)+"/"+camelotCode(note+12):"No signal";
}
TonalAnalysis analyseTonal(const juce::AudioBuffer<float>& audio,double rate,const std::function<bool()>& cancelled)
{
    TonalAnalysis result;
    if(audio.getNumChannels()<1 || audio.getNumSamples()<32 || !std::isfinite(rate) || rate<8000){result.complete=true;return result;}
    const int sourceFrames=std::min(audio.getNumSamples(),(int)(rate*30));
    int channel=0;if(audio.getNumChannels()>1 && audio.getRMSLevel(1,0,sourceFrames)>audio.getRMSLevel(0,0,sourceFrames))channel=1;
    const double analysisRate=std::min(12000.0,rate),ratio=analysisRate/rate;
    std::vector<float> data,secondary;data.reserve((size_t)(sourceFrames*ratio)+1);if(audio.getNumChannels()>1)secondary.reserve((size_t)(sourceFrames*ratio)+1);
    const double alpha=1-std::exp(-juce::MathConstants<double>::twoPi*2500/rate);
    std::array<double,4> filter{},secondaryFilter{};double phase=0,dc=0,secondaryDc=0,energy=0;
    const auto* source=audio.getReadPointer(channel);const auto* other=audio.getNumChannels()>1?audio.getReadPointer(1-channel):nullptr;
    for(int n=0;n<sourceFrames;++n)
    {
        if((n&4095)==0 && cancelled && cancelled())return {};
        double value=source[n];for(auto& stage:filter){stage+=alpha*(value-stage);value=stage;}
        double otherValue=other?other[n]:0;if(other)for(auto& stage:secondaryFilter){stage+=alpha*(otherValue-stage);otherValue=stage;}
        phase+=ratio;if(phase>=1){phase-=1;dc+=0.001*(value-dc);value-=dc;data.push_back((float)value);energy+=value*value;if(other){secondaryDc+=.001*(otherValue-secondaryDc);secondary.push_back((float)(otherValue-secondaryDc));}}
    }
    result.complete=true;if(data.size()<96 || energy/data.size()<1e-10)return result;
    // Exclude silent padding before choosing analysis windows. Otherwise a short
    // hit in a long file gets almost no pitch windows, despite audible tone.
    const int block=120;double peakBlock=0;std::vector<double> levels;
    for(size_t start=0;start<data.size();start+=block){double sum=0;for(size_t n=start;n<std::min(data.size(),start+block);++n)sum+=data[n]*data[n]+(secondary.empty()?0:secondary[n]*secondary[n]);levels.push_back(sum);peakBlock=std::max(peakBlock,sum);}
    int firstActive=0,lastActive=(int)levels.size()-1;
    while(firstActive<lastActive && levels[(size_t)firstActive]<peakBlock*.0001)++firstActive;
    while(lastActive>firstActive && levels[(size_t)lastActive]<peakBlock*.0001)--lastActive;
    const int begin=firstActive*block,end=std::min((int)data.size(),(lastActive+1)*block);
    if(end-begin>=96){data=std::vector<float>(data.begin()+begin,data.begin()+end);if(!secondary.empty())secondary=std::vector<float>(secondary.begin()+begin,secondary.begin()+end);}
    const int window=std::min(1024,(int)data.size()),maxLag=std::min((int)(analysisRate/25),window/2),length=window-maxLag;
    std::vector<double> pitches,confidences;
    for(int frame=0;frame<32;++frame)
    {
        if(cancelled && cancelled())return {};
        const int offset=(int)((data.size()-window)*frame/31);const float* p=data.data()+offset;
        double localEnergy=0;for(int n=0;n<window;++n)localEnergy+=p[n]*p[n];
        if(localEnergy/window<energy/data.size()*0.025)continue;
        double prev2=correlate(p,length,1),prev=correlate(p,length,2);
        for(int lag=3;lag<=maxLag;++lag)
        {
            const double next=correlate(p,length,lag);
            if(lag-1>=analysisRate/2000 && prev>0.85 && prev>prev2 && prev>=next)
            {
                const double denominator=prev2-2*prev+next;
                const double fine=std::abs(denominator)>1e-10?0.5*(prev2-next)/denominator:0;
                const double hz=analysisRate/(lag-1+std::clamp(fine,-0.5,0.5));
                pitches.push_back(69+12*std::log2(hz/440));confidences.push_back(prev);break;
            }
            prev2=prev;prev=next;
        }
    }
    if(!pitches.empty())
    {
        std::sort(pitches.begin(),pitches.end());result.midi=pitches[pitches.size()/2];
        result.lowMidi=pitches[pitches.size()/10];result.highMidi=pitches[pitches.size()-1-pitches.size()/10];
        result.varying=result.highMidi-result.lowMidi>1.0;
        for(double c:confidences)result.confidence+=c;result.confidence/=confidences.size();
    }
    constexpr int size=4096;juce::dsp::FFT fft(12);std::vector<float> spectrum(size*2);std::array<double,12> chroma{};
    std::array<double,12> notePower{};
    const int first=std::max(2,(int)(25*size/analysisRate)),last=std::min(size/2-1,(int)(3000*size/analysisRate));
    auto accumulateNotes=[&](double peak){
        if(peak>=1e-4)for(int k=first;k<=last;++k){
            const double v=spectrum[(size_t)k];if(v<peak*.02 || v<=spectrum[(size_t)k-1] || v<spectrum[(size_t)k+1])continue;
            const double left=spectrum[(size_t)k-1],right=spectrum[(size_t)k+1],denominator=left-2*v+right;
            const double fine=std::abs(denominator)>1e-12?std::clamp(.5*(left-right)/denominator,-.5,.5):0;
            const double pitch=69+12*std::log2((k+fine)*analysisRate/size/440);const int lower=(int)std::floor(pitch);const double fraction=pitch-lower;
            notePower[(size_t)((lower%12+12)%12)]+=v*v*(1-fraction);notePower[(size_t)(((lower+1)%12+12)%12)]+=v*v*fraction;
        }
    };
    int tonalFrames=0,fundamentalFrames=0;
    for(int frame=0;frame<16;++frame)
    {
        if(cancelled && cancelled())return {};
        std::fill(spectrum.begin(),spectrum.end(),0);const int available=std::min(size,(int)data.size());
        const int offset=(int)((data.size()-available)*frame/15);
        for(int n=0;n<available;++n)spectrum[(size_t)n]=data[(size_t)offset+n]*(float)(0.5-0.5*std::cos(juce::MathConstants<double>::twoPi*n/std::max(1,available-1)));
        fft.performFrequencyOnlyForwardTransform(spectrum.data());
        double arithmetic=0,logs=0,peak=0;for(int k=first;k<=last;++k){const double v=std::max(1e-12,(double)spectrum[(size_t)k]);arithmetic+=v;logs+=std::log(v);peak=std::max(peak,v);}
        const int bins=last-first+1;const double flatness=std::exp(logs/bins)/(arithmetic/bins);
        accumulateNotes(peak);
        if(peak<1e-4 || flatness>0.25)continue;++tonalFrames;
        // Do not turn a pitched drum's harmonics into a major/minor label.
        // Chords may correlate at a subharmonic which has no actual spectral peak.
        if(pitches.size()>=6 && result.highMidi-result.lowMidi<0.6)
        {
            const int fundamental=(int)std::round(440*std::pow(2,(result.midi-69)/12)*size/analysisRate);
            if(fundamental>0 && fundamental<size/2-1 && std::max({spectrum[(size_t)fundamental-1],spectrum[(size_t)fundamental],spectrum[(size_t)fundamental+1]})>peak*0.25)++fundamentalFrames;
        }
        for(int k=first;k<=last;++k)
        {
            const double v=spectrum[(size_t)k];if(v<peak*0.12 || v<=spectrum[(size_t)k-1] || v<spectrum[(size_t)k+1])continue;
            const double left=spectrum[(size_t)k-1],right=spectrum[(size_t)k+1],denominator=left-2*v+right;
            const double fine=std::abs(denominator)>1e-12?0.5*(left-right)/denominator:0;
            const double note=69+12*std::log2((k+std::clamp(fine,-0.5,0.5))*analysisRate/size/440);
            const int rounded=(int)std::round(note);if(rounded<0 || std::abs(note-rounded)>0.3)continue;
            chroma[(size_t)(rounded%12)]+=std::sqrt(v/peak);
        }
    }
    // Sum channel power, never waveforms: inverted stereo must not cancel notes.
    if(!secondary.empty())for(int frame=0;frame<16;++frame){
        if(cancelled && cancelled())return {};
        std::fill(spectrum.begin(),spectrum.end(),0);const int available=std::min(size,(int)secondary.size());const int offset=(int)((secondary.size()-available)*frame/15);
        for(int n=0;n<available;++n)spectrum[(size_t)n]=secondary[(size_t)offset+n]*(float)(.5-.5*std::cos(juce::MathConstants<double>::twoPi*n/std::max(1,available-1)));
        fft.performFrequencyOnlyForwardTransform(spectrum.data());double peak=0;for(int k=first;k<=last;++k)peak=std::max(peak,(double)spectrum[(size_t)k]);accumulateNotes(peak);
    }
    const double strongest=*std::max_element(notePower.begin(),notePower.end());if(strongest>0)for(size_t n=0;n<12;++n)result.noteEnergy[n]=(float)(notePower[n]/strongest);
    if(data.size()<analysisRate*0.12 || tonalFrames<4 || fundamentalFrames>tonalFrames/2)return result;
    double max=*std::max_element(chroma.begin(),chroma.end());int present=0;for(double v:chroma)if(v>max*0.18)++present;
    if(present<3 || present>9)return result;
    constexpr double major[]{1,0,0.2,0,0.8,0.2,0,0.65,0,0.2,0,0.2};
    constexpr double minor[]{1,0,0.2,0.8,0,0.2,0,0.65,0.2,0,0.2,0};
    double best=-2,second=-2;int key=-1;double mean=0;for(double v:chroma)mean+=v/12;
    for(int candidate=0;candidate<24;++candidate)
    {
        const auto* profile=candidate<12?major:minor;double pm=0;for(int n=0;n<12;++n)pm+=profile[n]/12;
        double dot=0,aa=0,bb=0;
        for(int n=0;n<12;++n){const double a=chroma[(size_t)n]-mean,b=profile[(n-candidate%12+12)%12]-pm;dot+=a*b;aa+=a*a;bb+=b*b;}
        const double score=aa*bb>0?dot/std::sqrt(aa*bb):-1;
        if(score>best){second=best;best=score;key=candidate;}else second=std::max(second,score);
    }
    if(best>0.72 && best-second>0.12)result.key=key;
    return result;
}
}
