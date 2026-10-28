#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KitLibrary.h"
#include "SampleEngine.h"

class DrumAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    DrumAudioProcessor();
    ~DrumAudioProcessor() override;
    drum::KitLibrary& getLibrary() { return library; }
    drum::SampleEngine& getSampleEngine() { return sampleEngine; }
    juce::AudioParameterFloat& macro(int n) { return *macros[(size_t)n]; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DJL Drum"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return sampleEngine.tailSeconds(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
private:
    drum::KitLibrary library;
    std::array<juce::AudioParameterFloat*,drum::numMacros> macros {};
    drum::SampleEngine sampleEngine { library, [this] { triggerAsyncUpdate(); } };
    void handleAsyncUpdate() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAudioProcessor)
};
