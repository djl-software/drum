#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void near(float a,float b,const char* message) {check(std::abs(a-b)<0.00003f,message);}
std::shared_ptr<drum::Sample> source(int channels=2,bool opposite=false)
{
    auto result=std::make_shared<drum::Sample>();result->rate=48000;result->audio.setSize(channels,4800);
    for(int c=0;c<channels;++c)for(int n=0;n<4800;++n)result->audio.setSample(c,n,c==0?0.25f:opposite?-0.25f:0.5f);
    result->peak=result->audio.getMagnitude(0,4800);result->waveform.build(result->audio);return result;
}
}
void runChannelAttackTests()
{
    auto sample=source();drum::InstrumentControls controls;std::array<float,drum::numMacros> macros;macros.fill(0.5f);
    auto render=[&](double rate=48000,int send=-1) {
        auto result=drum::SampleEngine::renderPreview(sample,controls,macros,120,rate,send);
        check(result.sample!=nullptr,"Channel/attack preview failed");return result.sample;
    };
    auto stereo=render();near(stereo->audio.getSample(0,1000),0.2f,"Stereo left changed");near(stereo->audio.getSample(1,1000),0.4f,"Stereo right changed");
    controls.sound.mono=1;auto mono=render();
    near(mono->audio.getSample(0,1000),0.3f,"Mono must average stereo channels");
    check(mono->waveform.channelsAreIdentical(),"Mono output channels differ");
    sample=source();
    for(int n=0;n<4800;++n) {sample->audio.setSample(0,n,(float)std::sin(n*0.14)*0.33f);sample->audio.setSample(1,n,(float)std::cos(n*0.11)*0.27f);}
    check(render()->waveform.channelsAreIdentical(),"Mono rounding prevented one-lane display");sample=source();
    controls.pan=1;auto panned=render();near(panned->audio.getSample(0,1000),0,"Mono pan left not silent");near(panned->audio.getSample(1,1000),0.3f,"Mono pan changed right gain");
    controls.pan=0;sample=source(2,true);check(render()->peak==0,"Opposite-polarity stereo did not cancel in mono");
    sample=source(1);near(render()->audio.getSample(0,1000),0.2f,"Mono source changed level");
    sample=source();controls={};controls.sound.attackLevel=0;
    check(render()->audio.getSample(0,20)==stereo->audio.getSample(0,20),"Disabled attack changed sound");
    controls.sound.attackOn=1;
    std::array<float,3> signatures{};
    for(int type=0;type<3;++type)
    {
        controls.sound.attackType=type;auto hit=render();float delta=0;
        for(int n=0;n<384;++n)delta+=std::abs(hit->audio.getSample(0,n)-stereo->audio.getSample(0,n));
        check(delta>1,"Attack layer inaudible");signatures[(size_t)type]=delta;
        near(hit->audio.getSample(0,0),stereo->audio.getSample(0,0),"Attack begins with a discontinuity");
        for(int n=400;n<4000;++n)near(hit->audio.getSample(0,n),stereo->audio.getSample(0,n),"Attack altered sample tail");
        auto repeated=render();
        for(int n=0;n<500;++n)near(hit->audio.getSample(0,n),repeated->audio.getSample(0,n),"Attack did not reset per note");
        for(double rate:{8000.0,44100.0,96000.0})
        {
            controls.sound.attackTone=16000;auto high=render(rate);
            check(std::isfinite(high->peak) && high->peak<1.3f,"Attack exceeded its finite gain bound");
        }
        controls.sound.attackTone=1800;
    }
    check(std::abs(signatures[0]-signatures[1])>0.1f && std::abs(signatures[1]-signatures[2])>0.1f,"Attack types are indistinguishable");
    controls.sound.attackType=1;controls.sound.attackDecay=100;auto longer=render();
    check(std::abs(longer->audio.getSample(0,1001)-stereo->audio.getSample(0,1001))>0.005f,"Attack decay does not extend transient");
    controls.sound.attackTone=500;auto lower=render();check(std::abs(lower->audio.getSample(0,31)-longer->audio.getSample(0,31))>0.01f,"Attack tone has no effect");
    controls.sound.sendA=-6.020599913;auto send=render(48000,0);auto dry=render();
    for(int n=0;n<400;++n)near(send->audio.getSample(0,n),dry->audio.getSample(0,n)*0.5f,"Send omitted attack layer");
    controls.mute=true;check(render()->peak==0,"Mute left attack audible");controls.mute=false;
    controls.sound.attackLevel=-60;auto silent=render();near(silent->audio.getSample(0,31),stereo->audio.getSample(0,31),"Attack minimum is not silent");

    // Recall, migration and copies must retain each instrument's independent settings.
    DrumAudioProcessor processor;auto& library=processor.getLibrary();auto state=library.snapshot();auto kit=state.activeKit,group=drum::activeKit(state)->groups.front().id;
    auto id=library.addInstrument(kit,group).id;auto second=library.addInstrument(kit,group).id;
    controls={};controls.sound.mono=1;controls.sound.attackOn=1;controls.sound.attackType=2;controls.sound.attackLevel=-7;controls.sound.attackTone=3000;controls.sound.attackDecay=25;controls.sound.cutoff=900;
    check(library.setControls(kit,id,controls).ok,"New sound controls rejected");
    juce::MemoryBlock saved;processor.getStateInformation(saved);DrumAudioProcessor restored;restored.setStateInformation(saved.getData(),(int)saved.getSize());state=restored.getLibrary().snapshot();
    check(drum::findInstrument(*drum::activeKit(state),id)->controls==controls,"Mono/attack recall lost fields");
    check(drum::findInstrument(*drum::activeKit(state),second)->controls.sound==drum::SoundParameters{},"New controls leaked to another instrument");
    auto copy=library.duplicateInstrument(kit,id);check(copy.ok,"Instrument duplication failed");state=library.snapshot();
    check(drum::findInstrument(*drum::activeKit(state),copy.id)->controls==controls,"Copy omitted mono/attack");
    auto xml=library.toXml();auto* instrument=xml->getChildByName("Kit")->getChildByName("Group")->getChildByName("Instrument");
    const auto before=restored.getLibrary().toXml()->toString();
    for(auto key:{"edit_mono","edit_attackOn","edit_attackType","edit_attackDecay"})
    {
        const auto value=instrument->getStringAttribute(key);instrument->setAttribute(key,"nan");
        check(!restored.getLibrary().restore(*xml),"Invalid mono/attack accepted");instrument->removeAttribute(key);
        check(!restored.getLibrary().restore(*xml),"Missing mono/attack accepted");instrument->setAttribute(key,value);
    }
    instrument->setAttribute("edit_mono",0.5);check(!restored.getLibrary().restore(*xml),"Fractional channel choice accepted");
    check(before==restored.getLibrary().toXml()->toString(),"Rejected restore replaced current kit");
    xml=library.toXml();xml->setAttribute("schemaVersion",5);
    for(auto* k:xml->getChildIterator())for(auto* g:k->getChildIterator())for(auto* i:g->getChildIterator())
        for(auto& p:drum::soundParameters)if(drum::introducedInVersion6(p))i->removeAttribute(juce::String("edit_")+p.key);
    check(restored.getLibrary().restore(*xml),"Schema 5 migration failed");state=restored.getLibrary().snapshot();
    const auto migrated=drum::findInstrument(*drum::activeKit(state),id)->controls;
    check(migrated.sound.mono==0 && migrated.sound.attackOn==0 && migrated.sound.cutoff==900,"Migration changed old sound settings");

    // Exercise a channel switch on an already sounding voice: five-ms ramp, no retrigger.
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Channel switch.wav");file.getParentDirectory().createDirectory();file.deleteFile();
    auto stream=file.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));check(writer!=nullptr,"Channel fixture writer failed");
    writer->writeFromAudioSampleBuffer(sample->audio,0,sample->audio.getNumSamples());writer.reset();
    auto& engine=processor.getSampleEngine();engine.requestLoad(kit,id,file);check(engine.waitForLoading(10000),"Channel fixture timed out");
    controls={};check(library.setControls(kit,id,controls).ok && engine.waitForLoading(10000),"Channel reset failed");
    engine.prepare(48000);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);engine.process(audio,midi);
    controls.sound.mono=1;check(library.setControls(kit,id,controls).ok && engine.waitForLoading(10000),"Channel change failed");midi.clear();engine.process(audio,midi);
    near(audio.getSample(0,300),0.375f,"Sounding voice did not become mono");
    check(std::abs(audio.getSample(0,0)-0.25f)<0.001f,"Channel switch is abrupt");
    for(int n=1;n<240;++n)check(std::abs(audio.getSample(0,n)-audio.getSample(0,n-1))<0.001f,"Channel ramp has discontinuities");
    std::cout<<"Mono playback, attack layers, sends, smoothing, recall and schema migration passed\n";
}
