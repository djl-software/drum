#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <array>
namespace drum
{
struct TonalAnalysis
{
    double midi=-1,lowMidi=-1,highMidi=-1,confidence=0;
    int key=-1; // C..B major (0..11), then C..B minor (12..23).
    bool varying=false,complete=false;
    std::array<float,12> noteEnergy{}; // Relative spectral power, C..B, folded across octaves.
};
TonalAnalysis analyseTonal(const juce::AudioBuffer<float>&,double rate,const std::function<bool()>& cancelled={});
juce::String tileKey(const TonalAnalysis&,double transpose=0);
int strongestNote(const TonalAnalysis&,double transpose=0);
juce::String spectrumKey(const TonalAnalysis&,double transpose=0);
juce::String musicalNote(double midi);
juce::String musicalKey(int key);
juce::String camelotCode(int key);
}
