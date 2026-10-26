#include "PluginProcessor.h"
#include "PluginEditor.h"

DrumAudioProcessor::DrumAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

DrumAudioProcessor::~DrumAudioProcessor() { sampleEngine.shutdown(); cancelPendingUpdate(); }
void DrumAudioProcessor::handleAsyncUpdate()
{
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void DrumAudioProcessor::prepareToPlay(double rate, int) { sampleEngine.prepare(rate); }
void DrumAudioProcessor::releaseResources() { sampleEngine.release(); }

bool DrumAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono()
         || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet().isDisabled();
}

void DrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
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
    copyXmlToBinary(*state, data);
}

void DrumAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 4 * 1024 * 1024) return;
    if (const auto state = getXmlFromBinary(data, size)) library.restore(*state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumAudioProcessor(); }
