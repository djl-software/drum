#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SoundEditor.h"
#include "EnvelopeEditor.h"
#include "BuiltinEffects.h"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
juce::Component* find(juce::Component& root,const char* id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
template<class T>T& control(juce::Component& root,const char* id){auto* result=dynamic_cast<T*>(find(root,id));check(result!=nullptr,id);return *result;}
void click(juce::Component& root,const char* id){control<juce::TextButton>(root,id).onClick();}
void near(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int channel,const char* message){for(int c=channel;c<channel+2;++c)for(int n=0;n<a.getNumSamples();++n)check(std::abs(a.getSample(c,n)-b.getSample(c,n))<1.e-6f,message);}
float difference(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int channel){float total=0;for(int n=0;n<a.getNumSamples();++n)total+=std::abs(a.getSample(channel,n)-b.getSample(channel,n));return total;}
}
void runSendProcessingTests(){
    DrumAudioProcessor p(true);p.newKit();auto& lib=p.getLibrary();auto& engine=p.getSampleEngine();auto state=lib.snapshot();const auto kit=state.activeKit,group=state.kits[0].groups[0].id;
    const auto id=lib.addInstrument(kit,group).id;
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("send-processing.wav");file.deleteFile();juce::WavAudioFormat format;
    {std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(file),48000,2,24,{},0));check(writer!=nullptr,"Send fixture writer");juce::AudioBuffer<float> audio(2,48000);for(int n=0;n<48000;++n)for(int c=0;c<2;++c)audio.setSample(c,n,.2f*(float)std::sin(juce::MathConstants<double>::twoPi*1000*n/48000));check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Send fixture audio");}
    auto layout=p.getBusesLayout();layout.outputBuses.set(1,juce::AudioChannelSet::stereo());layout.outputBuses.set(2,juce::AudioChannelSet::stereo());check(p.setBusesLayout(layout),"Send output layout");p.prepareToPlay(48000,128);
    engine.requestLoad(kit,id,file);check(engine.waitForLoading(10000),"Send loading");
    // Stage on/off must change the sound and restore the exact retained processing.
    auto sourceSample=engine.waveformFor(id);
    for(int stage=0;stage<7;++stage){
        drum::InstrumentControls active,off,reference;
        switch(stage){
        case 0:active.sound.sampleGain=-12;off=active;off.sound.bypassSample=1;break;
        case 1:active.sound.mode=2;active.sound.envelopes[0]=drum::adsrShape(1,10,.2,10);off=active;off.sound.bypassAmplitude=1;reference.sound.mode=2;reference.sound.ampDecay=0;reference.sound.ampRelease=2;break;
        case 2:active.tuning=12;off=active;off.sound.bypassPitch=1;break;
        case 3:active.sound.filterOn=1;active.sound.cutoff=100;off=active;off.sound.filterOn=0;reference=off;break;
        case 4:active.sound.equalizer.enabled=true;active.sound.equalizer.bands[2]={true,3,1000,-18,1};off=active;off.sound.equalizer.enabled=false;reference=off;break;
        case 5:active.sound.lfoOn=1;active.sound.lfoVolume=1;active.sound.lfoRate=9;off=active;off.sound.lfoOn=0;reference=off;break;
        case 6:active.sound.attackOn=1;active.sound.attackLevel=-3;off=active;off.sound.attackOn=0;reference=off;break;}
        auto render=[&](const drum::InstrumentControls& c){return drum::SampleEngine::renderPreview(sourceSample,c,{},120,48000).sample;};
        auto wet=render(active),dry=render(off),expected=render(reference),again=render(active);check(wet && dry && expected && again,"Stage bypass preview missing");
        const int count=juce::jmin(16384,wet->audio.getNumSamples(),dry->audio.getNumSamples(),expected->audio.getNumSamples());double changed=0;
        for(int n=0;n<count;++n){changed+=std::abs(wet->audio.getSample(0,n)-dry->audio.getSample(0,n));for(int ch=0;ch<2;++ch){check(std::abs(dry->audio.getSample(ch,n)-expected->audio.getSample(ch,n))<1e-6f,"Disabled stage did not produce neutral audio");check(wet->audio.getSample(ch,n)==again->audio.getSample(ch,n),"Stage re-enable changed retained processing");}}
        check(changed>.01,"Stage enable/disable made no audible difference");
    }
    auto settings=drum::InstrumentControls{};settings.sound.sendA=0;settings.sound.sendB=0;check(lib.setControls(kit,id,settings).ok,"Send initial controls");
    auto routing=lib.snapshot().kits[0].returnRouting;routing[0].mixer.output=1;routing[1].mixer.output=2;check(lib.setReturnMixer(kit,0,routing[0].mixer).ok && lib.setReturnMixer(kit,1,routing[1].mixer).ok,"Send return routing");
    auto hit=[&](int note=21){p.getEffects().sync();check(engine.waitForLoading(10000),"Send bank update");engine.stop();juce::MidiBuffer midi;juce::AudioBuffer<float> out(p.getTotalNumOutputChannels(),128);p.processBlock(out,midi);midi.addEvent(juce::MidiMessage::noteOn(1,note,(juce::uint8)127),0);p.processBlock(out,midi);midi.clear();for(int n=0;n<20;++n)p.processBlock(out,midi);return out;};
    auto baseline=hit();check(baseline.getMagnitude(2,0,128)>.1f && baseline.getMagnitude(4,0,128)>.1f,"Default sends not audible");
    auto filter=std::make_shared<drum::SendProcessing>();filter->sound=settings.sound;filter->sound.filterOn=1;filter->sound.cutoff=100;settings.sendProcessing[0]=filter;check(lib.setControls(kit,id,settings).ok,"Independent filter save");auto filtered=hit();near(baseline,filtered,0,"Send filter changed main audio");near(baseline,filtered,4,"Send filter changed Send B");check(filtered.getMagnitude(2,0,128)<.03f,"Send filter did not affect audible return");
    auto bypassFilter=std::make_shared<drum::SendProcessing>(*filter);bypassFilter->sound.filterOn=0;settings.sendProcessing[0]=bypassFilter;check(lib.setControls(kit,id,settings).ok,"Independent filter bypass save");auto filterOff=hit();near(baseline,filterOff,0,"Send filter bypass changed main");near(baseline,filterOff,2,"Send filter off did not restore neutral send");near(baseline,filterOff,4,"Send filter off changed other send");
    auto pitch=std::make_shared<drum::SendProcessing>(*filter);pitch->sound.filterOn=0;pitch->tuning=12;settings.sendProcessing[0]=pitch;check(lib.setControls(kit,id,settings).ok,"Independent pitch save");auto pitched=hit();near(baseline,pitched,0,"Send pitch changed main audio");near(baseline,pitched,4,"Send pitch changed Send B");check(difference(baseline,pitched,2)>1,"Send pitch did not affect audible return");
    auto envelope=std::make_shared<drum::SendProcessing>(*pitch);envelope->tuning=0;envelope->sound.mode=2;auto& shape=envelope->sound.envelopes[0];shape.count=4;shape.hold=3;for(int n=0;n<4;++n)shape.points[n]={n*1000./3,.25};settings.sendProcessing[0]=envelope;check(lib.setControls(kit,id,settings).ok,"Independent envelope save");auto shaped=hit();near(baseline,shaped,0,"Send envelope changed main audio");near(baseline,shaped,4,"Send envelope changed Send B");check(shaped.getMagnitude(2,0,128)<baseline.getMagnitude(2,0,128)*.3,"Send envelope did not affect audible return");
    auto effect=p.getEffects().add(kit,"return-0",drum::builtinDescription(drum::BuiltinKind::utility));check(effect.ok,"Send return effect add");
    auto* utility=dynamic_cast<drum::BuiltinEffect*>(p.getEffects().instance(effect.id));check(utility!=nullptr,"Send return utility missing");utility->controls[0]->setValueNotifyingHost(utility->controls[0]->convertTo0to1(-6));
    auto wet=hit();near(shaped,wet,0,"Send return chain changed dry audio");near(shaped,wet,4,"Send return chain changed another send");check(wet.getMagnitude(2,0,128)<shaped.getMagnitude(2,0,128)*.55f,"Independent send did not enter shared return chain");
    auto xml=lib.toXml();drum::KitLibrary restored;check(restored.restore(*xml),"Independent sends could not be restored");check(restored.snapshot().kits[0].groups[0].instruments[0].controls==settings,"Independent send settings changed on recall");
    auto invalid=std::make_unique<juce::XmlElement>(*xml);auto* send=invalid->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument")->getChildByName("SendProcessing");check(send!=nullptr,"Saved send missing");send->setAttribute("cutoff",-1);check(!restored.restore(*invalid),"Invalid send filter accepted");
    std::array<float,drum::numMacros> macros{};auto sample=engine.waveformFor(id);auto mainPreview=drum::SampleEngine::renderPreview(sample,settings,macros,120,48000);auto sendPreview=drum::SampleEngine::renderPreview(sample,settings,macros,120,48000,0);check(mainPreview.sample && sendPreview.sample && sendPreview.sample->peak<mainPreview.sample->peak*.3f,"Send waveform preview does not use independent DSP");
    auto second=lib.addInstrument(kit,group).id;auto secondSettings=drum::InstrumentControls{};secondSettings.tuning=-5;secondSettings.sound.sendA=0;secondSettings.sound.sendB=0;check(lib.setControls(kit,second,secondSettings).ok,"Second instrument controls");engine.requestLoad(kit,second,file);check(engine.waitForLoading(10000),"Second instrument sample");auto otherInstrument=hit(22);
    DrumAudioProcessorEditor editor(p);editor.setSize(1280,900);click(editor,"open-sound-editor");state=lib.snapshot();control<SoundEditor>(editor,"sound-editor").select(kit,drum::findInstrument(*drum::activeKit(state),id));
    auto current=[&]{auto s=lib.snapshot();return drum::findInstrument(*drum::activeKit(s),id)->controls;};
    auto before=current();control<juce::ComboBox>(editor,"waveform-view").setSelectedId(4,juce::sendNotificationSync); // Send B
    click(editor,"overlay-pitch");control<juce::Slider>(editor,"envelope-pitch-amount").setValue(7,juce::sendNotificationSync);
    auto edited=current();check(edited.sound==before.sound && edited.sendProcessing[0] && *edited.sendProcessing[0]==*before.sendProcessing[0] && edited.sendProcessing[1] && std::abs(edited.sendProcessing[1]->sound.pitchAmount-7)<1.e-8,"Send selector edit leaked into main or another send");
    control<juce::ComboBox>(editor,"waveform-view").setSelectedId(2,juce::sendNotificationSync);check(std::abs(control<juce::Slider>(editor,"envelope-pitch-amount").getValue()-before.sound.pitchAmount)<1.e-8,"Main editor shows send pitch settings");
    control<juce::ComboBox>(editor,"waveform-view").setSelectedId(4,juce::sendNotificationSync);check(control<juce::Slider>(editor,"envelope-pitch-amount").getValue()>6.99,"Send editor did not recall its settings");
    click(editor,"edit-tab-8");control<juce::Slider>(editor,"key-coarse").setValue(9,juce::sendNotificationSync);auto tuned=current();check(tuned.tuning==before.tuning && tuned.sendProcessing[1]->tuning==9,"Send Tune changed main tuning");
    click(editor,"edit-tab-1");click(editor,"filter-envelope-tab");control<juce::ToggleButton>(editor,"editor-stage-enabled").setToggleState(true,juce::sendNotificationSync);control<juce::Slider>(editor,"edit-cutoff").setValue(714,juce::sendNotificationSync);auto tone=current();check(tone.sound==before.sound && tone.sendProcessing[1]->sound.cutoff==714 && *tone.sendProcessing[0]==*before.sendProcessing[0],"Send filter UI changed another destination");
    for(int n=0;n<6;++n){auto previous=current();control<juce::ComboBox>(editor,"waveform-view").setSelectedId(n+3,juce::sendNotificationSync);click(editor,"overlay-pitch");control<juce::Slider>(editor,"envelope-pitch-amount").setValue(n+2,juce::sendNotificationSync);auto changed=current();check(changed.sound==previous.sound && changed.sendProcessing[n] && std::abs(changed.sendProcessing[n]->sound.pitchAmount-(n+2))<1.e-8,"A-F selection not independently editable");for(int other=0;other<6;++other)if(other!=n)check(changed.sendProcessing[other]==previous.sendProcessing[other],"Editing one send replaced another send");}
    // Checkbox bypass affects only the selected send and retains its envelope.
    auto uiBefore=current();click(editor,"overlay-pitch");auto& stageOn=control<juce::ToggleButton>(editor,"editor-stage-enabled");stageOn.setToggleState(false,juce::sendNotificationSync);auto uiOff=current();check(uiOff.sound==uiBefore.sound && uiOff.sendProcessing[5]->sound.bypassPitch==1 && uiOff.sendProcessing[5]->sound.envelopes[1]==uiBefore.sendProcessing[5]->sound.envelopes[1],"Send pitch checkbox changed main or lost shape");stageOn.setToggleState(true,juce::sendNotificationSync);check(current()==uiBefore,"Send pitch checkbox did not restore exact state");
    auto legacy=std::make_unique<juce::XmlElement>(*xml);legacy->setAttribute("schemaVersion",25);legacy->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument")->deleteAllChildElementsWithTagName("SendProcessing");check(restored.restore(*legacy),"Legacy kit load failed");auto legacyState=restored.snapshot();for(auto& edit:legacyState.kits[0].groups[0].instruments[0].controls.sendProcessing)check(!edit,"Legacy kit gained send shaping");
    auto all=lib.toXml();check(restored.restore(*all),"All-six-send recall failed");check(restored.snapshot().kits[0].groups[0].instruments[0].controls==current(),"All-six-send settings lost on recall");
    control<juce::ComboBox>(editor,"waveform-view").setSelectedId(4,juce::sendNotificationSync);for(int n=0;n<40;++n){juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();juce::MessageManager::getInstance()->runDispatchLoopUntil(1);}
    auto otherAfter=hit(22);near(otherInstrument,otherAfter,0,"Send edits changed another instrument main output");near(otherInstrument,otherAfter,2,"Send edits changed another instrument Send A");near(otherInstrument,otherAfter,4,"Send edits changed another instrument Send B");
    juce::FileOutputStream image(juce::File::getCurrentWorkingDirectory().getChildFile("drum-independent-send.png"));juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),image);
    // Extra send voices must not consume the main instrument's random sequence.
    auto randomSettings=drum::InstrumentControls{};randomSettings.sound.sendA=0;randomSettings.sound.randomPan=.7;randomSettings.sound.lfoOn=1;randomSettings.sound.lfoShape=5;randomSettings.sound.lfoRate=30;randomSettings.sound.lfoPitch=2;randomSettings.sound.lfoPan=.3;
    auto reference=std::make_unique<DrumAudioProcessor>(true),independent=std::make_unique<DrumAudioProcessor>(true);auto shared=lib.toXml();
    for(auto* processor:{reference.get(),independent.get()}){check(processor->getLibrary().restore(*shared),"Random isolation fixture restore");check(processor->setBusesLayout(layout),"Random isolation layout");auto edit=randomSettings;if(processor==independent.get()){auto shape=std::make_shared<drum::SendProcessing>();shape->sound=edit.sound;shape->sound.filterOn=1;shape->sound.cutoff=100;edit.sendProcessing[0]=shape;}check(processor->getLibrary().setControls(kit,id,edit).ok,"Random isolation controls");processor->prepareToPlay(48000,128);processor->getEffects().sync();check(processor->getSampleEngine().waitForLoading(10000),"Random isolation loading");}
    juce::AudioBuffer<float> ref(6,128),alt(6,128);for(int hit=0;hit<3;++hit)for(int block=0;block<64;++block){juce::MidiBuffer midi;if(block==0)midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)100),0);juce::MidiBuffer otherMidi(midi);reference->processBlock(ref,midi);independent->processBlock(alt,otherMidi);near(ref,alt,0,"Independent send changed main random-pan/LFO sequence");}
    std::cout<<"Independent send envelope, filter, pitch, dry/other-send isolation, graph return routing, waveform preview, UI scope and persistence passed\n";
}
