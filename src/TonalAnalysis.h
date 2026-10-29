#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
namespace drum
{
struct TonalAnalysis
{
    double midi=-1,lowMidi=-1,highMidi=-1,confidence=0;
    int key=-1; // C..B major (0..11), then C..B minor (12..23).
    bool varying=false,complete=false;
};
TonalAnalysis analyseTonal(const juce::AudioBuffer<float>&,double rate,const std::function<bool()>& cancelled={});
juce::String musicalNote(double midi);
juce::String musicalKey(int key);
juce::String camelotCode(int key);
}
