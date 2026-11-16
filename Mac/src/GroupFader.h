#pragma once

namespace drum
{
// A final gain on each owned output branch. Separate smoothing histories keep
// each branch aligned, including retained wet tails and routes to other groups.
class GroupFader final : public juce::AudioProcessor
{
public:
    explicit GroupFader(std::array<std::atomic<float>,2>& gains):AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),targets(gains){}
    void prepareToPlay(double rate,int) override{initial=true;for(int c=0;c<2;++c){levels[(size_t)c].reset(rate,.02);levels[(size_t)c].setCurrentAndTargetValue(targets[(size_t)c].load());}}
    void releaseResources() override{}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override{for(int c=0;c<2;++c){auto& level=levels[(size_t)c];const auto target=targets[(size_t)c].load();if(initial)level.setCurrentAndTargetValue(target);else level.setTargetValue(target);auto* data=audio.getWritePointer(c);for(int f=0;f<audio.getNumSamples();++f)data[f]*=level.getNextValue();}initial=false;}
    const juce::String getName() const override{return "Group output fader";}
    bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}double getTailLengthSeconds() const override{return 0;}
    bool hasEditor() const override{return false;}juce::AudioProcessorEditor* createEditor() override{return nullptr;}
    int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}void setCurrentProgram(int) override{}const juce::String getProgramName(int) override{return {};}void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock&) override{}void setStateInformation(const void*,int) override{}
private:
    std::array<std::atomic<float>,2>& targets;
    std::array<juce::SmoothedValue<float>,2> levels;
    bool initial=true;
};
}
