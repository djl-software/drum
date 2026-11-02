#pragma once
#include "KitLibrary.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <optional>
#include "SpectrumAnalyzer.h"

namespace drum {
enum class BuiltinKind { eq, compressor, transient, saturation, clipper, limiter, gate, utility, stereoWidth };
juce::String builtinName(BuiltinKind);
juce::PluginDescription builtinDescription(BuiltinKind);
bool isBuiltin(const juce::PluginDescription&);
std::unique_ptr<juce::AudioProcessor> createBuiltin(const juce::PluginDescription&);
std::vector<EffectSlot> factoryChain(int preset=0);
std::vector<EffectSlot> defaultChain();
juce::File defaultChainFile();
bool saveChain(const juce::File&,const std::vector<EffectSlot>&);
std::optional<std::vector<EffectSlot>> loadChain(const juce::File&);

class BuiltinEffect final : public juce::AudioProcessor {
public:
    explicit BuiltinEffect(BuiltinKind);
    const juce::String getName() const override {return "Drum "+builtinName(kind);}
    bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return kind==BuiltinKind::stereoWidth?.03:0;}
    bool hasEditor() const override{return true;} juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override{return 1;} int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{} const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    bool isBusesLayoutSupported(const BusesLayout& l) const override{return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void prepareToPlay(double,int) override; void releaseResources() override{} void reset() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void getStateInformation(juce::MemoryBlock&) override; void setStateInformation(const void*,int) override;
    Equalizer equalizer() const; void setEqualizer(const Equalizer&);
    void resetParameters();
    const BuiltinKind kind;
    std::vector<juce::AudioParameterFloat*> controls;
    std::atomic<bool> monoCheck{false},stereoActive{false};
    std::atomic<float> correlation{1},monoChange{0};
    std::atomic<float> inputPeak{0},outputPeak{0},reduction{0};
    std::unique_ptr<SpectrumTap> spectrum, waveform;
private:
    void parameter(const juce::String&,float,float,float,const juce::String& unit={});
    juce::AudioBuffer<float> widthDelay;int widthWrite=0;float bassL=0,bassR=0;double energyL=0,energyR=0,crossEnergy=0;juce::SmoothedValue<float> monoBlend;
    VoiceEqualizer eq;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<juce::SmoothedValue<float>> smooth;
    bool gateOpen=false;
    double hz=44100;float envelope=0,fast=0,slow=0,gainEnvelope=1;
};
}
