#include "InstrumentSettingsPicker.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "InstrumentNotes.h"
#include "BuiltinEffects.h"
#include <stdexcept>
#include <iostream>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(const drum::Result& result){if(!result.ok)throw std::runtime_error(result.message.toStdString());}
drum::Instrument instrument(DrumAudioProcessor& p,const juce::String& id){auto state=p.getLibrary().snapshot();for(const auto& k:state.kits)if(auto* i=drum::findInstrument(k,id))return *i;throw std::runtime_error("Instrument missing");}
juce::Component* control(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* found=control(*child,id))return found;return nullptr;}
void capture(juce::Component& editor,const char* name){auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream output(file);check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),output),"Instrument screenshot failed");}
void archiveFile(const juce::File& file,const juce::String& manifest,const juce::String& sampleEntry,const juce::MemoryBlock& audio){juce::ZipFile::Builder zip;zip.addEntry(new juce::MemoryInputStream(manifest.toRawUTF8(),manifest.getNumBytesAsUTF8(),true),6,"Instrument.xml",juce::Time::getCurrentTime());if(sampleEntry.isNotEmpty())zip.addEntry(new juce::MemoryInputStream(audio,true),0,sampleEntry,juce::Time::getCurrentTime());juce::FileOutputStream output(file);check(zip.writeToStream(output,nullptr),"Cannot create test archive");}
}

