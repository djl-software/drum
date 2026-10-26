#include "PluginProcessor.h"
#include "PluginEditor.h"

DrumAudioProcessor::DrumAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void DrumAudioProcessor::prepareToPlay(double, int) {}
void DrumAudioProcessor::releaseResources() {}

bool DrumAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono()
         || output == juce::AudioChannelSet::stereo())
        && output == layout.getMainInputChannelSet();
}

void DrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    // Transparent starting point: add Drum DSP here without allocating on the audio thread.
    for (int channel = getTotalNumInputChannels(); channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    midi.clear();
}

juce::AudioProcessorEditor* DrumAudioProcessor::createEditor()
{
    return new DrumAudioProcessorEditor(*this);
}

void DrumAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    juce::XmlElement state("DrumState");
    state.setAttribute("schemaVersion", 1);
    copyXmlToBinary(state, data);
}

void DrumAudioProcessor::setStateInformation(const void* data, int size)
{
    // No parameters yet. Validate the envelope before adding future state restoration.
    const auto state = getXmlFromBinary(data, size);
    if (state == nullptr || !state->hasTagName("DrumState"))
        return;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumAudioProcessor(); }
