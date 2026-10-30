#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ChainEditor.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(float a,float b,const char* message){if(std::abs(a-b)>0.0001f){std::cerr<<message<<": "<<a<<" expected "<<b<<'\n';throw std::runtime_error(message);}}
void pump(int ms=30){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);juce::Timer::callPendingTimersSynchronously();}
juce::File sampleFile(const juce::String& name,float value,int frames=4800)
{
    auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("chain-fixtures");folder.createDirectory();auto file=folder.getChildFile(name+".wav");file.deleteFile();
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(file),48000,2,24,{},0));check(writer!=nullptr,"Chain sample writer failed");
    juce::AudioBuffer<float> audio(2,frames);for(int c=0;c<2;++c)for(int n=0;n<frames;++n)audio.setSample(c,n,value);
    check(writer->writeFromAudioSampleBuffer(audio,0,frames),"Chain fixture write failed");return file;
}
juce::Component* find(juce::Component& root,const juce::String& id)
{if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;return nullptr;}
void click(juce::Component& root,const char* id){auto* button=dynamic_cast<juce::Button*>(find(root,id));check(button!=nullptr,"Chain button missing");button->onClick();}
void capture(juce::Component& editor,const char* name)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream output(file);
    check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),output),"Chain screenshot failed");
}
struct Setup
{
    DrumAudioProcessor processor;juce::String kit,group,a,b;
    Setup()
    {
        auto state=processor.getLibrary().snapshot();kit=state.activeKit;group=state.kits[0].groups[0].id;
        a=processor.getLibrary().addInstrument(kit,group).id;b=processor.getLibrary().addInstrument(kit,group).id;
        processor.getLibrary().rename(a,"Kick");processor.getLibrary().rename(b,"Snare");
        auto routes=processor.getBusesLayout();routes.outputBuses.set(1,juce::AudioChannelSet::stereo());routes.outputBuses.set(2,juce::AudioChannelSet::stereo());routes.outputBuses.set(16,juce::AudioChannelSet::stereo());
        check(processor.setBusesLayout(routes),"Chain output layout failed");processor.prepareToPlay(48000,64);
        auto& engine=processor.getSampleEngine();engine.requestLoad(kit,a,sampleFile("kick",0.25f));engine.requestLoad(kit,b,sampleFile("snare",0.5f));check(engine.waitForLoading(10000),"Chain sources failed");
        drum::InstrumentControls c;c.sound.output=1;c.sound.sendA=-6.020599913;processor.getLibrary().setControls(kit,a,c);c.sound.output=2;c.sound.sendA=-60;processor.getLibrary().setControls(kit,b,c);
        settle();
    }
    void settle(){processor.getEffects().sync();check(processor.getSampleEngine().waitForLoading(10000),"Chain bank timeout");}
    juce::AudioBuffer<float> hit(int frames=64,int offset=0)
    {
        processor.getSampleEngine().stop();juce::AudioBuffer<float> silence(8,64);juce::MidiBuffer midi;processor.processBlock(silence,midi);
        juce::AudioBuffer<float> audio(8,frames);midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),offset);midi.addEvent(juce::MidiMessage::noteOn(1,22,(juce::uint8)127),offset);processor.processBlock(audio,midi);return audio;
    }
    drum::Instrument instrument(const juce::String& id){auto s=processor.getLibrary().snapshot();return *drum::findInstrument(*drum::activeKit(s),id);}
};
void parameter(drum::EffectRack& rack,const juce::String& slot,int index,float value)
{auto* plugin=rack.instance(slot);check(plugin!=nullptr,"Hosted effect not loaded");plugin->getParameters()[index]->setValueNotifyingHost(value);}
}
void runChainTests(const juce::String& effectPath,const juce::String& drumPath)
{
    Setup setup;auto& rack=setup.processor.getEffects();auto& library=setup.processor.getLibrary();
    auto found=rack.discover(juce::File(effectPath));check(found.size()==1,"External VST3 fixture not discovered");auto description=*found[0];
    auto empty=setup.hit();near(empty.getSample(2,0),0.25f,"Empty chain changed audio");near(empty.getSample(4,0),0.5f,"Empty second chain changed audio");
    auto first=rack.add(setup.kit,setup.a,description);auto second=rack.add(setup.kit,setup.a,description);check(first.ok && second.ok,"Effects could not be added");setup.settle();
    check(rack.supportsSidechain(first.id),"Stereo sidechain not enabled");
    parameter(rack,first.id,4,1);rack.bypass(setup.kit,setup.a,second.id,true);
    check(rack.sidechain(setup.kit,setup.a,first.id,22,-6.020599913,1,true).ok,"Sidechain setup failed");setup.settle();
    auto sideAudio=setup.hit();near(sideAudio.getSample(2,0),0.25f,"Sidechain source or gain incorrect");near(sideAudio.getSample(4,0),0,"Sidechain source mute failed");
    rack.sidechain(setup.kit,setup.a,first.id,22,0,0.5,false);setup.settle();sideAudio=setup.hit();near(sideAudio.getSample(2,0),0.375f,"Sidechain mix incorrect");near(sideAudio.getSample(4,0),0.5f,"Sidechain source did not unmute");
    rack.sidechain(setup.kit,setup.a,first.id,-1,0,1,false);parameter(rack,first.id,4,0);rack.bypass(setup.kit,setup.a,second.id,false);setup.settle();
    parameter(rack,second.id,0,0.5f);parameter(rack,second.id,1,0.6f);
    auto audio=setup.hit();near(audio.getSample(2,0),0.225f,"Serial chain order incorrect");near(audio.getSample(6,0),0.1125f,"Sends do not receive chain output");near(audio.getSample(4,0),0.5f,"Effect leaked into another instrument");
    check(rack.move(setup.kit,setup.a,second.id,0).ok,"Reorder failed");setup.settle();audio=setup.hit();near(audio.getSample(2,0),0.175f,"Reordering did not change processing order");
    check(rack.bypass(setup.kit,setup.a,first.id,true).ok,"Bypass failed");setup.settle();audio=setup.hit();near(audio.getSample(2,0),0.35f,"Bypass changed dry signal");
    check(rack.bypass(setup.kit,setup.a,second.id,true).ok,"Second bypass failed");setup.settle();audio=setup.hit();near(audio.getSample(2,0),0.25f,"Fully bypassed chain not transparent");
    rack.bypass(setup.kit,setup.a,first.id,false);rack.bypass(setup.kit,setup.a,second.id,false);setup.settle();
    juce::MemoryBlock saved;setup.processor.getStateInformation(saved);
    DrumAudioProcessor recalled;recalled.setStateInformation(saved.getData(),(int)saved.getSize());recalled.setBusesLayout(setup.processor.getBusesLayout());recalled.prepareToPlay(48000,64);recalled.getEffects().sync();check(recalled.getSampleEngine().waitForLoading(10000),"Chain recall source failed");
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);juce::AudioBuffer<float> recalledAudio(8,64);recalled.processBlock(recalledAudio,midi);near(recalledAudio.getSample(2,0),0.175f,"Effect order or parameters lost on recall");
    auto duplicate=library.duplicateInstrument(setup.kit,setup.a);check(duplicate.ok,"Instrument with effects could not be duplicated");setup.settle();
    auto original=setup.instrument(setup.a),copy=setup.instrument(duplicate.id);check(copy.effects.size()==2 && copy.effects[0].id!=original.effects[0].id && copy.effects[0].state==original.effects[0].state,"Copied chain did not retain independent state");
    auto* copyPlugin=rack.instance(copy.effects[1].id);check(copyPlugin && copyPlugin!=rack.instance(first.id),"Copied chain shares plugin instance");library.removeInstrument(setup.kit,duplicate.id);setup.settle();
    // Switching kits captures live plugin edits even before the periodic save.
    parameter(rack,second.id,1,0.7f);auto otherKit=library.addKit();check(otherKit.ok,"Second kit failed");setup.settle();
    check(rack.instance(first.id)==nullptr,"Inactive kit kept its live effect instance");
    library.activateKit(setup.kit);setup.settle();audio=setup.hit();near(audio.getSample(2,0),0.225f,"Kit switch lost live plugin parameters");
    parameter(rack,second.id,1,0.6f);library.removeKit(otherKit.id);setup.settle();
    juce::AudioBuffer<float> zero(8,0);midi.clear();midi.addEvent(juce::MidiMessage::allSoundOff(1),0);setup.processor.processBlock(zero,midi);
    midi.clear();setup.processor.processBlock(recalledAudio,midi);near(recalledAudio.getSample(4,0),0,"Zero-frame MIDI did not stop dry instrument");
    // Latency must be compensated for the untouched instrument too.
    parameter(rack,first.id,0,0.5f);parameter(rack,first.id,1,0.5f);parameter(rack,second.id,0,0.5f);parameter(rack,second.id,1,0.5f);parameter(rack,first.id,2,0.5f);
    setup.hit();rack.sync();setup.processor.releaseResources();setup.processor.prepareToPlay(48000,64);setup.settle();
    check(setup.processor.getLatencySamples()==32,"Hosted latency not reported to DAW");audio=setup.hit(192,8);
    near(audio.getSample(2,39),0,"Effect output arrived before latency");near(audio.getSample(2,40),0.25f,"Effect output timing incorrect");near(audio.getSample(4,39),0,"Dry instrument was not latency aligned");near(audio.getSample(4,40),0.5f,"Dry instrument latency incorrect");
    // A new MIDI event in the second sub-block must retain its sample offset.
    audio=setup.hit(192,80);near(audio.getSample(2,111),0,"Chunked processing moved MIDI earlier");near(audio.getSample(2,112),0.25f,"Chunked processing lost MIDI offset");
    // A delayed effect keeps rendering after all source voices have ended.
    setup.processor.getSampleEngine().requestLoad(setup.kit,setup.a,sampleFile("short",0.25f,8));setup.settle();
    auto c=setup.instrument(setup.b).controls;c.mute=true;library.setControls(setup.kit,setup.b,c);setup.settle();audio=setup.hit();check(audio.getMagnitude(2,32,8)>0.001f,"Effect tail stopped at the sample end");
    // Missing plugins retain their place and state, and pass audio through.
    auto missing=description;missing.fileOrIdentifier=juce::File::getCurrentWorkingDirectory().getChildFile("missing-effect.vst3").getFullPathName();
    rack.remove(setup.kit,setup.a,first.id);rack.remove(setup.kit,setup.a,second.id);auto unavailable=rack.add(setup.kit,setup.a,missing);check(unavailable.ok && rack.status(unavailable.id).contains("Missing"),"Missing effect not retained visibly");
    setup.processor.getSampleEngine().requestLoad(setup.kit,setup.a,sampleFile("kick",0.25f));setup.settle();audio=setup.hit();near(audio.getSample(2,0),0.25f,"Missing effect did not pass through");
    auto malformed=library.toXml();auto* instrument=malformed->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument");instrument->getChildByName("Effect")->getChildByName("State")->deleteAllTextElements();instrument->getChildByName("Effect")->getChildByName("State")->addTextElement("invalid");check(!library.restore(*malformed),"Corrupt plugin state accepted");
    rack.remove(setup.kit,setup.a,unavailable.id);setup.settle();
    // File drops append real VST3 effects without an embedded plugin browser.
    DrumAudioProcessorEditor editor(setup.processor);editor.setSize(1280,900);editor.addToDesktop(0);editor.setVisible(true);pump();click(editor,"open-sound-editor");click(editor,"edit-tab-7");
    auto* chain=dynamic_cast<ChainEditor*>(find(editor,"plugin-chain"));check(chain && chain->isVisible(),"Chain tab not visible");
    juce::StringArray dropped{effectPath};check(chain->isInterestedInFileDrag(dropped),"VST3 drop rejected");chain->filesDropped(dropped,700,60);pump(100);setup.settle();pump();
    auto uiSlots=setup.instrument(setup.a).effects;check(uiSlots.size()==1,"File drop failed to insert effect");
    check(find(editor,"chain-browser")==nullptr && find(editor,"chain-search")==nullptr,"Removed plugin list is still present");
    auto* flow=find(editor,"chain-flow");check(flow && flow->getX()==0,"Chain did not reclaim browser space");
    chain->filesDropped(dropped,700,60);pump(100);setup.settle();pump();check(setup.instrument(setup.a).effects.size()==2,"Second VST3 drop failed");
    auto dragged=setup.instrument(setup.a).effects[0].id;
    auto limited=setup.instrument(setup.a).effects;
    while(limited.size()<drum::maxEffects){auto slot=limited.front();slot.id=juce::Uuid().toString();limited.push_back(slot);}
    check(library.setEffects(setup.kit,setup.a,limited).ok,"Sixteen effect slots rejected");
    auto extra=limited.front();extra.id=juce::Uuid().toString();limited.push_back(extra);
    check(!library.setEffects(setup.kit,setup.a,limited).ok,"Seventeenth effect slot accepted");
    limited.resize(2);check(library.setEffects(setup.kit,setup.a,limited).ok,"Effect limit test could not restore chain");
    chain->itemDropped(juce::DragAndDropTarget::SourceDetails("slot:"+dragged,chain,{1100,60}));pump();
    check(setup.instrument(setup.a).effects[1].id==dragged,"Dragging a card did not reorder the chain");
    click(editor,"chain-bypass-0");pump();check(setup.instrument(setup.a).effects[0].bypassed,"Card bypass did not update chain");
    click(editor,"chain-bypass-0");pump();
    capture(editor,"drum-chain.png");editor.setSize(800,600);capture(editor,"drum-chain-compact.png");editor.setSize(1280,900);
    check(!chain->isInterestedInFileDrag({"C:\\example.adv"}),"Live preset accepted as a plugin");
    click(editor,"chain-edit-0");pump();click(editor,"chain-remove-0");pump(); // Close editor before releasing its processor.
    check(setup.instrument(setup.a).effects.size()==1,"Remove card button did not remove the effect");
    // Persisted chains also work when Drum itself is hosted as a compiled VST3.
    setup.processor.getStateInformation(saved);juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> drums;format.findAllTypesForFile(drums,juce::File(drumPath).getFullPathName());check(drums.size()==1,"Drum VST3 missing");
    juce::String error;auto plugin=format.createInstanceFromDescription(*drums[0],48000,64,error);check(plugin!=nullptr,"Nested Drum host failed");
    juce::XmlElement envelope("VST3PluginState");envelope.createNewChildElement("IComponent")->addTextElement(saved.toBase64Encoding());juce::MemoryBlock hostState;juce::AudioProcessor::copyXmlToBinary(envelope,hostState);
    plugin->setStateInformation(hostState.getData(),(int)hostState.getSize());check(plugin->setBusesLayout(setup.processor.getBusesLayout()),"Nested host outputs failed");plugin->prepareToPlay(48000,64);
    bool played=false;for(int attempt=0;attempt<100;++attempt){pump(20);midi.clear();midi.addEvent(juce::MidiMessage::allSoundOff(1),0);midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);plugin->processBlock(recalledAudio,midi);if(std::abs(recalledAudio.getSample(2,0)-0.125f)<0.0001f){played=true;break;}}
    check(played,"Compiled Drum VST3 did not process its nested VST3 chain");plugin->releaseResources();
    std::cout<<"External VST3 chains: order, bypass, per-instrument isolation, sends, latency, tails, state, missing plugins, drag/drop and nested hosting passed\n";
}
// Optional compatibility check against a locally installed effect. It never
// edits that effect's presets, settings files or the user's DAW project.
void runChainSmoke(const juce::String& path)
{
    Setup setup;auto& rack=setup.processor.getEffects();auto found=rack.discover(juce::File(path));
    check(!found.isEmpty(),"No audio effect found for compatibility check");
    auto slot=rack.add(setup.kit,setup.a,*found[0]);setup.settle();
    check(slot.ok && rack.instance(slot.id),"Installed effect could not be hosted");
    auto audio=setup.hit(4096);for(int c=0;c<audio.getNumChannels();++c)for(int n=0;n<audio.getNumSamples();++n)
        check(std::isfinite(audio.getSample(c,n)),"Installed effect produced nonfinite audio");
    check(audio.getMagnitude(2,0,audio.getNumSamples())>0,"Installed effect produced no audio");
    juce::MemoryBlock state;setup.processor.getStateInformation(state);
    setup.processor.setStateInformation(state.getData(),(int)state.getSize());setup.settle();
    check(rack.instance(slot.id),"Installed effect did not recall");
    audio=setup.hit(4096);check(audio.getMagnitude(2,0,audio.getNumSamples())>0,"Recalled effect produced no audio");
    auto& audition=setup.processor.getSampleEngine();audition.setAuditionLoop(21);
    rack.bypass(setup.kit,setup.a,slot.id,true);
    setup.processor.releaseResources();setup.processor.prepareToPlay(48000,64);setup.settle();
    check(audition.auditionLoopRunning(),"Installed effect bypass/reprepare stopped audition");
    juce::MidiBuffer noMidi;juce::AudioBuffer<float> repeatAudio(8,24001);setup.processor.processBlock(repeatAudio,noMidi);
    check(repeatAudio.getMagnitude(2,0,repeatAudio.getNumSamples())>0,"Bypassed audition is silent");
    audition.pauseAudition();rack.bypass(setup.kit,setup.a,slot.id,false);setup.settle();
    DrumAudioProcessorEditor editor(setup.processor);editor.setSize(1280,900);editor.addToDesktop(0);editor.setVisible(true);pump();
    click(editor,"open-sound-editor");click(editor,"edit-tab-7");
    auto* chain=dynamic_cast<ChainEditor*>(find(editor,"plugin-chain"));check(chain!=nullptr,"Compatibility chain UI missing");
    chain->filesDropped({path},700,60);pump(100);setup.settle();pump();
    check(setup.instrument(setup.a).effects.size()==2,"Second installed effect drop failed");
    click(editor,"chain-remove-0");pump();setup.settle();pump();
    check(setup.instrument(setup.a).effects.size()==1,"Installed effect Remove button failed");
    click(editor,"chain-remove-0");pump();setup.settle();pump();
    check(setup.instrument(setup.a).effects.empty(),"Last installed effect Remove button failed");
    chain->filesDropped({path},700,60);pump(100);setup.settle();pump();
    check(setup.instrument(setup.a).effects.size()==1,"Installed effect could not be added again after removal");
    std::cout<<found[0]->name<<": loaded, processed, recalled, added twice, removed through cards and added again successfully\n";
}
