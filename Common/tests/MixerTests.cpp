#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include "PluginProcessor.h"
#include "platform/HostFloat.h"
#include "PluginEditor.h"
#include "MixerPage.h"
#include "SoundEditor.h"
#include "MixerSendBar.h"
#include "MixerReadouts.h"
#include "MeterPeakState.h"
#include "SendRouteDisplay.h"
#include "BuiltinEffects.h"
#if JUCE_MAC
#include "../../Mac/src/GroupEditor.h"
#endif
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void near(float actual,float expected,const char* message){if(std::abs(actual-expected)>.0002f){std::cerr<<actual<<" expected "<<expected<<'\n';throw std::runtime_error(message);}}
juce::File source(float value,const char* name){auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(file),48000,2,24,{},0));check(writer!=nullptr,"Mixer fixture writer");juce::AudioBuffer<float> audio(2,48000);for(int c=0;c<2;++c)juce::FloatVectorOperations::fill(audio.getWritePointer(c),value,48000);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Mixer fixture audio");return file;}
struct Setup {
    DrumAudioProcessor p;juce::String kit,group,other,a,b,c;
    Setup(){auto state=p.getLibrary().snapshot();kit=state.activeKit;group=state.kits[0].groups[0].id;auto& lib=p.getLibrary();lib.rename(group,"Hi Hats");other=lib.addGroup(kit).id;lib.rename(other,"Percussion");a=lib.addInstrument(kit,group).id;b=lib.addInstrument(kit,group).id;c=lib.addInstrument(kit,other).id;lib.rename(a,"Open hi-hat");lib.rename(b,"Closed hi-hat");lib.rename(c,"Shaker");
        auto layout=p.getBusesLayout();layout.outputBuses.set(1,juce::AudioChannelSet::stereo());layout.outputBuses.set(2,juce::AudioChannelSet::stereo());check(p.setBusesLayout(layout),"Mixer output layout");p.prepareToPlay(48000,128);
        p.getSampleEngine().requestLoad(kit,a,source(.1f,"mixer-open.wav"));p.getSampleEngine().requestLoad(kit,b,source(.2f,"mixer-closed.wav"));p.getSampleEngine().requestLoad(kit,c,source(.05f,"mixer-shaker.wav"));settle();}
    void settle(){p.getEffects().sync();check(p.getSampleEngine().waitForLoading(10000),"Mixer sample loading");}
    juce::AudioBuffer<float> hit(){settle();p.getSampleEngine().stop();juce::MidiBuffer midi;juce::AudioBuffer<float> audio(p.getTotalNumOutputChannels(),128);p.processBlock(audio,midi);auto state=p.getLibrary().snapshot();for(const auto& g:state.kits[0].groups)for(const auto& i:g.instruments)midi.addEvent(juce::MidiMessage::noteOn(1,i.note,(juce::uint8)127),0);p.processBlock(audio,midi);midi.clear();for(int n=0;n<20;++n)p.processBlock(audio,midi);return audio;}
};
juce::Component* find(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
juce::Component* showing(juce::Component& root,const juce::String& id){if(root.getComponentID()==id && root.isShowing())return &root;for(auto* c:root.getChildren())if(auto* result=showing(*c,id))return result;return nullptr;}
void capture(juce::Component& c,const char* name){juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);check(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),out),"Mixer screenshot");}
}
void runMixerTests(){
    {
        drum::SoundParameters sound;std::array<drum::ReturnRouting,6> routes{};
        sound.sendD=0;
        check(drum::displayedSendRoutes(sound,true,routes)[3],"Connected Send D is not highlighted");
        sound.sendD=drum::sendFloor;
        check(!drum::displayedSendRoutes(sound,true,routes)[3],"Disconnected Send D remains highlighted");
        routes[0].sends[3]=0;sound.sendA=0;
        check(drum::displayedSendRoutes(sound,true,routes)[3],"Indirect A to D route is not highlighted");
        routes[0].mixer.mute=true;
        check(!drum::displayedSendRoutes(sound,true,routes)[3],"Muted upstream return lights Send D");
        routes[0].mixer.mute=false;routes[3].mixer.volumeDb=-70;
        check(!drum::displayedSendRoutes(sound,true,routes)[3],"Silent Send D remains highlighted");
        routes[3].mixer.volumeDb=0;
        check(!drum::displayedSendRoutes(sound,false,routes)[3],"Inaudible instrument lights Send D");
        sound.sendA=drum::sendFloor;
        check(!drum::displayedSendRoutes(sound,true,routes)[3],"Unused return route lights Send D");
    }

#if JUCE_MAC
    {
        DrumAudioProcessor source(true);source.newKit();auto state=source.getLibrary().snapshot();auto group=state.kits[0].groups[0].id;
        source.editorPresentation.hosted={120,180,1000,800,true};source.editorPresentation.floating={210,250,950,760,true};source.editorPresentation.mixer=true;source.editorPresentation.group=group;
        juce::MemoryBlock saved;source.getStateInformation(saved);DrumAudioProcessor recalled(true);recalled.setStateInformation(saved.getData(),(int)saved.getSize());
        check(recalled.editorPresentation.hosted.width==1000 && recalled.editorPresentation.hosted.x==120 && recalled.editorPresentation.floating.height==760 && recalled.editorPresentation.group==group,"Live state lost independent hosted/floating mixer placement");
        std::unique_ptr<juce::AudioProcessorEditor> editor(recalled.createEditor());auto* mixer=dynamic_cast<DrumMixerPage*>(find(*editor,"drum-mixer"));
        check(editor->getWidth()==1000 && editor->getHeight()==800 && mixer && mixer->isVisible() && mixer->selectedGroup()==group,"Recreated editor did not reopen the remembered mixer view and size");
        recalled.editorBeingDeleted(editor.get());
    }
    {class Window final:public juce::DocumentWindow{public:Window(const char* title):DocumentWindow(title,juce::Colours::black,closeButton){setUsingNativeTitleBar(true);setSize(400,300);}void closeButtonPressed()override{setVisible(false);}};Window owner("Drum test owner"),child("Chain test child");owner.setAlwaysOnTop(true);owner.setVisible(true);djl::HostFloat::showAuxiliaryWindow(child,&owner);auto* parent=[static_cast<NSView*>(owner.getPeer()->getNativeHandle()) window];auto* auxiliary=[static_cast<NSView*>(child.getPeer()->getNativeHandle()) window];check([auxiliary parentWindow]==parent,"Chain window is not attached above its owning Drum window");check([auxiliary level]==[parent level],"Chain window is below the floating Drum window level");auto* ordered=[NSApp orderedWindows];check([ordered indexOfObject:auxiliary]<[ordered indexOfObject:parent],"Chain window did not open in front of Drum");}
#endif

#if JUCE_MAC || JUCE_WINDOWS
    auto nativeVisible=[](juce::ComponentPeer* peer){
#if JUCE_MAC
        return (bool)[[static_cast<NSView*>(peer->getNativeHandle()) window] isVisible];
#else
        return IsWindowVisible(static_cast<HWND>(peer->getNativeHandle()))!=0;
#endif
    };
    {DrumAudioProcessor processor(true);processor.newKit();std::unique_ptr<juce::AudioProcessorEditor> hosted(processor.createEditor());
        juce::DocumentWindow shell("Simulated DAW editor",juce::Colour(0xff10151f),juce::DocumentWindow::closeButton);shell.setUsingNativeTitleBar(true);shell.setContentNonOwned(hosted.get(),true);shell.setVisible(true);auto* native=shell.getPeer();check(native!=nullptr,"Native host simulation has no peer");native->setVisible(false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(120);check(!nativeVisible(native),"Non-float native window was raised after the host hid it");
        processor.setFloatEnabled(true);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);auto* floating=processor.getFloatingWindow();check(floating && floating->isVisible() && !nativeVisible(native),"Float depends on the hosted native window visibility");native->setVisible(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(70);check(nativeVisible(floating->getPeer()),"Native Float was hidden with its host shell");processor.setFloatEnabled(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(50);shell.clearContentComponent();processor.editorBeingDeleted(hosted.get());}
#endif
    using namespace drum;
#if JUCE_MAC
    {
        Setup minimum;auto& model=minimum.p.getLibrary();minimum.hit();
        MixerControls down;down.volumeDb=-70;
        check(model.setGroupMixer(minimum.kit,minimum.group,down).ok,"Group minimum fixture");
        auto output=minimum.hit();near(output.getSample(0,127),.05f,"Group minimum leaked member dry audio");
        auto state=model.snapshot();auto controls=findInstrument(state.kits[0],minimum.a)->controls;controls.sound.sendA=0;
        model.setControls(minimum.kit,minimum.a,controls);
        output=minimum.hit();near(output.getSample(0,127),.05f,"Group minimum leaked its instrument return to Main");
        state=model.snapshot();auto otherControls=findInstrument(state.kits[0],minimum.c)->controls;otherControls.sound.sendA=0;
        model.setControls(minimum.kit,minimum.c,otherControls);
        output=minimum.hit();near(output.getSample(0,127),.10f,"Group minimum silenced another group's shared send");
        auto shaping=std::make_shared<SendProcessing>();shaping->sound=otherControls.sound;otherControls.sendProcessing[0]=shaping;
        model.setControls(minimum.kit,minimum.c,otherControls);
        output=minimum.hit();near(output.getSample(0,127),.10f,"Independent send shaping used another group's fader");
        auto ownShaping=std::make_shared<SendProcessing>();ownShaping->sound=controls.sound;controls.sendProcessing[0]=ownShaping;
        model.setControls(minimum.kit,minimum.a,controls);
        output=minimum.hit();near(output.getSample(0,127),.10f,"Group minimum leaked an independently shaped send");
        const auto slotId=juce::Uuid().toString();
        model.setEffects(minimum.kit,"return-0",{{slotId,builtinDescription(BuiltinKind::utility).createXml()->toString(),{},false}});minimum.settle();
        auto* shared=dynamic_cast<BuiltinEffect*>(minimum.p.getEffects().instance(slotId));check(shared!=nullptr,"Shared editor fixture");
        const auto cloneId="group-shared:"+minimum.other+":"+slotId;auto* clone=minimum.p.getEffects().instance(cloneId);check(clone!=nullptr,"Shared send processing was not instantiated for the active group");
        shared->controls[0]->setValueNotifyingHost(shared->controls[0]->convertTo0to1(-6.0206f));minimum.p.getEffects().capture();
        output=minimum.hit();near(output.getSample(0,127),.075f,"Shared send editor did not update the per-group processing");
        check(minimum.p.getEffects().instance(cloneId)==clone,"Live shared-send edit recreated the effect instance");
        shared->controls[0]->setValueNotifyingHost(shared->controls[0]->convertTo0to1(0));minimum.p.getEffects().capture();
        down.volumeDb=-6.020599913;model.setGroupMixer(minimum.kit,minimum.group,down);
        output=minimum.hit();near(output.getSample(0,127),.30f,"Group fader did not scale dry and instrument sends equally");
        model.setSendRoute(minimum.kit,"return-0",1,0);
        output=minimum.hit();near(output.getSample(0,127),.40f,"Cascaded send applied the group fader more than once");
        down.volumeDb=-70;model.setGroupMixer(minimum.kit,minimum.group,down);
        output=minimum.hit();near(output.getSample(0,127),.15f,"Cascaded sends leaked at group minimum or cut another group");
        model.setSendRoute(minimum.kit,"return-0",1,sendFloor);
        otherControls.sound.sendA=sendFloor;model.setControls(minimum.kit,minimum.c,otherControls);
        check(model.setSendDestination(minimum.kit,"return-0",false,minimum.group).ok,"Minimum return-to-group fixture");
        output=minimum.hit();near(output.getSample(0,127),.05f,"Group minimum leaked its routed instrument return");
    }
    {
        Setup tails;auto& model=tails.p.getLibrary();auto state=model.snapshot();
        auto controls=findInstrument(state.kits[0],tails.a)->controls;controls.sound.sendA=0;model.setControls(tails.kit,tails.a,controls);
        MixerControls group;group.output=1;model.setGroupMixer(tails.kit,tails.group,group);
        MixerControls other;other.volumeDb=-70;model.setGroupMixer(tails.kit,tails.other,other);
        BuiltinEffect delay(BuiltinKind::tempoDelay);
        for(auto [index,value]:{std::pair{0,20.f},{3,70.f},{7,100.f}})delay.controls[(size_t)index]->setValueNotifyingHost(delay.controls[(size_t)index]->convertTo0to1(value));
        juce::MemoryBlock saved;delay.getStateInformation(saved);
        auto effect=[&]{return std::vector<EffectSlot>{{juce::Uuid().toString(),builtinDescription(BuiltinKind::tempoDelay).createXml()->toString(),saved.toBase64Encoding(),false}};};
        model.setEffects(tails.kit,"return-0",effect());
        model.setGroupSend(tails.kit,tails.group,3,0);model.setEffects(tails.kit,groupReturnId(tails.group,3),effect());
        auto output=tails.hit();check(output.getMagnitude(0,128)>.001f,"Wet-only group tail fixture is silent");
        group.volumeDb=-70;model.setGroupMixer(tails.kit,tails.group,group);tails.settle();
        juce::MidiBuffer empty;for(int block=0;block<20;++block)tails.p.processBlock(output,empty);
        near(output.getMagnitude(0,128),0,"Group minimum leaked existing instrument/group delay histories");
        group.volumeDb=0;model.setGroupMixer(tails.kit,tails.group,group);tails.settle();
        for(int block=0;block<20;++block)tails.p.processBlock(output,empty);
        check(output.getMagnitude(0,128)>.001f,"Group fader stopped processing rather than controlling existing wet audio");
    }

#endif
    Setup s;auto& lib=s.p.getLibrary();auto audio=s.hit();near(audio.getSample(0,127),.35f,"Unity mixer changed legacy output");// Linked instances must treat mixer edits as live changes, not project recalls.
    {DrumAudioProcessor lead(true),peer(true);lead.newKit();check(lead.getLibrary().restore(*lib.toXml()),"Linked audio fixture");auto path=juce::File::getCurrentWorkingDirectory().getChildFile("linked-audio-"+juce::Uuid().toString()+"/Mix.drumkit");check(lead.saveDocument(path).ok && peer.openDocument(path).ok,"Linked audio documents");peer.prepareToPlay(48000,128);check(peer.getSampleEngine().waitForLoading(10000),"Linked audio loading");peer.getEffects().sync();juce::AudioBuffer<float> sounding(peer.getTotalNumOutputChannels(),128);juce::MidiBuffer notes;notes.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);peer.processBlock(sounding,notes);notes.clear();for(int n=0;n<12;++n)peer.processBlock(sounding,notes);const auto epoch=peer.getLibrary().activeKitEpoch();auto fixture=lead.getLibrary().snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.pan=.5;lead.getLibrary().setControls(s.kit,s.a,values);lead.syncLinkedKit();peer.syncLinkedKit();check(peer.getLibrary().activeKitEpoch()==epoch,"Linked mixer edit recalled the kit and killed active audio");peer.processBlock(sounding,notes);check(peer.getSampleEngine().playbackPosition(21).active && sounding.getMagnitude(0,128)>.01f,"Linked mixer edit stopped the sounding voice");
        for(int kind=0;kind<3;++kind){auto live=lead.getLibrary().snapshot();auto controls=findInstrument(live.kits[0],s.a)->controls;if(kind==0)controls.volumeDb=-12;if(kind==1)controls.sound.sendA=-6;if(kind==2)controls.pan=-.5;lead.getLibrary().setControls(s.kit,s.a,controls);lead.syncLinkedKit();peer.syncLinkedKit();check(peer.getLibrary().activeKitEpoch()==epoch,"Linked level/send edit reset playback");check(peer.getSampleEngine().waitForLoading(10000),"Linked live controls timed out");peer.getEffects().sync();float previous=sounding.getSample(0,127),step=0;for(int block=0;block<12;++block){peer.processBlock(sounding,notes);for(int frame=0;frame<128;++frame){auto value=sounding.getSample(0,frame);step=juce::jmax(step,std::abs(value-previous));previous=value;}}if(step>=.001f)std::cerr<<"Linked step kind="<<kind<<" step="<<step<<" previous="<<previous<<"\n";check(step<.001f,"Linked live mix produced an audio discontinuity");check(peer.getSampleEngine().playbackPosition(21).active,"Linked level/send edit killed voice");}
        auto replacement=lead.getLibrary().snapshot();auto different=findInstrument(replacement.kits[0],s.b)->samplePath;check(lead.getLibrary().assignSample(s.kit,s.a,different).ok,"Linked replacement fixture");lead.syncLinkedKit();peer.syncLinkedKit();check(peer.getSampleEngine().waitForLoading(10000),"Linked replacement load");peer.processBlock(sounding,notes);
#if JUCE_MAC
        check(peer.getSampleEngine().playbackPosition(21).active,"Another track's Sample tab stopped this track's voice");
        check(peer.getLibrary().assignSample(s.kit,s.a,different).ok && peer.getSampleEngine().waitForLoading(10000),"Local replacement load");peer.processBlock(sounding,notes);
#endif
        check(!peer.getSampleEngine().playbackPosition(21).active,"A genuinely changed recording incorrectly reused the old voice");}
    // A sustained constant sample isolates discontinuities introduced by live mixing.
    auto slew=[&](auto edit,const char* message){auto last=s.hit();float previous=last.getSample(0,127),largest=0;edit();s.settle();juce::MidiBuffer empty;for(int block=0;block<20;++block){s.p.processBlock(last,empty);for(int frame=0;frame<128;++frame){float value=last.getSample(0,frame);largest=juce::jmax(largest,std::abs(value-previous));previous=value;}}check(largest<.001f,message);};
    MixerControls panGroup;panGroup.pan=1;slew([&]{lib.setGroupMixer(s.kit,s.group,panGroup);},"Group pan introduced an audio step");panGroup.pan=0;lib.setGroupMixer(s.kit,s.group,panGroup);
    slew([&]{auto fixture=lib.snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.sound.sendA=0;lib.setControls(s.kit,s.a,values);},"Send gain introduced an audio step");{auto fixture=lib.snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.sound.sendA=sendFloor;lib.setControls(s.kit,s.a,values);}
    slew([&]{auto fixture=lib.snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.pan=1;lib.setControls(s.kit,s.a,values);},"Instrument pan introduced an audio step");
    slew([&]{auto fixture=lib.snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.pan=0;values.volumeDb=-12;lib.setControls(s.kit,s.a,values);},"Instrument level introduced an audio step");
    {auto fixture=lib.snapshot();auto values=findInstrument(fixture.kits[0],s.a)->controls;values.volumeDb=0;lib.setControls(s.kit,s.a,values);}
    MixerControls fadeGroup;fadeGroup.volumeDb=-12;slew([&]{lib.setGroupMixer(s.kit,s.group,fadeGroup);},"Group level introduced an audio step");lib.setGroupMixer(s.kit,s.group,MixerControls{});
    slew([&]{lib.setGroupSend(s.kit,s.group,0,0);},"Group send introduced an audio step");lib.setGroupSend(s.kit,s.group,0,sendFloor);
    MixerControls group;group.volumeDb=-6.020599913;check(lib.setGroupMixer(s.kit,s.group,group).ok,"Group level");audio=s.hit();near(audio.getSample(0,127),.2f,"Group gain not applied");
    auto state=lib.snapshot();auto controls=findInstrument(state.kits[0],s.a)->controls;controls.volumeDb=-6.020599913;lib.setControls(s.kit,s.a,controls);audio=s.hit();near(audio.getSample(0,127),.175f,"Instrument and group gains do not multiply");
    group.mute=true;lib.setGroupMixer(s.kit,s.group,group);audio=s.hit();near(audio.getSample(0,127),.05f,"Group mute");group.mute=false;group.solo=true;lib.setGroupMixer(s.kit,s.group,group);audio=s.hit();near(audio.getSample(0,127),.125f,"Group solo");group.solo=false;group.pan=-1;lib.setGroupMixer(s.kit,s.group,group);audio=s.hit();near(audio.getSample(1,127),.05f,"Group pan");group.pan=0;group.output=1;lib.setGroupMixer(s.kit,s.group,group);audio=s.hit();near(audio.getSample(0,127),.05f,"Routed group also reached Main");near(audio.getSample(2,127),.125f,"Group separate output");controls.sound.output=2;lib.setControls(s.kit,s.a,controls);audio=s.hit();near(audio.getSample(2,127),.1f,"Instrument output did not override group");near(audio.getSample(4,127),.025f,"Instrument separate output level");
    group.output=7;lib.setGroupMixer(s.kit,s.group,group);audio=s.hit();near(audio.getSample(0,127),.15f,"Inactive group output did not fall back to Main");group.output=1;lib.setGroupMixer(s.kit,s.group,group);
    controls.sound.sendA=0;lib.setControls(s.kit,s.a,controls);MixerControls ret;ret.volumeDb=-6.020599913;lib.setReturnMixer(s.kit,0,ret);audio=s.hit();near(audio.getSample(0,127),.0625f,"Return output fader or post-group send gain");controls.sound.sendA=6;check(lib.setControls(s.kit,s.a,controls).ok,"+6 dB send rejected");audio=s.hit();near(audio.getSample(0,127),.05f+.0125f*sendGain(6),"+6 dB send gain not applied");controls.sound.sendA=0;lib.setControls(s.kit,s.a,controls);check(lib.setReturnLevel(s.kit,1,2,6).ok,"+6 dB return send rejected");
    ret.mute=true;lib.setReturnMixer(s.kit,0,ret);audio=s.hit();near(audio.getSample(0,127),.05f,"Return mute");ret.mute=false;ret.solo=true;lib.setReturnMixer(s.kit,0,ret);audio=s.hit();near(audio.getSample(0,127),.0125f,"Return solo level");near(audio.getSample(2,127),0,"Return solo left group dry audible");near(audio.getSample(4,127),0,"Return solo left instrument dry audible");
    check(lib.setGroupSend(s.kit,s.group,0,-6.020599913).ok,"Group send rejected");ret.solo=false;lib.setReturnMixer(s.kit,0,ret);audio=s.hit();
#if JUCE_MAC
    near(audio.getSample(0,127),.1125f,"Independent group send did not reach Main");near(audio.getSample(2,127),.1f,"Group return leaked into the group dry output");
#else
    near(audio.getSample(0,127),.09375f,"Group send did not add its summed signal to instrument sends");
#endif
lib.setGroupSend(s.kit,s.group,0,sendFloor);ret.solo=true;lib.setReturnMixer(s.kit,0,ret);
    check(lib.setGroupSend(s.kit,s.group,1,-18).ok,"Group send persistence fixture");
    auto xml=lib.toXml();KitLibrary restored;check(restored.restore(*xml),"Mixer persistence rejected");check(restored.snapshot().kits[0].groups[0].mixer==group && restored.snapshot().kits[0].returnRouting[0].mixer==ret,"Mixer settings not recalled");near((float)restored.snapshot().kits[0].groups[0].sends[1],-18,"Group send lost on recall");
    auto legacy=std::make_unique<juce::XmlElement>(*xml);legacy->setAttribute("schemaVersion",22);KitLibrary old;check(old.restore(*legacy),"Legacy kit rejected");check(old.snapshot().kits[0].groups[0].mixer==MixerControls{} && old.snapshot().kits[0].returnRouting[0].mixer==MixerControls{},"Legacy kit did not default to unity");
    auto bad=std::make_unique<juce::XmlElement>(*xml);bad->getChildByName("Kit")->getChildByName("Group")->setAttribute("mixVolume",1000);check(!restored.restore(*bad),"Invalid group mix accepted");ret.output=99;check(!lib.setReturnMixer(s.kit,0,ret).ok,"Invalid output accepted");
    ret={};lib.setReturnMixer(s.kit,0,ret);group={};lib.setGroupMixer(s.kit,s.group,group);s.hit();
    DrumMixerPage page(s.p);page.setSize(1232,764);page.addToDesktop(0);page.open();juce::Timer::callPendingTimersSynchronously();capture(page,"drum-mixer-groups.png");auto* button=dynamic_cast<juce::Button*>(find(page,"mixer-name-"+s.group));check(button!=nullptr,"Group mixer strip missing");button->onClick();check(find(page,"mixer-level-"+s.a)!=nullptr,"Overview group button did not open its instruments");auto* fader=dynamic_cast<juce::Slider*>(find(page,"mixer-level-"+s.a));fader->setValue(-9,juce::sendNotificationSync);auto current=lib.snapshot();near((float)findInstrument(current.kits[0],s.a)->controls.volumeDb,-9,"Mixer fader not wired");juce::Timer::callPendingTimersSynchronously();capture(page,"drum-mixer-instruments.png");
    auto* level=dynamic_cast<MixerLevelReading*>(find(page,"mixer-level-readout-"+s.a));check(level!=nullptr,"Top level readout missing");auto t=juce::Time::getCurrentTime();juce::MouseEvent doubleClick(juce::Desktop::getInstance().getMainMouseSource(),{},juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,level,level,t,{},t,2,false);level->mouseDoubleClick(doubleClick);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);near((float)fader->getValue(),0,"Double-click level readout did not reset to 0 dB");fader->setValue(-9,juce::sendNotificationSync);level->showEditor();level->mouseDoubleClick(doubleClick);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);near((float)fader->getValue(),0,"Editing level double-click did not reset");fader->setValue(-9,juce::sendNotificationSync);
    auto held=s.p.getEffects().mixerMaximum(0);s.p.getSampleEngine().stop();juce::AudioBuffer<float> silence(s.p.getTotalNumOutputChannels(),128);juce::MidiBuffer none;for(int n=0;n<500;++n)s.p.processBlock(silence,none);near(s.p.getEffects().mixerMaximum(0)[0],held[0],"Maximum peak reading decayed without reset");auto* peak=dynamic_cast<juce::Button*>(find(page,"mixer-peak-readout-"+s.a));check(peak!=nullptr,"Peak reset control missing");peak->onClick();near(s.p.getEffects().mixerMaximum(0)[0],0,"Clicking peak reading did not reset it");s.hit();
    auto* groupSend=dynamic_cast<juce::Slider*>(find(page,"mixer-send-"+s.group+"-2"));check(groupSend!=nullptr,"Group strip lacks sends");groupSend->setValue(-12,juce::sendNotificationSync);near((float)lib.snapshot().kits[0].groups[0].sends[2],-12,"Group send bar is not wired");
    auto* balance=dynamic_cast<juce::Slider*>(find(page,"mixer-pan-"+s.a));check(balance && balance->getSliderStyle()==juce::Slider::LinearBar,"Pan is not bar-only");check(level->getY()==peak->getY(),"Level and peak readings are not aligned");
    auto* send=dynamic_cast<juce::Slider*>(find(page,"mixer-send-"+s.a+"-0"));check(send!=nullptr,"Local send knob missing");send->setValue(-18,juce::sendNotificationSync);current=lib.snapshot();near((float)findInstrument(current.kits[0],s.a)->controls.sound.sendA,-18,"Local mixer send not wired");
    check(send->getTextFromValue(sendFloor).contains(juce::String::charToString(0x221e)) && send->getMaximum()==6,"Mixer send does not support infinity to +6 dB");check(send->getSliderStyle()==juce::Slider::LinearBar,"Mixer send is not a bar");auto* third=find(page,"mixer-send-"+s.a+"-2");auto* fourth=find(page,"mixer-send-"+s.a+"-3");auto* sixth=find(page,"mixer-send-"+s.a+"-5");check(send->getY()==third->getY() && fourth->getY()==sixth->getY() && fourth->getY()>send->getY(),"Send bars are not two rows of three");check(fader->getHeight()>220,"Send layout did not recover fader height");
    SoundEditor edit(s.p);edit.select(current.activeKit,findInstrument(current.kits[0],s.a));auto* routingSend=dynamic_cast<juce::Slider*>(find(edit,"edit-sendA"));check(routingSend!=nullptr,"Outputs & Sends control missing");near((float)routingSend->getValue(),-18,"Mixer and Outputs & Sends do not share the send");routingSend->setValue(-24,juce::sendNotificationSync);juce::MessageManager::getInstance()->runDispatchLoopUntil(60);juce::Timer::callPendingTimersSynchronously();near((float)send->getValue(),-24,"Outputs & Sends changes did not update Mixer");capture(page,"drum-mixer-instruments.png");
    auto* bar=dynamic_cast<MixerSendBar*>(send);check(bar!=nullptr,"Live-style send behavior missing");auto now=juce::Time::getCurrentTime();auto gesture=[&](float y,bool fine){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{10,y},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|(fine?juce::ModifierKeys::shiftModifier:0)),1,0,0,0,0,bar,bar,now,{10,10},now,1,false);};
    bar->mouseDown(gesture(10,false));bar->mouseDrag(gesture(0,false));near((float)bar->getValue(),-20,"Vertical send drag incorrect");bar->setValue(-24);bar->mouseDown(gesture(10,true));bar->mouseDrag(gesture(0,true));near((float)bar->getValue(),-23.6f,"Fine send drag incorrect");bar->mouseDoubleClick(gesture(0,false));near((float)bar->getValue(),(float)sendFloor,"Send reset does not switch off");
    bar->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));auto* entry=dynamic_cast<juce::TextEditor*>(find(*bar,"send-value-entry"));check(entry && entry->isVisible(),"Send value typing missing");entry->setText("+6",false);entry->onReturnKey();near((float)bar->getValue(),6,"Typed +6 dB not accepted");current=lib.snapshot();near((float)findInstrument(current.kits[0],s.a)->controls.sound.sendA,6,"Typed send boost did not reach audio model");
    capture(page,"drum-mixer-instruments.png");
    juce::Slider meter;meter.setRange(-60,12);meter.getProperties().set("mixer-fader",true);meter.getProperties().set("peak-l",2.f);meter.getProperties().set("peak-r",2.f);DrumMixerLook meterLook;juce::Image leds(juce::Image::ARGB,112,310,true);juce::Graphics mg(leds);meterLook.drawLinearSlider(mg,0,0,112,310,60,0,0,juce::Slider::LinearVertical,meter);check(leds.getPixelAt(40,35)==juce::Colour(0xffed5d61) && leds.getPixelAt(40,65)==juce::Colour(0xfff1cf59) && leds.getPixelAt(40,105)==juce::Colour(0xff54cf78),"Meter LEDs are not red/yellow/green by dB band");
    auto* selector=dynamic_cast<juce::ComboBox*>(find(page,"mixer-view"));selector->setSelectedId(2,juce::sendNotificationSync);auto* returnFader=dynamic_cast<juce::Slider*>(find(page,"mixer-level-return-0"));check(returnFader!=nullptr,"Return mixer fader missing");returnFader->setValue(-12,juce::sendNotificationSync);current=lib.snapshot();near((float)current.kits[0].returnRouting[0].mixer.volumeDb,-12,"Return mixer fader not wired");
    DrumAudioProcessor track(true);track.newKit();juce::AudioProcessor::TrackProperties headerProperties;headerProperties.name="9 PERCUSSION";track.updateTrackProperties(headerProperties);check(track.getLibrary().restore(*lib.toXml()),"Track mixer fixture recall");
    std::unique_ptr<juce::AudioProcessorEditor> editor(track.createEditorIfNeeded());editor->addToDesktop(0);editor->setVisible(true);
    auto* open=dynamic_cast<juce::Button*>(find(*editor,"open-mixer"));check(open && open->isVisible() && open->isEnabled(),"Kit mixer button missing or disabled");open->onClick();auto* mixer=find(*editor,"drum-mixer");check(mixer && mixer->isShowing() && mixer->getWidth()>1000,"Mixer overlay did not open");capture(*editor,"drum-mixer-full-editor.png");auto* fullMixer=dynamic_cast<DrumMixerPage*>(mixer);fullMixer->selectGroup(s.group);auto* instrumentName=dynamic_cast<juce::Button*>(find(*fullMixer,"mixer-name-"+s.b));check(instrumentName!=nullptr,"Mixer instrument-name navigation missing");instrumentName->onClick();auto* instrumentEditor=find(*editor,"sound-editor");check(instrumentEditor && instrumentEditor->isShowing() && !mixer->isVisible(),"Mixer instrument name did not open editor");auto* noteLabel=dynamic_cast<juce::Label*>(find(*instrumentEditor,"note-indicator"));auto selectedState=track.getLibrary().snapshot();const auto* selectedInstrument=drum::findInstrument(*drum::activeKit(selectedState),s.b);check(noteLabel && selectedInstrument && noteLabel->getText().contains("MIDI "+juce::String(selectedInstrument->note)),"Mixer opened the wrong instrument editor");check(!dynamic_cast<juce::Button*>(find(*editor,"open-mixer"))->getToggleState(),"Mixer navigation left its toolbar selected");dynamic_cast<juce::Button*>(find(*instrumentEditor,"edit-tab-7"))->onClick();open->onClick();check(!instrumentEditor->isVisible() && mixer->isShowing(),"Returning to mixer retained an overlapping instrument editor");
    auto* mixerBack=dynamic_cast<juce::Button*>(find(*mixer,"mixer-back"));check(mixerBack!=nullptr,"Mixer Back button missing");mixerBack->onClick();check(instrumentEditor->isShowing() && !mixer->isVisible(),"Mixer Back did not restore its instrument origin");check(dynamic_cast<juce::Button*>(find(*instrumentEditor,"edit-tab-7"))->getToggleState(),"Back lost the instrument tab");
    auto* instrumentBack=dynamic_cast<juce::Button*>(find(*instrumentEditor,"back-sound-editor"));auto* instrumentExit=find(*instrumentEditor,"close-sound-editor");check(instrumentBack && instrumentExit && instrumentBack->getRight()<=instrumentExit->getX(),"Back is missing or overlaps instrument Exit");instrumentBack->onClick();check(mixer->isShowing() && !instrumentEditor->isVisible() && fullMixer->selectedGroup()==s.group,"Instrument Back lost the previous Mixer view");
