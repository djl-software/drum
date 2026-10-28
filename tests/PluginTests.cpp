#include "PluginProcessor.h"
#include <iostream>
#include <memory>
#include <stdexcept>

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void runManagementTests();
void runPlaybackTests();
void runControlTests();
void runEditingTests();
void runWaveformTests();
void runPreviewTests();
void runChannelAttackTests();
void runVst3PlaybackTest(const juce::String&);

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    try
    {
        if(argc==3 && juce::String(argv[1])=="--vst3") { runVst3PlaybackTest(argv[2]); return 0; }
        DrumAudioProcessor processor;
        require(processor.acceptsMidi(), "Instrument must accept MIDI");
        require(!processor.producesMidi(), "Instrument must not expose MIDI output");
        require(!processor.isMidiEffect(), "Instrument classified as a MIDI effect");
        require(processor.getTotalNumInputChannels() == 0, "Instrument has an audio input");
        for (int channels : { 1, 2 })
        {
            auto layout = processor.getBusesLayout();
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
                    juce::MidiBuffer midi;
                    if (samples > 0)
                        midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8) 100), 0);
                    processor.processBlock(buffer, midi);
                    for (int c = 0; c < channels; ++c)
                        for (int s = 0; s < samples; ++s)
                            require(buffer.getSample(c, s) == 0.0f, "Empty instrument must render silence");
                    require(midi.isEmpty(), "Input MIDI was passed through");
                }
                processor.releaseResources();
            }
        }
        auto invalid = processor.getBusesLayout();
        invalid.inputBuses.add(juce::AudioChannelSet::stereo());
        require(!processor.isBusesLayoutSupported(invalid), "Audio input accepted");
        invalid = processor.getBusesLayout();
        invalid.outputBuses.set(0, juce::AudioChannelSet::disabled());
        require(!processor.isBusesLayoutSupported(invalid), "Disabled output accepted");
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
        runManagementTests();
        runPlaybackTests();
        runControlTests();
        runEditingTests();
        runWaveformTests();
        runPreviewTests();
        runChannelAttackTests();
        std::cout << "Drum processor tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
