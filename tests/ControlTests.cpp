#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void near(float actual,float expected,const char* message) { check(std::abs(actual-expected)<0.0001f,message); }
juce::Component* control(juce::Component& root,const char* id)
{
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* found=control(*child,id)) return found;
    return nullptr;
}
struct Rig
{
    DrumAudioProcessor p;
    juce::String kit,group,a,b;
    juce::File file;
    juce::AudioBuffer<float> audio{2,512};
    juce::MidiBuffer midi;
    Rig()
    {
        file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/control.wav");
        file.getParentDirectory().createDirectory(); file.deleteFile(); auto stream=file.createOutputStream();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),44100,1,24,{},0));
        check(writer!=nullptr,"Control fixture writer failed");
        juce::AudioBuffer<float> data(1,4410); for(int n=0;n<4410;++n) data.setSample(0,n,0.25f);
        check(writer->writeFromAudioSampleBuffer(data,0,4410),"Control fixture write failed"); writer.reset();
        auto s=p.getLibrary().snapshot(); kit=s.activeKit; group=drum::activeKit(s)->groups.front().id;
        a=p.getLibrary().addInstrument(kit,group).id;
        auto secondGroup=p.getLibrary().addGroup(kit).id; b=p.getLibrary().addInstrument(kit,secondGroup).id;
        check(p.getLibrary().assignNote(kit,a,36).ok && p.getLibrary().assignNote(kit,b,37).ok,"Control note setup failed");
        p.getSampleEngine().requestLoad(kit,a,file); p.getSampleEngine().requestLoad(kit,b,file); wait(); reset();
    }
    void wait() { check(p.getSampleEngine().waitForLoading(10000),"Controls bank timed out"); }
    void set(const juce::String& id,drum::InstrumentControls c) { check(p.getLibrary().setControls(kit,id,c).ok,"Control edit failed"); wait(); }
    void reset(int frames=512) { p.prepareToPlay(48000,frames); audio.setSize(2,frames); midi.clear(); }
    void hit(int note,int offset=0,int channel=1) { midi.addEvent(juce::MidiMessage::noteOn(channel,note,(juce::uint8)127),offset); }
    void render() { p.processBlock(audio,midi); }
};
}
void runControlTests()
{
    Rig r; drum::InstrumentControls c;
    c.volumeDb=-6.020599913; r.set(r.a,c); r.hit(36); r.render();
    near(r.audio.getSample(0,0),0.125f,"Instrument volume incorrect");
    c.volumeDb=0; r.set(r.a,c); r.render();
    check(r.audio.getSample(0,0)>0.125f && r.audio.getSample(0,0)<0.13f,"Volume update restarted or cut sounding voice");
    near(r.audio.getSample(0,400),0.25f,"Smoothed volume did not reach target");
    c.pan=1; r.set(r.a,c); r.render();
    near(r.audio.getSample(0,400),0,"Hard right pan left channel"); near(r.audio.getSample(1,400),0.25f,"Hard right pan right channel");
    c.pan=-1; r.set(r.a,c); r.reset(); r.hit(36); r.render();
    near(r.audio.getSample(0,0),0.25f,"Hard left pan left channel"); near(r.audio.getSample(1,0),0,"Hard left pan right channel");
    c={}; c.volumeDb=-60; r.set(r.a,c); r.reset(); r.hit(36); r.render(); near(r.audio.getMagnitude(0,512),0,"Minimum volume not silent");
    c={}; c.tuning=12; r.set(r.a,c); r.reset(5000); r.hit(36); r.render();
    check(r.audio.getSample(0,2390)>0 && r.audio.getSample(0,2401)==0,"Octave-up tuning duration incorrect");
    c.tuning=-12; r.set(r.a,c); r.reset(10000); r.hit(36); r.render();
    check(r.audio.getSample(0,9590)>0 && r.audio.getSample(0,9601)==0,"Octave-down tuning duration incorrect");
    c={}; r.set(r.a,c); r.reset(); r.hit(36); r.render(); c.mute=true; r.set(r.a,c); r.render();
    near(r.audio.getSample(0,200),0,"Mute did not stop sounding voice");
    c.mute=false; r.set(r.a,c); r.render(); near(r.audio.getMagnitude(0,512),0,"Unmute resurrected old voice");
    drum::InstrumentControls b; b.solo=true; r.set(r.b,b); r.reset(); r.hit(36); r.render();
    near(r.audio.getMagnitude(0,512),0,"Solo did not silence other organisational group");
    r.hit(37); r.render(); near(r.audio.getSample(0,0),0.25f,"Soloed instrument silent");
    c.solo=true; r.set(r.a,c); r.reset(); r.hit(36); r.hit(37); r.render(); near(r.audio.getSample(0,0),0.5f,"Multiple solo not additive");
    b.mute=true; r.set(r.b,b); r.reset(); r.hit(37); r.render(); near(r.audio.getMagnitude(0,512),0,"Mute did not take precedence over solo");
    c={}; b={}; c.choke=b.choke=1; r.set(r.a,c); r.set(r.b,b);
    r.reset(); r.hit(36); r.hit(37,100,2); r.render();
    near(r.audio.getSample(0,99),0.25f,"Choke happened before MIDI event");
    near(r.audio.getSample(0,100),0.5f,"Choke release did not start with new hit");
    near(r.audio.getSample(0,197),0.25f,"Choke did not cross MIDI channels and organisational groups");
    r.reset(); r.hit(36); r.hit(36,100); r.render(); near(r.audio.getSample(0,197),0.25f,"Choke did not stop earlier hit of same instrument");
    b.choke=2; r.set(r.b,b); r.reset(); r.hit(36); r.hit(37,100); r.render(); near(r.audio.getSample(0,197),0.5f,"Different choke groups cut each other");
    c.choke=0; r.set(r.a,c); r.reset(); r.hit(36); r.hit(36,100); r.render(); near(r.audio.getSample(0,197),0.5f,"Choke Off did not allow overlap");
    c.choke=1; b.choke=1; b.mute=true; r.set(r.a,c); r.set(r.b,b); r.reset(); r.hit(36); r.hit(37,100); r.render();
    near(r.audio.getSample(0,197),0.25f,"Muted hit choked audible instrument");

    c={-9.5,-0.25,3.25,true,true,16}; r.set(r.a,c);
    auto copy=r.p.getLibrary().duplicateInstrument(r.kit,r.a); check(copy.ok,"Copy with controls failed");
    auto s=r.p.getLibrary().snapshot(); check(drum::findInstrument(*drum::activeKit(s),copy.id)->controls==c,"Copy lost controls");
    juce::MemoryBlock saved; r.p.getStateInformation(saved); DrumAudioProcessor restored;
    restored.setStateInformation(saved.getData(),(int)saved.getSize()); s=restored.getLibrary().snapshot();
    check(drum::findInstrument(*drum::activeKit(s),r.a)->controls==c,"State recall lost controls");
    auto xml=r.p.getLibrary().toXml(); auto before=xml->toString();
    auto* element=xml->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument");
    element->setAttribute("pan","0.25garbage"); check(!r.p.getLibrary().restore(*xml),"Malformed control value accepted");
    check(r.p.getLibrary().toXml()->toString()==before,"Bad state changed library");
    xml=r.p.getLibrary().toXml(); xml->setAttribute("schemaVersion",3);
    check(restored.getLibrary().restore(*xml),"Version 3 migration failed"); s=restored.getLibrary().snapshot();
    check(drum::findInstrument(*drum::activeKit(s),r.a)->controls==drum::InstrumentControls{},"Legacy controls not neutral");
    c.pan=std::numeric_limits<double>::quiet_NaN(); check(!r.p.getLibrary().setControls(r.kit,r.a,c).ok,"NaN control accepted");

    r.set(r.a,{}); r.set(r.b,{});
    DrumAudioProcessorEditor editor(r.p);
    auto* volume=dynamic_cast<juce::Slider*>(control(editor,"instrument-volume")); check(volume!=nullptr,"Volume slider missing");
    volume->setValue(-12,juce::sendNotificationSync);
    auto* pan=dynamic_cast<juce::Slider*>(control(editor,"instrument-pan")); check(pan!=nullptr,"Pan slider missing"); pan->setValue(50,juce::sendNotificationSync);
    auto* tune=dynamic_cast<juce::Slider*>(control(editor,"instrument-tuning")); check(tune!=nullptr,"Tuning slider missing"); tune->setValue(7,juce::sendNotificationSync);
    auto* mute=dynamic_cast<juce::TextButton*>(control(editor,"instrument-mute")); check(mute!=nullptr,"Mute missing"); mute->setToggleState(true,juce::dontSendNotification); mute->onClick();
    auto* solo=dynamic_cast<juce::TextButton*>(control(editor,"instrument-solo")); check(solo!=nullptr,"Solo missing"); solo->setToggleState(true,juce::dontSendNotification); solo->onClick();
    auto* choke=dynamic_cast<juce::ComboBox*>(control(editor,"instrument-choke")); check(choke!=nullptr,"Choke selector missing"); choke->setSelectedId(5,juce::dontSendNotification); choke->onChange();
    s=r.p.getLibrary().snapshot(); auto ui=drum::findInstrument(*drum::activeKit(s),r.a)->controls;
    check(ui==drum::InstrumentControls{-12,0.5,7,true,true,4},"UI controls not applied");
    auto imageFile=juce::File::getCurrentWorkingDirectory().getChildFile("drum-controls.png"); imageFile.deleteFile();
    juce::FileOutputStream imageStream(imageFile); juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),imageStream);

    DrumAudioProcessor dropped; DrumAudioProcessorEditor dropEditor(dropped);
    auto* target=dynamic_cast<juce::FileDragAndDropTarget*>(control(dropEditor,"instrument-grid")); check(target!=nullptr,"Grid is not a drop target");
    target->filesDropped({r.file.getFullPathName(),r.file.getFullPathName()},10,150);
    check(dropped.getSampleEngine().waitForLoading(10000),"Bulk drop timed out");
    s=dropped.getLibrary().snapshot(); auto* k=drum::activeKit(s);
    check(drum::instrumentCount(*k)==2 && k->groups.front().instruments.front().name=="control","Bulk drop did not create named instruments");
    check(k->groups.front().instruments[0].note!=k->groups.front().instruments[1].note,"Bulk drop assigned duplicate keys");
    auto prior=dropped.getLibrary().toXml()->toString();
    target->filesDropped({r.file.withFileExtension("adg").getFullPathName()},10,150);
    check(prior==dropped.getLibrary().toXml()->toString(),"Unsupported preset changed kit");
    auto invalid=r.file.getSiblingFile("corrupt-drop.wav"); invalid.replaceWithText("not audio");
    dropped.getSampleEngine().requestImport(s.activeKit,k->groups.front().id,invalid);
    check(dropped.getSampleEngine().waitForLoading(10000),"Bad drop timeout");
    check(prior==dropped.getLibrary().toXml()->toString(),"Bad audio drop created empty instrument");
    std::cout << "Instrument controls, choke, migration, UI and drop tests passed\n";
}
