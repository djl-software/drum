#include "PluginProcessor.h"
#include <iostream>
#include <memory>
#include <stdexcept>

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    try
    {
        DrumAudioProcessor processor;
        for (int channels : { 1, 2 })
        {
            auto layout = processor.getBusesLayout();
            layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
            layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
            require(processor.setBusesLayout(layout), "Mono/stereo layout rejected");
            for (double rate : { 44100.0, 48000.0, 96000.0 })
            {
                processor.setRateAndBufferSizeDetails(rate, 512);
                processor.prepareToPlay(rate, 512);
                for (int samples : { 0, 1, 127, 512 })
                {
                    juce::AudioBuffer<float> buffer(channels, samples);
                    for (int c = 0; c < channels; ++c)
                        for (int s = 0; s < samples; ++s)
                            buffer.setSample(c, s, float((s % 31) - 15) / 32.0f);
                    juce::AudioBuffer<float> original;
                    original.makeCopyOf(buffer);
                    juce::MidiBuffer midi;
                    processor.processBlock(buffer, midi);
                    for (int c = 0; c < channels; ++c)
                        for (int s = 0; s < samples; ++s)
                            require(buffer.getSample(c, s) == original.getSample(c, s), "Audio changed");
                }
                processor.releaseResources();
            }
        }
        auto invalid = processor.getBusesLayout();
        invalid.outputBuses.set(0, juce::AudioChannelSet::mono());
        require(!processor.isBusesLayoutSupported(invalid), "Mismatched channels accepted");
        invalid.inputBuses.set(0, juce::AudioChannelSet::create5point1());
        invalid.outputBuses.set(0, juce::AudioChannelSet::create5point1());
        require(!processor.isBusesLayoutSupported(invalid), "Surround accepted");
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        require(state.getSize() > 0, "Empty state");
        processor.setStateInformation(state.getData(), int(state.getSize()));
        juce::MemoryBlock restored;
        processor.getStateInformation(restored);
        require(state == restored, "State round trip failed");
        processor.setStateInformation("invalid", 7);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        require(editor && editor->getWidth() > 0 && editor->getHeight() > 0, "Editor creation failed");
        std::cout << "Drum processor tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
