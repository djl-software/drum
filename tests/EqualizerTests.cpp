#include "LegacyTestState.h"
#include "PluginEditor.h"
#include "SoundEditor.h"
#include <iostream>
#include <stdexcept>
namespace
{
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
juce::Component* find(juce::Component& c,const juce::String& id)
{if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
template<class T>T& control(juce::Component& c,const char* id)
{auto* value=dynamic_cast<T*>(find(c,id));check(value!=nullptr,"EQ control missing");return *value;}
void click(juce::Component& c,const char* id){control<juce::TextButton>(c,id).onClick();}
juce::MouseEvent event(juce::Component& c,juce::Point<float> p,juce::Point<float> down,int modifiers=juce::ModifierKeys::leftButtonModifier)
{auto now=juce::Time::getCurrentTime();return {juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(modifiers),1,0,0,0,0,&c,&c,now,down,now,1,p!=down};}
void capture(juce::Component& c,const char* name)
{auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream stream(file);check(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),stream),"EQ screenshot failed");}
}
void runEqualizerTests()
{
    for(double rate:{44100.0,48000.0,96000.0})for(int type=0;type<8;++type)
    {
        drum::Equalizer eq;eq.enabled=true;eq.bands[0]={true,type,1000,9,0.70710678};
        for(double frequency:{100.0,1000.0,10000.0})
        {
            drum::VoiceEqualizer dsp;dsp.reset(eq,rate);double inputEnergy=0,outputEnergy=0;
            for(int n=0;n<(int)rate;++n)
            {
                const float input=(float)(0.1*std::sin(juce::MathConstants<double>::twoPi*frequency*n/rate));float left=input,right=-input;dsp.process(left,right);
                check(std::isfinite(left) && std::abs(left+right)<1e-6,"EQ stereo paths differ or output nonfinite");
                if(n>rate/2){inputEnergy+=input*input;outputEnergy+=left*left;}
            }
            check(std::abs(std::sqrt(outputEnergy/inputEnergy)-drum::eqMagnitude(eq,frequency,rate))<0.003,"EQ response curve disagrees with processed audio");
        }
        // Extreme resonance and corner frequencies must remain finite, including live changes.
        drum::VoiceEqualizer dsp;dsp.reset(eq,rate);
        for(int n=0;n<12000;++n)
        {
            if(n%500==0){eq.bands[0].frequency=n%1000?20:20000;eq.bands[0].q=n%1000?0.1:18;eq.enabled=n%1500!=0;dsp.update(eq,rate);}
            float l=n==0?1.0f:0.0f,r=l;dsp.process(l,r);check(std::isfinite(l) && std::abs(l)<1e10,"EQ unstable during edits");
        }
    }
    drum::Equalizer bypass;drum::VoiceEqualizer dsp;dsp.reset(bypass,48000);float l=.25f,r=-.2f;dsp.process(l,r);check(l==.25f && r==-.2f,"Default EQ alters old sounds");
    drum::Equalizer extreme;extreme.enabled=true;extreme.output=24;for(auto& b:extreme.bands)b={true,1,1000,24,18};dsp.reset(extreme,48000);
    for(int n=0;n<96000;++n){l=r=(float)std::sin(juce::MathConstants<double>::twoPi*1000*n/48000);dsp.process(l,r);check(std::isfinite(l) && std::isfinite(r),"Stacked EQ resonance overflows host audio");}
    DrumAudioProcessor processor;auto& library=processor.getLibrary();auto state=library.snapshot();const auto kit=state.activeKit,group=drum::activeKit(state)->groups[0].id,id=library.addInstrument(kit,group).id;
    auto current=[&]{auto s=library.snapshot();return drum::findInstrument(*drum::activeKit(s),id)->controls;};
    auto c=current();c.sound.equalizer.enabled=true;c.sound.equalizer.output=-3;
    for(int n=0;n<8;++n)c.sound.equalizer.bands[(size_t)n]={true,n,100.0*(n+1),n-4.0,0.5+n*0.1};
    check(library.setControls(kit,id,c).ok,"Cannot save EQ");auto xml=library.toXml();drum::KitLibrary restored;check(restored.restore(*xml),"EQ state restore failed");
    auto recalled=restored.snapshot();check(drum::findInstrument(*drum::activeKit(recalled),id)->controls==c,"EQ state changed on recall");
    auto* instrument=xml->getFirstChildElement()->getFirstChildElement()->getFirstChildElement();instrument->setAttribute("eq3q",-1);
    const auto prior=restored.toXml()->toString();check(!restored.restore(*xml) && restored.toXml()->toString()==prior,"Invalid EQ partially restored");
    xml=library.toXml();stripChainEffects(*xml);xml->setAttribute("schemaVersion",7);check(restored.restore(*xml),"Version 7 migration failed");recalled=restored.snapshot();
    check(drum::findInstrument(*drum::activeKit(recalled),id)->controls.sound.equalizer==drum::Equalizer{},"Old state acquired EQ processing");
    processor.getSampleEngine().requestLoad(kit,id,juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Preview percussion.wav"));check(processor.getSampleEngine().waitForLoading(10000),"EQ sample load failed");processor.getSampleEngine().prepare(48000);
    c=current();c.sound.equalizer={};check(library.setControls(kit,id,c).ok,"Reset EQ failed");
    DrumAudioProcessorEditor editor(processor);editor.setVisible(true);editor.setSize(1280,900);click(editor,"open-sound-editor");click(editor,"edit-tab-2");
    auto& graph=control<EqualizerEditor>(editor,"eight-band-eq");check(graph.isVisible(),"EQ graph hidden");
    const juce::Point<float> down{graph.getWidth()*0.5f,graph.getHeight()*0.5f},target=down+juce::Point<float>{40,-20};
    check(editor.getComponentAt(editor.getLocalPoint(&graph,down.roundToInt()))==&graph,"EQ mouse hit intercepted");
    graph.mouseDown(event(graph,down,down));graph.mouseDrag(event(graph,target,down));graph.mouseUp(event(graph,target,down));
    auto changed=current().sound.equalizer;check(changed.enabled,"EQ drag did not enable processing");int selected=-1;for(int n=0;n<8;++n)if(changed.bands[(size_t)n].enabled)selected=n;
    check(selected>=0 && changed.bands[(size_t)selected].gain!=0,"EQ drag did not change band gain");
    control<juce::Slider>(editor,"eq-q").setValue(2,juce::sendNotificationSync);check(current().sound.equalizer.bands[(size_t)selected].q==2,"EQ Q edit failed");
    control<juce::ComboBox>(editor,"eq-type").setSelectedId(2,juce::sendNotificationSync);check(!control<juce::Slider>(editor,"eq-gain").isEnabled(),"Cut band exposes inactive gain");
    auto source=processor.getSampleEngine().waveformFor(id);auto dry=drum::SampleEngine::renderPreview(source,drum::InstrumentControls{},{},120,48000);
    auto wet=drum::SampleEngine::renderPreview(source,current(),{},120,48000);check(wet.sample && dry.sample && std::abs(wet.sample->peak-dry.sample->peak)>0.01,"EQ does not affect edited waveform DSP");
    for(int n=0;n<40;++n){juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    capture(editor,"drum-eight-band-eq.png");editor.setSize(800,600);capture(editor,"drum-eight-band-eq-compact.png");editor.setSize(1280,900);
    click(editor,"edit-tab-0");auto& sampleGain=control<juce::Slider>(editor,"edit-sampleGain");sampleGain.setValue(24,juce::sendNotificationSync);
    auto settle=[&] {for(int n=0;n<1000;++n){juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();if(n>20 && control<juce::Label>(editor,"waveform-status").getText().startsWith("One hit"))return;}throw std::runtime_error("Clipping preview timed out");};
    settle();
    check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)==juce::Colour(0xff8f2638),"High sample gain does not warn red");capture(editor,"drum-sample-clipping.png");
    sampleGain.setValue(-24,juce::sendNotificationSync);check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)!=juce::Colour(0xff8f2638),"Clipping warning did not clear");
    settle();
    sampleGain.setValue(24,juce::sendNotificationSync);
    control<juce::Slider>(editor,"instrument-volume").setValue(-60,juce::sendNotificationSync);settle();
    check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)!=juce::Colour(0xff8f2638),"High gain falsely warned despite silent processed output");
    control<juce::Slider>(editor,"instrument-volume").setValue(-24,juce::sendNotificationSync);settle();
    check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)!=juce::Colour(0xff8f2638),"High gain falsely warned despite fader headroom");
    // Exactly full scale is not an overload; a little above it is.
    auto fullFile=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Full scale boundary.wav");fullFile.deleteFile();
    auto stream=fullFile.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));check(writer!=nullptr,"Full scale writer failed");
    juce::AudioBuffer<float> constant(1,4800);for(int n=0;n<4800;++n)constant.setSample(0,n,0.5f);writer->writeFromAudioSampleBuffer(constant,0,4800);writer.reset();
    c={};c.sound.normalise=1;c.sound.velocityVolume=0;check(library.setControls(kit,id,c).ok,"Full scale controls failed");
    processor.getSampleEngine().requestLoad(kit,id,fullFile);check(processor.getSampleEngine().waitForLoading(10000),"Full scale sample failed");settle();
    auto full=drum::SampleEngine::renderPreview(processor.getSampleEngine().waveformFor(id),c,{},120,48000);
    check(full.sample && full.sample->peak==1.0f,"Full scale fixture is not exactly 0 dBFS");
    check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)!=juce::Colour(0xff8f2638),"Exactly full scale falsely warns red");
    sampleGain.setValue(0.1,juce::sendNotificationSync);settle();
    check(sampleGain.findColour(juce::Slider::textBoxBackgroundColourId)==juce::Colour(0xff8f2638),"Above full scale did not warn red");
    std::cout<<"Eight-band EQ response/audio agreement, stereo, live edits, recall, migration, UI hit testing and gain warning passed\n";
}