#if JUCE_MAC
    auto* groupButton=dynamic_cast<juce::Button*>(find(*mixer,"mixer-name-"+s.group));check(groupButton!=nullptr,"Mixer group button missing");groupButton->onClick();juce::TopLevelWindow* groupWindow=nullptr;juce::Component* groupSetup=nullptr;for(int n=0;n<juce::TopLevelWindow::getNumTopLevelWindows();++n){auto* window=juce::TopLevelWindow::getTopLevelWindow(n);if(auto* setup=showing(*window,"group-processing")){groupWindow=window;groupSetup=setup;break;}}check(groupWindow && groupSetup,"Mixer group button did not open group setup");auto* groupBack=dynamic_cast<juce::Button*>(find(*groupSetup,"group-back"));auto* groupExit=find(*groupSetup,"group-exit");check(groupBack && groupExit && groupBack->getRight()<groupExit->getX(),"Group Back is missing or overlaps Exit");groupBack->onClick();check(!groupWindow->isVisible() && mixer->isShowing() && fullMixer->selectedGroup()==s.group,"Group Back lost its Mixer origin");
#endif

    dynamic_cast<juce::Button*>(find(*mixer,"mixer-back"))->onClick();check(mixer->isShowing() && fullMixer->selectedGroup().isEmpty(),"Back from a group did not restore the Mixer overview");
    auto* floatToggle=dynamic_cast<juce::ToggleButton*>(find(*editor,"float-window"));check(floatToggle && floatToggle->getY()==135 && floatToggle->getRight()==floatToggle->getParentComponent()->getWidth()-24,"Float checkbox is not on the name row under BPM");
    const char* navigation[]{"file-menu","library-menu","kit-menu","open-mixer","live-setup"};int prior=-1;for(auto* id:navigation){auto* button=find(*editor,id);check(button && button->getWidth()==108 && button->getX()>prior,"Navigation button widths/order differ");prior=button->getX();}
    check(find(*editor,"live-setup")->getX()-find(*editor,"open-mixer")->getX()==232,"Setup gap is not one button wide");
    check(dynamic_cast<juce::Button*>(find(*editor,"open-mixer"))->getToggleState() && !dynamic_cast<juce::Button*>(find(*editor,"kit-menu"))->getToggleState(),"Mixer active button is not depressed");
    check(find(*editor,"open-mixer")->getX()<find(*editor,"live-setup")->getX(),"Mixer and Setup order wrong");check(showing(*editor,"file-menu")!=nullptr,"File button missing");
    auto context=[&](juce::Component& button){auto time=juce::Time::getCurrentTime();juce::Point<float> point{10,10};juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys::rightButtonModifier,1,0,0,0,0,&button,&button,time,point,time,1,false);button.mouseDown(event);check(juce::Component::getCurrentlyModalComponent()!=nullptr,"Navigation right-click menu missing");juce::PopupMenu::dismissAllActiveMenus();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);};
    context(*showing(*editor,"kit-menu"));check(mixer->isVisible(),"Right-click navigated away from Mixer");dynamic_cast<juce::Button*>(showing(*editor,"kit-menu"))->onClick();check(!mixer->isVisible() && juce::Component::getCurrentlyModalComponent()==nullptr,"Kit button did not navigate without a menu");
    {auto* scroll=dynamic_cast<juce::ScrollBar*>(find(*editor,"instrument-scrollbar"));auto* viewport=dynamic_cast<juce::Viewport*>(find(*editor,"instrument-viewport"));auto* tiles=find(*editor,"instrument-grid");check(scroll && viewport && tiles && scroll->isShowing() && !scroll->isEnabled(),"Unused instrument scrollbar is not visible and disabled");check(!find(*editor,"instrument-up")->isShowing() && !find(*editor,"instrument-down")->isShowing(),"Earlier/Later buttons remain visible");capture(*editor,"drum-instrument-one-page.png");auto fixture=track.getLibrary().snapshot();for(int n=0;n<18;++n)track.getLibrary().addInstrument(fixture.activeKit,fixture.kits[0].groups[0].id);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);juce::Timer::callPendingTimersSynchronously();check(scroll->isEnabled(),"Overflow scrollbar not enabled");
        viewport->setViewPosition(0,0);juce::MouseWheelDetails wheel{};wheel.deltaY=-.5f;auto time=juce::Time::getCurrentTime();auto wheelAt=[&](juce::Point<float> point){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,{},0,0,0,0,0,tiles,tiles,time,point,time,0,false);};auto elsewhere=wheelAt({tiles->getWidth()/3.f+15,20});tiles->mouseExit(elsewhere);tiles->mouseWheelMove(elsewhere,wheel);check(viewport->getViewPositionY()==0,"Unhighlighted instrument scrolled");tiles->mouseMove(elsewhere);tiles->mouseWheelMove(elsewhere,wheel);check(viewport->getViewPositionY()>0,"Highlighted instrument wheel did not scroll");auto before=viewport->getViewPositionY();tiles->mouseWheelMove(wheelAt({-2,-2}),wheel);check(viewport->getViewPositionY()==before,"Blank space scrolled instrument list");
        auto* name=dynamic_cast<juce::Label*>(find(*editor,"track-name"));check(name && name->getY()==66 && name->getRight()==name->getParentComponent()->getWidth()-24 && std::abs(name->getFont().getHeight()-18)<.01,"Track header alignment/size incorrect");check(name->getText()=="9 PERCUSSION","Track header has missing or quoted name");capture(*editor,"drum-instrument-scrolling.png");}
    dynamic_cast<juce::Button*>(showing(*editor,"library-menu"))->onClick();check(juce::Component::getCurrentlyModalComponent()==nullptr,"Library navigation opened a file menu");check(dynamic_cast<juce::Button*>(showing(*editor,"library-menu"))->getToggleState(),"Library button is not depressed on Library page");auto* back=showing(*editor,"kit-menu");check(back!=nullptr,"Kit button missing from Library");auto* libraryScreen=back->getParentComponent();
    auto* libraryProcessor=track.libraryEditorProcessor();if(libraryProcessor->librariesEmpty())check(libraryProcessor->newLibraryDocument().ok,"Navigation library fixture");auto libraryState=libraryProcessor->getLibrary().snapshot();auto libraryId=libraryState.activeKit;libraryProcessor->getLibrary().rename(libraryId,"Unsaved navigation test");context(*showing(*editor,"library-menu"));dynamic_cast<juce::Button*>(back)->onClick();check(!libraryScreen->isVisible() && juce::Component::getCurrentlyModalComponent()==nullptr,"Returning to Kit prompted to close the library");check(libraryProcessor->getLibrary().snapshot().kits[0].name=="Unsaved navigation test","Navigation discarded library edits");
    dynamic_cast<juce::Button*>(showing(*editor,"file-menu"))->onClick();check(juce::Component::getCurrentlyModalComponent()!=nullptr,"File operations menu missing");check(dynamic_cast<juce::Button*>(showing(*editor,"file-menu"))->getToggleState(),"File button not depressed while menu is open");juce::PopupMenu::dismissAllActiveMenus();juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    MeterPeakHold hold;near(hold.process(1,0,48000,1),1,"Peak hold attack");near(hold.process(0,48000,48000,1),1,"Peak hold did not last one second");check(hold.process(0,4800,48000,1)<1,"Peak hold did not fall after delay");MeterPeakHold custom;custom.process(.5f,0,48000,2);near(custom.process(0,48000,48000,2),.5f,"Custom peak delay ignored");near(custom.process(0,48000,48000,2),.5f,"Custom hold ended early");
    near((float)track.getPeakHoldSeconds(),1,"Peak hold default not one second");track.setPeakHoldSeconds(2.5);juce::MemoryBlock saved;track.getStateInformation(saved);DrumAudioProcessor recalled(true);recalled.setStateInformation(saved.getData(),(int)saved.getSize());near((float)recalled.getPeakHoldSeconds(),2.5f,"Peak hold setting not saved in Live state");
    auto* floatMixerButton=dynamic_cast<juce::Button*>(showing(*editor,"open-mixer"));check(floatMixerButton!=nullptr,"Float mixer button not showing");floatMixerButton->onClick();juce::AudioProcessor::TrackProperties trackProperties;trackProperties.name="9 PERCUSSION";track.updateTrackProperties(trackProperties);track.setFloatEnabled(true);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);auto* floating=track.getFloatingWindow();check(floating && floating->isVisible() && !floating->isAlwaysOnTop(),"Float did not create an independent normal-level window");check(editor->getAlpha()==0,"Hosted editor not suppressed while floating");check(track.getHostedDrumEditor()==editor.get(),"Companion replaced the registered hosted editor");editor->setVisible(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(70);check(!editor->isVisible() && floating->isVisible(),"Track selection hiding closed Float or revived its hosted editor");check(find(*floating->getContentComponent(),"drum-mixer")!=nullptr,"Float did not preserve Mixer view");capture(*floating->getContentComponent(),"drum-floating-mixer.png");check(floating->getName()=="DJL DRUM" && dynamic_cast<juce::Label*>(find(*floating->getContentComponent(),"track-name"))->getText()=="9 PERCUSSION","Float title missing track name");
    DrumAudioProcessor otherTrack(true);juce::AudioProcessor::TrackProperties otherProperties;otherProperties.name="7 KICK";otherTrack.updateTrackProperties(otherProperties);otherTrack.setFloatEnabled(true);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);auto* otherWindow=otherTrack.getFloatingWindow();check(otherWindow && otherWindow!=floating && otherWindow->isVisible() && floating->isVisible(),"Two tracks cannot float simultaneously");track.setFloatEnabled(true);juce::MessageManager::getInstance()->runDispatchLoopUntil(50);check(track.getFloatingWindow()==floating,"Same track opened a second floating window");trackProperties.name="9 RENAMED";track.updateTrackProperties(trackProperties);juce::MessageManager::getInstance()->runDispatchLoopUntil(50);check(dynamic_cast<juce::Label*>(find(*floating->getContentComponent(),"track-name"))->getText()=="9 RENAMED" && dynamic_cast<juce::Label*>(find(*otherWindow->getContentComponent(),"track-name"))->getText()=="7 KICK","Track rename leaked between windows");
    track.editorBeingDeleted(editor.get());editor.reset();juce::MessageManager::getInstance()->runDispatchLoopUntil(50);check(track.getFloatingWindow()==floating && floating->isVisible(),"Float closed when the host editor was destroyed");
    editor.reset(track.createEditor());check(track.getActiveEditor()==nullptr && track.getHostedDrumEditor()==editor.get(),"Direct host editor registration failed");editor->addToDesktop(0);editor->setVisible(true);track.setFloatEnabled(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);check(track.getFloatingWindow()==nullptr && editor->getAlpha()==1,"Float off did not restore the hosted editor");editor->setVisible(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(70);check(!editor->isVisible(),"Non-float timer revived an editor hidden by the host");editor->setVisible(true);check(otherTrack.getFloatingWindow()==otherWindow && otherWindow->isVisible(),"Closing one track closed another track window");otherTrack.setFloatEnabled(false);juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    auto* identity=dynamic_cast<juce::AudioProcessorParameter*>(track.getParameters().getLast());auto token=identity->getName(64).fromFirstOccurrenceOf("DJL Link ",false,false);check(token.length()==16,"Track link token missing");auto assignment=drum::LiveKitLink::folder().getChildFile(token+".track.json");
    auto publishAssignment=[&](bool allowed){auto* object=new juce::DynamicObject;juce::var data(object);object->setProperty("protocol",1);object->setProperty("token",token);object->setProperty("timestamp",juce::Time::getCurrentTime().toMilliseconds()/1000.0);object->setProperty("trackName","Bridge Percussion");object->setProperty("allowed",allowed);object->setProperty("selected",false);check(assignment.replaceWithText(juce::JSON::toString(data)),"Track assignment fixture write");juce::MessageManager::getInstance()->runDispatchLoopUntil(180);};
    publishAssignment(false);check(track.isDuplicateBlocked() && !track.hasEditor(),"Duplicate instance not blocked by host track assignment");juce::AudioBuffer<float> blockedAudio(2,128);blockedAudio.clear();blockedAudio.addSample(0,0,1);juce::MidiBuffer blockedMidi;blockedMidi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);track.processBlock(blockedAudio,blockedMidi);check(blockedAudio.getMagnitude(0,128)==0 && blockedMidi.isEmpty(),"Duplicate instance still produced audio/MIDI");
    publishAssignment(true);check(!track.isDuplicateBlocked() && track.hasEditor() && editor->getAlpha()==1 && track.trackName()=="Bridge Percussion" && track.windowTitle()=="DJL DRUM" && !track.shouldShowHostedEditor(),"Track reassignment did not restore its own window");assignment.deleteFile();
    track.editorBeingDeleted(editor.get());