void runInstrumentFileTests()
{
    {InstrumentSettingsPicker picker;check(!picker.selected().any(),"Settings picker must start unticked");picker.setAll(false);check(!picker.selected().any(),"Settings picker None failed");picker.choices[1].setToggleState(true,juce::dontSendNotification);auto choice=picker.selected();check(choice.amplitude && !choice.sample && !choice.effects && !choice.sendShapes,"Settings picker selected wrong section");picker.setAll(true);check(picker.selected().effects && picker.selected().sendLevels,"Settings picker All failed");capture(picker,"drum-settings-picker.png");}
    auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("instrument-files-"+juce::Uuid().toString());folder.createDirectory();
    auto sample=folder.getChildFile("Original.wav");auto stream=sample.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));check(writer!=nullptr,"Cannot write instrument fixture");
    juce::AudioBuffer<float> wave(2,4096);for(int n=0;n<4096;++n){wave.setSample(0,n,.25f);wave.setSample(1,n,-.125f);}check(writer->writeFromAudioSampleBuffer(wave,0,4096),"Cannot write audio");writer.reset();
    juce::MemoryBlock sampleBytes;check(sample.loadFileAsData(sampleBytes),"Cannot retain sample bytes");
    DrumAudioProcessor source;auto state=source.getLibrary().snapshot();auto kit=state.activeKit,group=state.kits[0].groups[0].id;auto id=source.getLibrary().addInstrument(kit,group).id;
    good(source.getLibrary().rename(id,"Wood - soft room"));source.getSampleEngine().requestLoad(kit,id,sample);check(source.getSampleEngine().waitForLoading(10000),"Instrument source timed out");
    auto controls=instrument(source,id).controls;controls.volumeDb=-6;controls.pan=.25;controls.tuning=7;controls.choke=3;controls.sound.sampleGain=-3;controls.sound.mode=2;controls.sound.sendA=-12;controls.sound.sendF=-24;controls.sound.attackOn=1;controls.sound.attackDecay=12;controls.sound.equalizer.enabled=true;controls.sound.equalizer.bands[15]={true,3,1200,-3,1};controls.sound.envelopes[0]=drum::adsrShape(5,40,.5,100);controls.macros[0]={1,-12,0};auto send=std::make_shared<drum::SendProcessing>();send->sound=controls.sound;send->tuning=-12;send->sound.cutoff=1700;controls.sendProcessing[5]=send;good(source.getLibrary().setControls(kit,id,controls));
    good(source.getLibrary().setNotes(kit,id,"Wood pair\nSoft strike; room A.\nKeep the first transient."));
    auto effect=source.getEffects().add(kit,id,drum::builtinDescription(drum::BuiltinKind::utility));good(effect);auto* utility=dynamic_cast<drum::BuiltinEffect*>(source.getEffects().instance(effect.id));check(utility!=nullptr,"Export effect unavailable");utility->controls[0]->setValueNotifyingHost(utility->controls[0]->convertTo0to1(-9));
    auto file=folder.getChildFile("Wood.druminstrument").withFileExtension("druminstrument");good(source.exportInstrument(kit,id,file));auto original=instrument(source,id);check(original.effects[0].state.isNotEmpty(),"Export did not capture live effect settings");
    auto moved=folder.getChildFile("Elsewhere/Wood.druminstrument").withFileExtension("druminstrument");moved.getParentDirectory().createDirectory();check(file.copyFileTo(moved),"Cannot move instrument file");check(sample.deleteFile(),"Cannot remove original fixture");
    DrumAudioProcessor target(true);target.newKit();auto destination=target.getLibrary().snapshot();auto targetKit=destination.activeKit,targetGroup=destination.kits[0].groups[0].id;
    auto imported=target.importInstrumentFile(targetKit,targetGroup,moved);good(imported);check(target.getSampleEngine().waitForLoading(10000),"Imported sample timed out");auto copy=instrument(target,imported.id);
    check(copy.name==original.name && copy.notes==original.notes && copy.controls==original.controls,"Instrument export lost settings or notes");
    check(copy.id!=original.id && copy.effects.size()==1 && copy.effects[0].id!=original.effects[0].id && copy.effects[0].state==original.effects[0].state,"Imported effect state or independent identity lost");
    check(juce::File(copy.samplePath).existsAsFile() && copy.samplePath!=sample.getFullPathName() && target.getSampleEngine().statusFor(copy.id).ready,"Export was not self-contained");
    juce::MemoryBlock importedBytes;juce::File(copy.samplePath).loadFileAsData(importedBytes);check(importedBytes==sampleBytes,"Export changed the source sample bytes");
    // Settings-only presets replace processing, never the destination identity/audio.
    auto preset=folder.getChildFile("Wood.drumsettings");good(source.saveInstrumentSettings(kit,id,preset));
    auto settingsTarget=target.getLibrary().addInstrument(targetKit,targetGroup).id;good(target.getLibrary().rename(settingsTarget,"Different snare"));good(target.getLibrary().setNotes(targetKit,settingsTarget,"Keep these destination notes"));
    target.getSampleEngine().requestLoad(targetKit,settingsTarget,juce::File(copy.samplePath));check(target.getSampleEngine().waitForLoading(10000),"Settings target sample timed out");auto identity=instrument(target,settingsTarget);auto returnBefore=target.getLibrary().snapshot().kits[0].returns[0].effects;
    good(target.loadInstrumentSettings(targetKit,settingsTarget,preset));auto applied=instrument(target,settingsTarget);
    check(applied.controls==original.controls && applied.effects.size()==original.effects.size() && applied.effects[0].state==original.effects[0].state && applied.effects[0].id!=original.effects[0].id,"Settings preset lost independent sends or effect state");
    check(applied.id==identity.id && applied.name==identity.name && applied.note==identity.note && applied.samplePath==identity.samplePath && applied.notes==identity.notes,"Settings load replaced sample or identity");
    // Every section can be selected independently; unselected values and slot IDs stay intact.
    using Selection=drum::InstrumentSettingsSelection;
    const Selection none{false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    const std::array<bool Selection::*,14> sections{&Selection::sample,&Selection::amplitude,&Selection::pitch,&Selection::filter,&Selection::equalizer,&Selection::lfo,&Selection::attack,&Selection::tuning,&Selection::mixer,&Selection::routing,&Selection::sendLevels,&Selection::sendShapes,&Selection::macros,&Selection::effects};
    for(auto section:sections){
        auto defaults=drum::InstrumentControls{};defaults.volumeDb=-21;defaults.pan=-.7;defaults.tuning=-5;defaults.sound.cutoff=900;defaults.sound.sendB=-9;defaults.sound.sampleGain=9;
        good(target.getLibrary().setControls(targetKit,settingsTarget,defaults));auto before=instrument(target,settingsTarget);auto choice=none;choice.*section=true;
        good(target.loadInstrumentSettings(targetKit,settingsTarget,preset,choice));auto after=instrument(target,settingsTarget);
        // Applying the same section to an isolated reference yields exactly the selected result.
        auto expected=before.controls;
        if(choice.sample){for(const auto& p:drum::soundParameters)if(p.page==0 || juce::String(p.key)=="bypassSample")expected.sound.*p.member=original.controls.sound.*p.member;expected.choke=original.controls.choke;}
        if(choice.amplitude){for(const auto& p:drum::soundParameters)if(juce::String(p.key)=="mode" || juce::String(p.key).startsWith("amp") || juce::String(p.key)=="velocityVolume" || juce::String(p.key)=="bypassAmplitude")expected.sound.*p.member=original.controls.sound.*p.member;expected.sound.envelopes[0]=original.controls.sound.envelopes[0];}
        if(choice.pitch){for(const auto& p:drum::soundParameters)if(juce::String(p.key).startsWith("pitch") || juce::String(p.key)=="bypassPitch")expected.sound.*p.member=original.controls.sound.*p.member;expected.sound.envelopes[1]=original.controls.sound.envelopes[1];}
        if(choice.filter){for(const auto& p:drum::soundParameters)if(p.page==2)expected.sound.*p.member=original.controls.sound.*p.member;expected.sound.envelopes[2]=original.controls.sound.envelopes[2];}
        if(choice.equalizer){expected.sound.equalizer=original.controls.sound.equalizer;}
        if(choice.lfo){for(const auto& p:drum::soundParameters)if(p.page==3)expected.sound.*p.member=original.controls.sound.*p.member;expected.sound.envelopes[3]=original.controls.sound.envelopes[3];}
        if(choice.attack){for(const auto& p:drum::soundParameters)if(p.page==6)expected.sound.*p.member=original.controls.sound.*p.member;}
        if(choice.tuning){expected.tuning=original.controls.tuning;expected.sound.detune=original.controls.sound.detune;expected.sound.velocityPitch=original.controls.sound.velocityPitch;expected.sound.bendRange=original.controls.sound.bendRange;expected.sound.keyOverride=original.controls.sound.keyOverride;}
        if(choice.mixer){expected.volumeDb=original.controls.volumeDb;expected.pan=original.controls.pan;expected.mute=original.controls.mute;expected.solo=original.controls.solo;expected.sound.randomPan=original.controls.sound.randomPan;}
        if(choice.routing)expected.sound.output=original.controls.sound.output;
        if(choice.sendLevels){for(const auto& p:drum::soundParameters)if(juce::String(p.key).startsWith("send"))expected.sound.*p.member=original.controls.sound.*p.member;}
        if(choice.sendShapes)expected.sendProcessing=original.controls.sendProcessing;
        if(choice.macros)expected.macros=original.controls.macros;
        if(choice.effects)expected.sound.bypassChain=original.controls.sound.bypassChain;
        check(after.controls==expected,"Selective settings load replaced an unchecked section");
        if(!choice.effects)check(after.effects==before.effects,"Unchecked effect chain changed slot identity/state");
        check(after.id==identity.id && after.samplePath==identity.samplePath && after.note==identity.note,"Selective settings load replaced destination audio/identity");
    }
    auto beforeEmpty=target.getLibrary().toXml()->toString();check(!target.loadInstrumentSettings(targetKit,settingsTarget,preset,none).ok && target.getLibrary().toXml()->toString()==beforeEmpty,"Empty section selection changed state");
    auto presetXml=juce::parseXML(preset);check(presetXml && !preset.loadFileAsString().contains(copy.samplePath) && presetXml->getTagName()=="DrumInstrumentSettings","Settings preset contains sample audio reference");
    good(target.loadInstrumentSettings(targetKit,settingsTarget,preset));auto savedTarget=target.getLibrary().toXml()->toString();auto invalidPreset=folder.getChildFile("Invalid.drumsettings");invalidPreset.replaceWithText("<DrumInstrumentSettings version=\"99\"/>");check(!target.loadInstrumentSettings(targetKit,settingsTarget,invalidPreset).ok && target.getLibrary().toXml()->toString()==savedTarget,"Invalid settings changed destination state");
    check(target.getLibrary().snapshot().kits[0].returns[0].effects==returnBefore,"Settings load changed shared return effects");
    // Kit -> Library uses exactly the same format and never replaces a tile.
    auto back=folder.getChildFile("Back.druminstrument").withFileExtension("druminstrument");good(target.exportInstrument(targetKit,copy.id,back));auto second=source.importInstrumentFile(kit,group,back);good(second);auto roundTrip=instrument(source,second.id);check(roundTrip.notes==copy.notes && roundTrip.controls==copy.controls && drum::instrumentCount(*drum::activeKit(source.getLibrary().snapshot()))==2,"Kit-to-Library import failed or replaced an instrument");
    // Notes stay attached to their captured identity, save immediately and recall.
    InstrumentNotes notes(source,kit,id);auto* text=dynamic_cast<juce::TextEditor*>(control(notes,"instrument-notes-text"));check(text!=nullptr,"Notes editor missing");text->setText("First instrument\nSecond line",false);text->onTextChange();check(instrument(source,id).notes==text->getText() && instrument(source,second.id).notes==copy.notes,"Notes were saved to the wrong instrument");
    juce::MemoryBlock saved;source.getStateInformation(saved);DrumAudioProcessor recalled;recalled.setStateInformation(saved.getData(),(int)saved.getSize());check(instrument(recalled,id).notes==text->getText(),"Notes did not recall with project");check(!source.getLibrary().setNotes(kit,id,juce::String::repeatedString("x",65537)).ok,"Oversized notes accepted");
    // Malformed or damaged archives must not change the collection.
    juce::ZipFile zip(moved);std::unique_ptr<juce::InputStream> manifestStream(zip.createStreamForEntry(zip.getIndexOfFileName("Instrument.xml")));check(manifestStream!=nullptr,"Export manifest missing");auto manifest=manifestStream->readEntireStreamAsString();auto xml=juce::parseXML(manifest);check(xml!=nullptr,"Export manifest invalid");
    auto before=target.getLibrary().toXml()->toString();auto bad=folder.getChildFile("Bad.druminstrument").withFileExtension("druminstrument");auto damaged=sampleBytes;static_cast<char*>(damaged.getData())[100]^=1;archiveFile(bad,manifest,"sample.wav",damaged);check(!target.importInstrumentFile(targetKit,targetGroup,bad).ok && target.getLibrary().toXml()->toString()==before,"Damaged audio partially imported");
    xml->setAttribute("sample","../escape.wav");xml->getChildByName("DrumState")->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument")->setAttribute("samplePath","../escape.wav");bad.deleteFile();archiveFile(bad,xml->toString(),"../escape.wav",sampleBytes);check(!target.importInstrumentFile(targetKit,targetGroup,bad).ok && target.getLibrary().toXml()->toString()==before,"Unsafe sample reference partially imported");
    juce::MemoryBlock preserved,after;file.loadFileAsData(preserved);check(!source.exportInstrument(kit,id,file).ok && file.loadFileAsData(after) && after==preserved,"Missing sample replaced an existing export");
    // Sidechain references survive note reassignment and never attach to an
    // unrelated instrument just because it occupies the old MIDI key.
    {
        DrumAudioProcessor refs;auto refState=refs.getLibrary().snapshot();auto refKit=refState.activeKit,refGroup=refState.kits[0].groups[0].id;auto voice=refs.getLibrary().addInstrument(refKit,refGroup).id,detector=refs.getLibrary().addInstrument(refKit,refGroup).id;good(refs.getLibrary().rename(detector,"Trigger"));good(refs.getLibrary().setNotes(refKit,voice,"No sample assigned"));auto duck=refs.getEffects().add(refKit,voice,drum::builtinDescription(drum::BuiltinKind::ducker));good(duck);good(refs.getEffects().sidechain(refKit,voice,duck.id,instrument(refs,detector).note,-6,.5,true));auto refFile=folder.getChildFile("Sidechain.druminstrument").withFileExtension("druminstrument");good(refs.exportInstrument(refKit,voice,refFile));
        auto detectors=target.getLibrary().addGroup(targetKit).id;auto trigger=target.getLibrary().addInstrument(targetKit,detectors).id;good(target.getLibrary().rename(trigger,"Trigger"));auto ret=drum::EffectSlot{juce::Uuid().toString(),drum::builtinDescription(drum::BuiltinKind::ducker).createXml()->toString(),{},false};ret.sidechainNote=instrument(target,trigger).note;good(target.getLibrary().setEffects(targetKit,"return-0",{ret}));
        auto linked=target.importInstrumentFile(targetKit,targetGroup,refFile);good(linked);auto linkedSound=instrument(target,linked.id);check(linkedSound.samplePath.isEmpty() && linkedSound.notes=="No sample assigned" && linkedSound.effects[0].sidechainNote==instrument(target,trigger).note && linkedSound.effects[0].sidechainMute && linkedSound.effects[0].sidechainGain==-6 && linkedSound.effects[0].sidechainMix==.5,"Named sidechain or sample-free instrument did not transfer");auto targetState=target.getLibrary().snapshot();check(drum::activeKit(targetState)->returns[0].effects[0].sidechainNote==instrument(target,trigger).note,"Import changed an existing return's sidechain source");
        good(target.getLibrary().removeInstrument(targetKit,trigger));auto unlinked=target.importInstrumentFile(targetKit,targetGroup,refFile);good(unlinked);check(unlinked.message.contains("unavailable") && instrument(target,unlinked.id).effects[0].sidechainNote==-1,"Missing sidechain was attached to another sound");
        good(refs.getEffects().sidechain(refKit,voice,duck.id,instrument(refs,voice).note,-3,.25,false));good(refs.exportInstrument(refKit,voice,refFile));auto self=target.importInstrumentFile(targetKit,targetGroup,refFile);good(self);check(instrument(target,self.id).effects[0].sidechainNote==instrument(target,self.id).note,"Self-sidechain did not follow imported copy");
    }
    // Notes/transfer controls are available in both workspaces and the editor.
    for(auto* p:{&source,&target}){DrumAudioProcessorEditor editor(*p);editor.setSize(1280,900);check(control(editor,"instrument-import") && control(editor,"instrument-export") && control(editor,"instrument-notes"),"Instrument actions missing in Library or Kit");auto* volume=dynamic_cast<juce::Slider*>(control(editor,"instrument-volume"));auto* pan=dynamic_cast<juce::Slider*>(control(editor,"instrument-pan"));check(volume && pan && volume->getSliderStyle()==juce::Slider::RotaryVerticalDrag && pan->getSliderStyle()==juce::Slider::RotaryVerticalDrag && bool(pan->getProperties()["bipolar"]),"Mixer controls are not the requested dials");capture(editor,p==&target?"drum-kit-instrument-panel.png":"drum-library-instrument-panel.png");auto* open=dynamic_cast<juce::TextButton*>(control(editor,"open-sound-editor"));open->onClick();check(control(editor,"editor-instrument-export")->isVisible() && control(editor,"editor-instrument-notes")->isVisible(),"Instrument editor actions missing");capture(editor,"drum-instrument-editor-actions.png");}
    capture(notes,"drum-instrument-notes.png");
    std::cout<<"Instrument files: portable audio, Library/Kit transfer, effects, controls, notes, validation and dial UI passed\n";
}
