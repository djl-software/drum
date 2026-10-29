#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void near(float value,float expected,const char* message)
{
    if(std::abs(value-expected)>=0.00005f) throw std::runtime_error(juce::String(message).toStdString()+": actual "+std::to_string(value)+", expected "+std::to_string(expected));
}
juce::File fixture(const char* name,int channels=1,double rate=44100,int frames=4410)
{
    auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data"); folder.createDirectory();
    auto file=folder.getChildFile(name); file.deleteFile(); auto stream=file.createOutputStream();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),rate,(unsigned int)channels,24,{},0));
    check(writer!=nullptr,"Fixture writer failed");
    juce::AudioBuffer<float> data(channels,frames);
    for(int c=0;c<channels;++c) for(int n=0;n<frames;++n) data.setSample(c,n,c==0?0.25f:0.5f);
    check(writer->writeFromAudioSampleBuffer(data,0,frames),"Fixture write failed");
    return file;
}
struct Setup
{
    DrumAudioProcessor processor;
    juce::String kit,group,instrument;
    Setup()
    {
        auto s=processor.getLibrary().snapshot(); kit=s.activeKit; group=drum::activeKit(s)->groups.front().id;
        auto added=processor.getLibrary().addInstrument(kit,group); check(added.ok,"Instrument setup failed"); instrument=added.id;
        check(processor.getLibrary().assignNote(kit,instrument,36).ok,"Note setup failed");
        processor.setRateAndBufferSizeDetails(48000,128); processor.prepareToPlay(48000,128);
    }
    void load(const juce::File& file)
    {
        processor.getSampleEngine().requestLoad(kit,instrument,file);
        check(processor.getSampleEngine().waitForLoading(10000),"Sample loader timed out");
    }
};
void writePreview(juce::Component& editor)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("drum-playback.png"); file.deleteFile();
    juce::FileOutputStream stream(file);
    check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),stream),"Playback preview failed");
}
}
void runPlaybackTests()
{
    auto mono=fixture("mono.wav"),stereo=fixture("stereo.wav",2);
    Setup setup; setup.load(mono);
    auto& p=setup.processor; check(p.getSampleEngine().statusFor(setup.instrument).ready,"Valid sample did not load");
    juce::AudioBuffer<float> output(2,128); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)64),23);
    midi.addEvent(juce::MidiMessage::noteOff(1,36),30);
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)64),40);
    midi.addEvent(juce::MidiMessage::allSoundOff(1),80);
    p.processBlock(output,midi);
    near(output.getSample(0,22),0,"Note started early");
    near(output.getSample(0,23),0.25f*64/127,"Velocity or MIDI offset incorrect");
    near(output.getSample(1,35),0.25f*64/127,"One-shot stopped on note-off or mono spread failed");
    near(output.getSample(0,40),0.5f*64/127,"Repeated hits did not overlap");
    near(output.getSample(0,80),0,"MIDI panic ignored");
    // Zero-velocity note-on does not retrigger.
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)0),0); p.processBlock(output,midi);
    near(output.getMagnitude(0,128),0,"Zero velocity triggered a hit");
    // The 0.1-second file must play for 4800 host samples at 48 kHz.
    p.prepareToPlay(48000,128); output.setSize(2,5000);
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    check(output.getSample(0,4790)>0 && output.getSample(0,4801)==0,"Sample-rate conversion changed duration");
    p.prepareToPlay(44100,128); output.setSize(2,4500);
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    check(output.getSample(0,4400)>0 && output.getSample(0,4411)==0,"Native sample rate duration incorrect");
    setup.load(stereo); p.prepareToPlay(48000,128); output.setSize(2,128);
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    near(output.getSample(0,1),0.25f,"Stereo left incorrect"); near(output.getSample(1,1),0.5f,"Stereo right incorrect");
    p.getSampleEngine().stop(); p.processBlock(output,midi); near(output.getMagnitude(0,128),0,"UI stop failed");
    p.getSampleEngine().audition(36); p.processBlock(output,midi); near(output.getSample(0,0),0.2f,"Audition failed");
    auto layout=p.getBusesLayout(); layout.outputBuses.set(0,juce::AudioChannelSet::mono()); check(p.setBusesLayout(layout),"Mono layout failed");
    p.prepareToPlay(48000,128); output.setSize(1,128); midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    near(output.getSample(0,0),0.375f,"Stereo-to-mono mix incorrect");
    auto previous=p.getLibrary().toXml()->toString();
    setup.load(mono.getSiblingFile("does-not-exist.wav"));
    check(p.getLibrary().toXml()->toString()==previous,"Failed replacement changed sample reference");
    check(!p.getSampleEngine().statusFor(setup.instrument).ready,"Missing file reported as loaded");
    p.prepareToPlay(48000,128); midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    near(output.getSample(0,0),0.375f,"Failed replacement lost previous audio");
    auto invalid=mono.getSiblingFile("not-audio.wav"); invalid.replaceWithText("invalid audio"); setup.load(invalid);
    check(p.getLibrary().toXml()->toString()==previous,"Corrupt replacement changed assignment");
    auto longSample=fixture("too-long.wav",1,8000,248000); setup.load(longSample);
    check(p.getLibrary().toXml()->toString()==previous,"Long-file rejection changed assignment");
    juce::MemoryBlock state; p.getStateInformation(state);
    DrumAudioProcessor recalled; recalled.setStateInformation(state.getData(),(int)state.getSize());
    check(recalled.getSampleEngine().waitForLoading(10000),"Recall loader timed out");
    recalled.prepareToPlay(48000,128); output.setSize(2,128); midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); recalled.processBlock(output,midi);
    near(output.getSample(1,0),0.5f,"Saved sample did not play after recall");
    auto newKit=p.getLibrary().addKit(); check(newKit.ok,"New kit failed");
    midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0); p.processBlock(output,midi);
    near(output.getMagnitude(0,128),0,"Old kit triggered while new kit was loading");
    check(p.getSampleEngine().waitForLoading(10000),"Kit switch timed out");
    p.processBlock(output,midi); near(output.getMagnitude(0,128),0,"Old voices survived kit switch");
    p.getLibrary().activateKit(setup.kit); check(p.getSampleEngine().waitForLoading(10000),"Return kit timed out");
    check(p.getLibrary().assignSample(setup.kit,setup.instrument,mono.getSiblingFile("missing.wav").getFullPathName()).ok,"Missing path setup failed");
    check(p.getSampleEngine().waitForLoading(10000),"Missing sample loader timed out");
    p.processBlock(output,midi); near(output.getMagnitude(0,128),0,"Missing sample played stale data");
    check(p.getSampleEngine().statusFor(setup.instrument).message.containsIgnoreCase("missing"),"Missing sample not reported");
    setup.load(stereo); check(p.getSampleEngine().statusFor(setup.instrument).ready,"Relink failed");
    layout.outputBuses.set(0,juce::AudioChannelSet::stereo()); check(p.setBusesLayout(layout),"Stereo layout restore failed");
    p.prepareToPlay(48000,128); output.setSize(2,128);
    for(int n=0;n<256;++n) midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0);
    p.processBlock(output,midi); near(output.getSample(0,0),32.0f,"Voice pool not bounded at 128");
    p.getSampleEngine().stop(); p.processBlock(output,midi);
    p.getLibrary().rename(setup.instrument,"Stereo test hit");
    check(p.getSampleEngine().waitForLoading(10000),"Rename sync failed");
    DrumAudioProcessorEditor editor(p); writePreview(editor);
    std::cout << "Sample loading, MIDI playback, resampling and recall tests passed\n";
}
void runVst3PlaybackTest(const juce::String& path)
{
    Setup setup; setup.load(fixture("host.wav",2));
    check(setup.processor.getLibrary().setControls(setup.kit,setup.instrument,{-6.020599913,1.0,12.0,false,true,2}).ok,"Hosted control setup failed");
    juce::MemoryBlock state; setup.processor.getStateInformation(state);
    juce::VST3PluginFormat format; juce::OwnedArray<juce::PluginDescription> types; format.findAllTypesForFile(types,juce::File(path).getFullPathName());
    check(types.size()==1 && types[0]->isInstrument,"VST3 scan failed");
    juce::String error; auto plugin=format.createInstanceFromDescription(*types[0],48000,128,error);
    if(!plugin) throw std::runtime_error(error.toStdString());
    // JUCE's VST3 host serialises component state inside its own envelope.
    juce::XmlElement envelope("VST3PluginState");
    envelope.createNewChildElement("IComponent")->addTextElement(state.toBase64Encoding());
    juce::MemoryBlock hostState; juce::AudioProcessor::copyXmlToBinary(envelope,hostState);
    plugin->setStateInformation(hostState.getData(),(int)hostState.getSize()); plugin->prepareToPlay(48000,128);
    juce::AudioBuffer<float> output(2,128); juce::MidiBuffer midi;
    const auto verifyPlayback=[&](juce::AudioPluginInstance& instance)
    {
        bool played=false;
        for(int attempt=0;attempt<500;++attempt)
        {
            midi.clear();
            midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
            midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),31);
            instance.processBlock(output,midi);
            if(output.getSample(1,31)>0.2f) { played=true; break; }
            juce::Thread::sleep(10);
        }
        check(played,"Compiled VST3 did not render a loaded sample");
        near(output.getSample(0,30),0,"VST3 started before MIDI event");
        near(output.getSample(0,31),0,"VST3 pan control incorrect");
        near(output.getSample(1,31),0.25f,"VST3 volume or sample playback incorrect");
    };
    verifyPlayback(*plugin);
    juce::MemoryBlock recalled; plugin->getStateInformation(recalled);
    check(recalled.getSize()>0,"VST3 did not save state");
    plugin->releaseResources();
    plugin.reset();
    plugin=format.createInstanceFromDescription(*types[0],48000,128,error);
    if(!plugin) throw std::runtime_error(error.toStdString());
    plugin->setStateInformation(recalled.getData(),(int)recalled.getSize());
    plugin->prepareToPlay(48000,128); verifyPlayback(*plugin); plugin->releaseResources();
    // Exercise new editing and routing through the actual compiled VST3 wrapper.
    drum::InstrumentControls editing;
    editing.sound.mode=1;editing.sound.fadeOut=1;editing.sound.start=0.25;editing.sound.end=0.75;
    editing.sound.reverse=1;editing.sound.output=1;editing.sound.sendA=-6.020599913;
    editing.macros[0]={1,-6.020599913,0};
    check(setup.processor.getLibrary().setControls(setup.kit,setup.instrument,editing).ok,"Hosted editing setup failed");
    setup.processor.macro(0).setValueNotifyingHost(0);setup.processor.getStateInformation(state);
    envelope.getChildByName("IComponent")->deleteAllTextElements();envelope.getChildByName("IComponent")->addTextElement(state.toBase64Encoding());
    juce::AudioProcessor::copyXmlToBinary(envelope,hostState);plugin->setStateInformation(hostState.getData(),(int)hostState.getSize());
    auto routing=plugin->getBusesLayout();check(routing.outputBuses.size()==22,"Compiled VST3 missing output buses");
    routing.outputBuses.set(1,juce::AudioChannelSet::stereo());routing.outputBuses.set(16,juce::AudioChannelSet::stereo());
    check(plugin->setBusesLayout(routing),"Compiled VST3 rejected output routing");plugin->prepareToPlay(48000,256);output.setSize(6,256);
    bool editedPlayed=false;
    for(int attempt=0;attempt<500;++attempt)
    {
        midi.clear();midi.addEvent(juce::MidiMessage::allSoundOff(1),0);midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0);midi.addEvent(juce::MidiMessage::noteOff(1,36),64);
        plugin->processBlock(output,midi);if(output.getSample(2,0)>0.1f){editedPlayed=true;break;}juce::Thread::sleep(10);
    }
    check(editedPlayed,"Compiled VST3 edited sound silent");near(output.getSample(0,0),0,"Compiled VST3 routing leaked to Main");
    near(output.getSample(2,0),0.125f,"Compiled VST3 macro not applied");near(output.getSample(4,0),0.0625f,"Compiled VST3 send incorrect");near(output.getSample(2,200),0,"Compiled VST3 gate note-off ignored");
    juce::AudioProcessorParameter* firstMacro=nullptr;
    for(int n=1;n<=drum::numMacros;++n)
    {
        juce::AudioProcessorParameter* found=nullptr;
        for(auto* parameter:plugin->getParameters()) if(parameter->getName(64)=="Macro "+juce::String(n)) found=parameter;
        check(found!=nullptr,"Compiled VST3 missing macro automation parameter");if(n==1)firstMacro=found;
    }
    firstMacro->setValueNotifyingHost(1);midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0);plugin->processBlock(output,midi);
    near(output.getSample(2,0),0.25f,"Compiled VST3 did not respond to host macro automation");plugin->releaseResources();
    editing={};editing.sound.mono=1;editing.sound.attackOn=1;editing.sound.attackType=2;
    editing.sound.attackLevel=-6;editing.sound.attackDecay=50;editing.sound.velocityVolume=0;editing.sound.output=1;editing.sound.sendA=-6.020599913;
    editing.sound.mode=2;editing.sound.envelopes[0]=drum::adsrShape(0,2,0.4,100);editing.sound.envelopes[0].points[2].bend=0.2;
    editing.sound.pitchAmount=12;editing.sound.envelopes[1]=drum::adsrShape(0,3,0,100);editing.sound.envelopes[1].points[2].bend=-0.2;editing.sound.keyOverride=23;
    check(setup.processor.getLibrary().setControls(setup.kit,setup.instrument,editing).ok,"Hosted attack setup failed");
    setup.processor.getStateInformation(state);
    envelope.getChildByName("IComponent")->deleteAllTextElements();envelope.getChildByName("IComponent")->addTextElement(state.toBase64Encoding());
    juce::AudioProcessor::copyXmlToBinary(envelope,hostState);plugin->setStateInformation(hostState.getData(),(int)hostState.getSize());
    std::array<float,drum::numMacros> macros;macros.fill(0.5f);
    auto expected=drum::SampleEngine::renderPreview(setup.processor.getSampleEngine().waveformFor(setup.instrument),editing,macros,120,48000);
    check(expected.sample!=nullptr,"Hosted attack reference missing");
    for(int recall=0;recall<2;++recall)
    {
        check(plugin->setBusesLayout(routing),"Hosted attack routing failed");plugin->prepareToPlay(48000,256);bool played=false;
        for(int attempt=0;attempt<500;++attempt)
        {
            midi.clear();midi.addEvent(juce::MidiMessage::allSoundOff(1),0);midi.addEvent(juce::MidiMessage::noteOn(1,36,(juce::uint8)127),0);
            plugin->processBlock(output,midi);if(output.getSample(2,0)>0.3f){played=true;break;}juce::Thread::sleep(10);
        }
        check(played,"Compiled VST3 mono/attack did not play");
        for(int n=0;n<256;++n)
        {
            const float value=expected.sample->audio.getSample(0,n);
            near(output.getSample(2,n),value,"Compiled VST3 attack differs from preview");
            near(output.getSample(3,n),value,"Compiled VST3 mono channels differ");near(output.getSample(4,n),value*0.5f,"Compiled VST3 attack send differs");
        }
        plugin->getStateInformation(recalled);plugin->releaseResources();plugin.reset();
        if(recall==0)
        {
            plugin=format.createInstanceFromDescription(*types[0],48000,256,error);check(plugin!=nullptr,"Hosted attack recall instance failed");
            plugin->setStateInformation(recalled.getData(),(int)recalled.getSize());
        }
    }
    std::cout << "Compiled VST3 sample/MIDI playback, mono/attack and session recall passed\n";
}
