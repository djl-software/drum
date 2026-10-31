#include "LegacyTestState.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void near(float actual,float expected,const char* message,float tolerance=0.0002f) {check(std::abs(actual-expected)<tolerance,message);}
juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;
    return nullptr;
}
void click(juce::Component& root,const char* id)
{
    auto* button=dynamic_cast<juce::TextButton*>(find(root,id));check(button!=nullptr,"Editing button missing");button->onClick();
}
void capture(juce::Component& editor,const juce::String& name)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);
    check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),out),"Screenshot failed");
}
struct Rig
{
    DrumAudioProcessor p;
    juce::String kit,group,id;
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    drum::InstrumentControls controls;
    Rig()
    {
        auto state=p.getLibrary().snapshot();kit=state.activeKit;group=drum::activeKit(state)->groups.front().id;
        id=p.getLibrary().addInstrument(kit,group).id;load(0);
    }
    void load(int shape)
    {
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/edit-"+juce::String(shape)+".wav");
        file.getParentDirectory().createDirectory();file.deleteFile();auto stream=file.createOutputStream();juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));check(writer!=nullptr,"Editing fixture writer failed");
        juce::AudioBuffer<float> data(1,48000);
        for(int n=0;n<data.getNumSamples();++n)data.setSample(0,n,shape==0?0.25f:shape==1?(float)(0.1+0.8*n/47999.0):(float)(0.25*std::sin(juce::MathConstants<double>::twoPi*10000*n/48000)));
        check(writer->writeFromAudioSampleBuffer(data,0,data.getNumSamples()),"Editing fixture write failed");writer.reset();
        p.getSampleEngine().requestLoad(kit,id,file);wait();
    }
    void wait() {check(p.getSampleEngine().waitForLoading(10000),"Editing bank timed out");}
    void set() {check(p.getLibrary().setControls(kit,id,controls).ok,"Editing settings rejected");wait();}
    void reset(int frames=4096)
    {
        p.prepareToPlay(48000,frames);audio.setSize(p.getTotalNumOutputChannels(),frames);midi.clear();
    }
    void hit(int frame=0,int velocity=127,int channel=1) {midi.addEvent(juce::MidiMessage::noteOn(channel,21,(juce::uint8)velocity),frame);}
    void off(int frame,int channel=1) {midi.addEvent(juce::MidiMessage::noteOff(channel,21),frame);}
    void render() {p.processBlock(audio,midi);}
};
}
void runEditingTests()
{
    Rig r;
    r.controls.sound.start=0.25;r.controls.sound.end=0.5;r.set();r.reset(14000);r.hit();r.render();
    near(r.audio.getSample(0,1000),0.25f,"Trimmed sample level incorrect");near(r.audio.getSample(0,12001),0,"End marker ignored");
    r.load(1);r.controls.sound.reverse=1;r.set();r.reset();r.hit();r.render();
    near(r.audio.getSample(0,0),0.5f,"Reverse did not start at trimmed end",0.001f);
    check(r.audio.getSample(0,1000)<r.audio.getSample(0,100),"Reverse did not move backwards");
    r.load(0);r.controls={};r.controls.sound.mode=1;r.controls.sound.fadeOut=10;r.set();r.reset();r.hit();r.off(1000);r.render();
    near(r.audio.getSample(0,999),0.25f,"Gate released before note-off");near(r.audio.getSample(0,1600),0,"Gate failed to release");
    r.controls.sound.mode=0;r.set();r.reset();r.hit();r.off(1000);r.render();near(r.audio.getSample(0,1600),0.25f,"Trigger obeyed note-off");
    r.controls.sound.mode=2;r.controls.sound.ampAttack=10;r.controls.sound.ampDecay=10;r.controls.sound.ampSustain=0.5;r.controls.sound.ampRelease=20;r.set();
    r.reset();r.hit();r.off(2000);r.render();
    check(r.audio.getSample(0,0)<0.001f && r.audio.getSample(0,479)>0.249f,"ADSR attack incorrect");
    near(r.audio.getSample(0,1500),0.125f,"ADSR sustain incorrect");near(r.audio.getSample(0,3000),0,"ADSR release incorrect");
    r.controls={};r.controls.sound.mode=2;r.controls.sound.loop=1;r.controls.sound.start=0.1;r.controls.sound.end=0.2;r.controls.sound.loopStart=0.25;r.controls.sound.loopEnd=0.75;r.controls.sound.loopFade=4;r.controls.sound.ampRelease=10;r.set();
    r.reset(30000);r.hit();r.off(20000);r.render();near(r.audio.getSample(0,19000),0.25f,"Classic loop failed to sustain");near(r.audio.getSample(0,25000),0,"Loop failed to release");
    r.controls.sound.reverse=1;r.set();r.reset(30000);r.hit();r.off(20000);r.render();near(r.audio.getSample(0,19000),0.25f,"Reverse loop failed");near(r.audio.getSample(0,25000),0,"Reverse loop failed to release");
    r.controls={};r.controls.sound.voices=1;r.set();r.reset();r.hit();r.hit(100);r.render();near(r.audio.getSample(0,400),0.25f,"Instrument voice limit ignored");
    r.controls.sound.voices=2;r.set();r.reset();r.hit();r.hit(100);r.render();near(r.audio.getSample(0,400),0.5f,"Polyphony overlap lost");
    r.controls.sound.retrigger=1;r.set();r.reset();r.hit();r.hit(100);r.render();near(r.audio.getSample(0,400),0.25f,"Retrigger did not cut old voice");
    r.controls={};r.controls.sound.normalise=1;r.set();r.reset();r.hit();r.render();near(r.audio.getSample(0,0),1,"Normalise incorrect");
    r.controls={};r.controls.sound.velocityVolume=0;r.set();r.reset();r.hit(0,32);r.render();near(r.audio.getSample(0,0),0.25f,"Velocity amount ignored");
    r.controls={};r.controls.sound.end=0.1;r.controls.sound.pitchAmount=12;r.controls.sound.pitchSustain=1;r.set();r.reset(5000);r.hit();r.render();check(r.audio.getSample(0,2300)>0 && r.audio.getSample(0,2401)==0,"Pitch envelope did not change playback rate");
    r.controls={};r.controls.sound.filterOn=1;r.controls.sound.filterType=1;r.controls.sound.cutoff=500;r.set();r.reset();r.hit();r.render();check(r.audio.getMagnitude(3000,1000)<0.0001f,"High-pass failed to reject DC");
    r.load(2);r.controls.sound.filterType=0;r.controls.sound.filterSlope=1;r.set();r.reset();r.hit();r.render();check(r.audio.getMagnitude(3000,1000)<0.001f,"24 dB low-pass failed to reject high frequencies");
    // Extreme modulation must remain finite at all supported rates.
    r.controls.sound.resonance=10;r.controls.sound.drive=24;r.controls.sound.filterAmount=8;r.controls.sound.lfoOn=1;r.controls.sound.lfoFilter=8;r.controls.sound.lfoRate=30;r.set();
    for(double rate:{44100.0,48000.0,96000.0})
    {
        r.reset(12000);r.p.prepareToPlay(rate,12000);r.hit();r.render();
        for(int c=0;c<r.audio.getNumChannels();++c)for(int n=0;n<r.audio.getNumSamples();++n)check(std::isfinite(r.audio.getSample(c,n)) && std::abs(r.audio.getSample(c,n))<100,"Filter became unstable");
    }
    r.load(0);r.controls={};r.controls.sound.lfoOn=1;r.controls.sound.lfoShape=2;r.controls.sound.lfoPhase=180;r.controls.sound.lfoVolume=1;r.set();r.reset();r.hit();r.render();near(r.audio.getMagnitude(0,4096),0,"LFO volume modulation ignored");
    r.controls.sound.lfoOn=0;r.set();r.reset();r.hit();r.render();near(r.audio.getSample(0,0),0.25f,"Disabled LFO still changed volume");
    r.controls={};r.controls.sound.mode=2;r.controls.sound.loop=1;r.controls.sound.end=0.1;r.set();
    auto mutedCopy=r.p.getLibrary().duplicateInstrument(r.kit,r.id);check(mutedCopy.ok,"Muted preview fixture failed");
    auto mutedControls=r.controls;mutedControls.mute=true;check(r.p.getLibrary().setControls(r.kit,mutedCopy.id,mutedControls).ok,"Muted preview controls failed");r.wait();
    r.reset(48000);r.hit();r.render();r.p.getSampleEngine().audition(22);r.render();near(r.audio.getSample(0,40000),0.25f,"Muted preview released another held instrument");
    check(r.p.getLibrary().removeInstrument(r.kit,mutedCopy.id).ok,"Muted preview fixture cleanup failed");r.wait();
    r.controls={};r.controls.sound.bendRange=12;r.controls.sound.end=0.1;r.set();r.reset(5000);r.midi.addEvent(juce::MidiMessage::pitchWheel(1,16383),0);r.hit();r.render();check(r.audio.getSample(0,2403)==0,"Pitch bend not applied");
    r.controls={};r.controls.macros[0]={1,-6.020599913,0};r.set();r.p.macro(0).setValueNotifyingHost(0);r.reset();r.hit();r.render();near(r.audio.getSample(0,0),0.125f,"Macro gain mapping failed");
    r.p.macro(0).setValueNotifyingHost(1);r.render();near(r.audio.getSample(0,400),0.25f,"Macro automation did not update playing voice");
    r.controls={};r.controls.sound.output=1;r.controls.sound.sendA=-6.020599913;r.set();r.reset();r.hit();r.render();near(r.audio.getSample(0,0),0.25f,"Disabled output did not fall back to Main");
    auto layout=r.p.getBusesLayout();layout.outputBuses.set(1,juce::AudioChannelSet::stereo());layout.outputBuses.set(16,juce::AudioChannelSet::stereo());
    check(r.p.setBusesLayout(layout),"Aux outputs rejected");r.reset();r.hit();r.render();
    near(r.audio.getMagnitude(0,0,4096),0,"Routed sound leaked to Main");near(r.audio.getSample(2,0),0.25f,"Aux output silent");near(r.audio.getSample(4,0),0.125f,"Send output incorrect");
    layout.outputBuses.set(0,juce::AudioChannelSet::mono());check(r.p.setBusesLayout(layout),"Mono plus auxiliary outputs rejected");r.reset();r.hit();r.render();near(r.audio.getSample(1,0),0.25f,"Aux offset after mono Main incorrect");near(r.audio.getSample(3,0),0.125f,"Send offset after mono Main incorrect");

    r.controls={};for(auto& p:drum::soundParameters)
    {
        auto value=p.minimum+(p.maximum-p.minimum)*0.25;if(p.step==1)value=std::floor(value);
        r.controls.sound.*(p.member)=value;
    }
    r.controls.sound.end=0.9;r.controls.sound.loopEnd=0.8;r.controls.macros[15]={3,12,-12};r.set();
    r.p.macro(15).setValueNotifyingHost(0.75f);juce::MemoryBlock saved;r.p.getStateInformation(saved);DrumAudioProcessor restored;
    restored.setStateInformation(saved.getData(),(int)saved.getSize());auto state=restored.getLibrary().snapshot();
    check(drum::findInstrument(*drum::activeKit(state),r.id)->controls==r.controls,"Editing state round trip lost fields");near(restored.macro(15).get(),0.75f,"Macro value lost in recall");
    auto xml=r.p.getLibrary().toXml();auto* instrument=xml->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument");
    instrument->setAttribute("edit_start",0.99);check(!restored.getLibrary().restore(*xml),"Invalid region accepted");
    instrument->setAttribute("edit_start","nan");check(!restored.getLibrary().restore(*xml),"NaN editing value accepted");
    xml=r.p.getLibrary().toXml();stripChainEffects(*xml);xml->setAttribute("schemaVersion",4);check(restored.getLibrary().restore(*xml),"Version 4 migration failed");state=restored.getLibrary().snapshot();check(drum::findInstrument(*drum::activeKit(state),r.id)->controls.sound==drum::SoundParameters{},"Legacy sound did not keep defaults");
    auto slices=r.p.getLibrary().sliceInstrument(r.kit,r.id,8);check(slices.ok,"Slicing failed");state=r.p.getLibrary().snapshot();auto* sliced=drum::findGroup(*drum::activeKit(state),slices.id);
    check(sliced && sliced->instruments.size()==8 && drum::instrumentCount(*drum::activeKit(state))==9,"Slicing lost original or wrong count");
    check(sliced->instruments.front().controls.sound.start==r.controls.sound.start && sliced->instruments.back().controls.sound.end==r.controls.sound.end,"Slices ignored selected region");
    for(int n=1;n<8;++n)check(sliced->instruments[(size_t)n].controls.sound.start==sliced->instruments[(size_t)(n-1)].controls.sound.end,"Slices contain gaps");
    auto before=r.p.getLibrary().toXml()->toString();check(!r.p.getLibrary().sliceInstrument(r.kit,r.id,88).ok && before==r.p.getLibrary().toXml()->toString(),"Rejected slicing changed kit");

    r.controls={};r.set();DrumAudioProcessorEditor editor(r.p);editor.setSize(1280,900);click(editor,"open-sound-editor");
    auto* reverse=dynamic_cast<juce::ComboBox*>(find(editor,"edit-reverse"));check(reverse!=nullptr,"Reverse UI missing");reverse->setSelectedId(2,juce::sendNotificationSync);
    auto* attack=dynamic_cast<juce::Slider*>(find(editor,"edit-ampAttack"));check(attack!=nullptr,"Envelope UI missing");attack->setValue(25,juce::sendNotificationSync);
    state=r.p.getLibrary().snapshot();auto sound=drum::findInstrument(*drum::activeKit(state),r.id)->controls.sound;check(sound.reverse==1 && sound.ampAttack==25,"Editing UI not saved");
    for(int page=0;page<7;++page)
    {
        click(editor,("edit-tab-"+juce::String(page)).toRawUTF8());capture(editor,"drum-editor-"+juce::String(page)+".png");
        for(auto& p:drum::soundParameters)check((find(editor,juce::String("edit-")+p.key)!=nullptr)==(p.member!=&drum::SoundParameters::keyOverride),"Sound control visibility incorrect");
    }
    editor.setSize(800,600);click(editor,"edit-tab-0");capture(editor,"drum-editor-compact.png");
    auto* target=dynamic_cast<juce::ComboBox*>(find(editor,"macro-target-0"));target->setSelectedId(2,juce::sendNotificationSync);
    state=r.p.getLibrary().snapshot();check(drum::findInstrument(*drum::activeKit(state),r.id)->controls.macros[0].target==1,"Macro mapping UI not saved");
    click(editor,"close-sound-editor");check(!find(editor,"sound-editor")->isVisible(),"Back to kit failed");
    std::cout<<"Editing, envelopes, loops, filter, modulation, routing, macros, slicing and UI tests passed\n";
}
