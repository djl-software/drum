#include <juce_audio_processors/juce_audio_processors.h>

// Deterministic external VST3 fixture: nonlinear chain ordering, latency and tails
// are checked through the same nested-plugin path used for third-party effects.
class ChainTestEffect final : public juce::AudioProcessor
{
public:
    ChainTestEffect():AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true))
    {
        addParameter(gain=new juce::AudioParameterFloat("gain","Gain",0.0f,2.0f,0.5f));
        addParameter(bias=new juce::AudioParameterFloat("bias","Bias",-0.5f,0.5f,0.0f));
        addParameter(delay=new juce::AudioParameterInt("delay","Delay",0,64,0));
        addParameter(feedback=new juce::AudioParameterFloat("feedback","Feedback",0.0f,0.9f,0.0f));
    }
    void prepareToPlay(double,int) override {reset();setLatencySamples(delay->get());}
    void releaseResources() override {}
    void reset() override {history={};cursor=0;}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {return layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo() && layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override
    {
        const int d=delay->get();if(d!=getLatencySamples())setLatencySamples(d);
        for(int frame=0;frame<audio.getNumSamples();++frame)
        {
            for(int channel=0;channel<2;++channel)
            {
                const float input=audio.getSample(channel,frame)*gain->get()+bias->get();
                const float previous=d?history[(size_t)channel][(size_t)((cursor+128-d)%128)]:0;
                history[(size_t)channel][(size_t)cursor]=input+previous*feedback->get();audio.setSample(channel,frame,d?previous:input);
            }
            cursor=(cursor+1)%128;
        }
    }
    const juce::String getName() const override {return "Drum Chain Test Effect";}
    bool acceptsMidi() const override {return false;}bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return delay->get()>0?1.0:0;}
    bool hasEditor() const override {return true;}juce::AudioProcessorEditor* createEditor() override {return new juce::GenericAudioProcessorEditor(*this);}
    int getNumPrograms() override {return 1;}int getCurrentProgram() override {return 0;}void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& saved) override {juce::XmlElement xml("TestEffect");for(int i=0;i<getParameters().size();++i)xml.setAttribute("p"+juce::String(i),getParameters()[i]->getValue());copyXmlToBinary(xml,saved);}
    void setStateInformation(const void* data,int size) override {if(auto xml=getXmlFromBinary(data,size))for(int i=0;i<getParameters().size();++i)getParameters()[i]->setValueNotifyingHost((float)xml->getDoubleAttribute("p"+juce::String(i),getParameters()[i]->getValue()));}
private:
    juce::AudioParameterFloat *gain=nullptr,*bias=nullptr,*feedback=nullptr;juce::AudioParameterInt* delay=nullptr;
    std::array<std::array<float,128>,2> history{};int cursor=0;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChainTestEffect();}