#if JUCE_MAC
    {
        Setup groups;auto& model=groups.p.getLibrary();
        auto effect=[](float db){BuiltinEffect utility(BuiltinKind::utility);utility.controls[0]->setValueNotifyingHost(utility.controls[0]->convertTo0to1(db));juce::MemoryBlock state;utility.getStateInformation(state);return std::vector<EffectSlot>{{juce::Uuid().toString(),builtinDescription(BuiltinKind::utility).createXml()->toString(),state.toBase64Encoding(),false}};};
        check(model.setEffects(groups.kit,groups.a,effect(-6.0206f)).ok,"Instrument chain fixture");
        check(model.setEffects(groups.kit,groupChainId(groups.group),effect(-6.0206f)).ok,"Group chain fixture");
        {auto state=model.snapshot();auto controls=findInstrument(state.kits[0],groups.a)->controls;controls.sound.bypassChain=0;check(model.setControls(groups.kit,groups.a,controls).ok,"Enable member chain");}
        auto output=groups.hit();near(output.getSample(0,127),.175f,"Group chain did not follow summed member chains");
        auto state=model.snapshot();auto controls=findInstrument(state.kits[0],groups.a)->controls;controls.sound.sendA=0;model.setControls(groups.kit,groups.a,controls);
        check(model.setEffects(groups.kit,"return-0",effect(-6.0206f)).ok,"Instrument return chain fixture");
        check(model.setSendDestination(groups.kit,"return-0",false,groups.group).ok,"Send-to-group route rejected");
        output=groups.hit();near(output.getSample(0,127),.1875f,"Instrument return did not enter group before its chain");
        check(model.setGroupSend(groups.kit,groups.group,3,0).ok && model.setGroupSend(groups.kit,groups.other,3,0).ok,"Independent group send levels rejected");
        check(model.setEffects(groups.kit,groupReturnId(groups.group,3),effect(-6.0206f)).ok && model.setEffects(groups.kit,groupReturnId(groups.other,3),effect(-12.0412f)).ok,"Independent group return chains rejected");
        output=groups.hit();near(output.getSample(0,127),.26875f,"Group send chain affected another group or instrument return");
        check(!model.setSendDestination(groups.kit,groupReturnId(groups.group,3),false,groups.group).ok,"Group-return feedback was accepted");
        check(model.setSendDestination(groups.kit,"return-0",true,{}).ok,"Return-to-Main route rejected");output=groups.hit();near(output.getSample(0,127),.275f,"Main destination also fed the group chain");
        check(model.setSendRoute(groups.kit,"return-0",1,0).ok,"Send-to-send route rejected");check(!model.setSendRoute(groups.kit,"return-1",0,0).ok,"Send cycle accepted");
        auto legacy=model.toXml();legacy->setAttribute("schemaVersion",27);for(auto* group:legacy->getChildByName("Kit")->getChildIterator())if(group->hasTagName("Group")){group->deleteAllChildElementsWithTagName("GroupChain");group->deleteAllChildElementsWithTagName("GroupReturn");}
        juce::XmlElement* legacyA=nullptr;juce::XmlElement* legacyD=nullptr;for(auto* ret:legacy->getChildByName("Kit")->getChildIterator())if(ret->hasTagName("Return")){if(ret->getIntAttribute("index")==0)legacyA=ret;if(ret->getIntAttribute("index")==3)legacyD=ret;}
        auto* oldEffect=new juce::XmlElement(*legacyA->getChildByName("Effect"));oldEffect->setAttribute("id",juce::Uuid().toString());legacyD->addChildElement(oldEffect);
        KitLibrary migrated;check(migrated.restore(*legacy),"Legacy group send conversion failed");auto migratedState=migrated.snapshot();check(findGroup(migratedState.kits[0],groups.group)->returns[0].effects.empty(),"Unconnected legacy return was copied");check(findGroup(migratedState.kits[0],groups.group)->returns[3].effects.size()==1 && findGroup(migratedState.kits[0],groups.group)->returns[3].effects[0].id!=migratedState.kits[0].returns[3].effects[0].id,"Connected legacy group send lost its effects or reused a shared instance");
        auto pattern=model.addPattern(groups.kit);check(pattern.ok,"Group pattern fixture");auto patterns=model.snapshot();auto invalid=patterns.kits[0].patterns.back();invalid.hits.push_back({groupChainId(groups.group),0,100});check(!model.setPattern(groups.kit,invalid).ok,"Group audio bus was accepted as a MIDI instrument");
        auto xml=model.toXml();KitLibrary recalled;check(recalled.restore(*xml),"Group processing recall failed");auto restored=recalled.snapshot();auto* first=findGroup(restored.kits[0],groups.group);auto* second=findGroup(restored.kits[0],groups.other);check(first && second && first->chain.effects.size()==1 && first->returns[3].effects[0].state!=second->returns[3].effects[0].state,"Group chains did not recall independently");
        check(recalled.duplicateGroup(groups.kit,groups.group).ok && recalled.duplicateKit(groups.kit).ok,"Copying group processing reused effect identities");
        GroupEditor editor(groups.p,groups.kit,groups.group);editor.setSize(1100,820);capture(editor,"drum-group-processing.png");
    }
#endif
    std::cout<<"Mixer levels, groups, returns, routing, persistence and UI passed\n";
}
