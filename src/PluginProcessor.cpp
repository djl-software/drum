#include "PluginProcessor.h"
#include "PluginEditor.h"

DrumAudioProcessor::DrumAudioProcessor()
    : AudioProcessor([] {
        BusesProperties buses;
        buses=buses.withOutput("Output",juce::AudioChannelSet::stereo(),true);
        for(int n=2;n<=16;++n) buses=buses.withOutput("Out "+juce::String(n),juce::AudioChannelSet::stereo(),false);
        for(int n=0;n<6;++n) buses=buses.withOutput("Send "+juce::String::charToString((juce::juce_wchar)('A'+n)),juce::AudioChannelSet::stereo(),false);
        return buses;
    }())
{
    for(int n=0;n<drum::numMacros;++n)
    {
        auto* parameter=new juce::AudioParameterFloat(juce::ParameterID("macro"+juce::String(n+1),1),"Macro "+juce::String(n+1),0.0f,1.0f,0.5f);
        addParameter(parameter); macros[(size_t)n]=parameter;
    }
}

DrumAudioProcessor::~DrumAudioProcessor() { sampleEngine.shutdown(); cancelPendingUpdate(); }
void DrumAudioProcessor::handleAsyncUpdate()
{
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void DrumAudioProcessor::prepareToPlay(double rate, int)
{
    sampleEngine.prepare(rate);
    std::array<int,22> channels; channels.fill(-1); int offset=0;
    for(int n=0;n<getBusCount(false);++n)
    {
        auto count=getChannelCountOfBus(false,n);
        if(count>0) { channels[(size_t)n]=offset; offset+=count; }
    }
    sampleEngine.setOutputChannels(channels,getMainBusNumOutputChannels());
}
void DrumAudioProcessor::releaseResources() { sampleEngine.release(); }

bool DrumAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    if(layout.inputBuses.size()!=0 || layout.outputBuses.size()!=22) return false;
    for(int n=1;n<layout.outputBuses.size();++n)
        if(!layout.outputBuses[n].isDisabled() && layout.outputBuses[n]!=juce::AudioChannelSet::stereo()) return false;
    return (output == juce::AudioChannelSet::mono()
         || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet().isDisabled();
}

void DrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    std::array<float,drum::numMacros> values;
    for(int n=0;n<drum::numMacros;++n) values[(size_t)n]=macros[(size_t)n]->get();
    sampleEngine.setMacros(values);
    if(auto* playhead=getPlayHead()) if(auto position=playhead->getPosition())
        if(auto bpm=position->getBpm(); bpm && std::isfinite(*bpm)) sampleEngine.setTempo(*bpm);
    sampleEngine.process(buffer, midi);
    midi.clear();
}

juce::AudioProcessorEditor* DrumAudioProcessor::createEditor()
{
    return new DrumAudioProcessorEditor(*this);
}

void DrumAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    const auto state = library.toXml();
    for(int n=0;n<drum::numMacros;++n) state->setAttribute("macro"+juce::String(n+1),(double)macros[(size_t)n]->get());
    copyXmlToBinary(*state, data);
}

void DrumAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 4 * 1024 * 1024) return;
    if (const auto state = getXmlFromBinary(data, size))
    {
        std::array<float,drum::numMacros> values;
        for(int n=0;n<drum::numMacros;++n)
        {
            const auto key="macro"+juce::String(n+1);
            auto raw=state->getStringAttribute(key,"0.5");char* end=nullptr;auto v=std::strtod(raw.toRawUTF8(),&end);
            if(end==raw.toRawUTF8() || *end!='\0' || !std::isfinite(v) || v<0 || v>1) return;
            values[(size_t)n]=(float)v;
        }
        if(library.restore(*state)) for(int n=0;n<drum::numMacros;++n)
        {
            // Restoring state is not a user gesture. The wrapper synchronises
            // its controller afterwards; do not mark the host project edited.
            juce::AudioProcessorParameter* parameter=macros[(size_t)n];
            parameter->setValue(values[(size_t)n]);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumAudioProcessor(); }
