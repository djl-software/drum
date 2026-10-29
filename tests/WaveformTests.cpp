#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SoundEditor.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
void click(juce::Component& root,const char* id)
{
    auto* button=dynamic_cast<juce::TextButton*>(find(root,id));check(button && button->isEnabled(),"Waveform button unavailable");button->onClick();
}
juce::MouseEvent event(juce::Component& component,float fraction,int flags=juce::ModifierKeys::leftButtonModifier)
{
    juce::Point<float> point(10+fraction*(component.getWidth()-20),60);
    auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(flags),1,0,0,0,0,&component,&component,now,point,now,1,false};
}
void capture(juce::Component& component,const char* name,float scale=1)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);
    check(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,scale),out),"Waveform screenshot failed");
}
juce::File fixture(int frames=48000,int channels=2)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Stereo percussion "+juce::String(frames)+".wav");
    file.getParentDirectory().createDirectory();file.deleteFile();
    auto stream=file.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,(unsigned)channels,24,{},0));
    check(writer!=nullptr,"Waveform fixture writer failed");
    juce::AudioBuffer<float> data(channels,frames);data.clear();
    for(int channel=0;channel<channels;++channel)
        for(int n=0;n<frames;++n)
        {
            double value=0;
            for(double onset:{0.01,0.37,0.61,0.84})
            {
                double t=n/48000.0-onset;
                if(t>=0) value+=0.65*std::sin(juce::MathConstants<double>::twoPi*((70+channel*23)*t+8*(1-std::exp(-35*t))))*std::exp(-t*18)
                             +0.15*std::sin(juce::MathConstants<double>::twoPi*(1900+channel*370)*t)*std::exp(-t*80);
            }
            data.setSample(channel,n,(float)value);
        }
    check(writer->writeFromAudioSampleBuffer(data,0,frames),"Waveform fixture write failed");return file;
}
}
void runWaveformTests()
{
    {
        juce::AudioBuffer<float> audio(2,128);audio.clear();
        audio.setSample(0,40,0.5f);audio.setSample(1,40,0.5f);
        drum::WaveformData waveform;waveform.build(audio);
        check(waveform.channelsAreIdentical(),"Identical stereo channels not recognised as mono for display");
        audio.setSample(1,40,-0.5f);waveform.build(audio);
        check(!waveform.channelsAreIdentical(),"Opposite-polarity channels incorrectly collapsed");
        audio.copyFrom(1,0,audio,0,0,128);audio.setSample(1,127,0.000001f);waveform.build(audio);
        check(!waveform.channelsAreIdentical(),"Small stereo difference incorrectly collapsed");
        audio.setSize(1,128,true);waveform.build(audio);
        check(waveform.channelsAreIdentical(),"Mono source did not use a single display lane");
    }
    // Query against raw audio, including non-aligned ranges, odd block counts,
    // DC, signed impulses and the final partial block. No extrema may disappear.
    juce::Random random(19302);
    for(int frames:{1,31,64,65,127,193,4097,100003})
    {
        juce::AudioBuffer<float> audio(2,frames);
        for(int n=0;n<frames;++n)
        {audio.setSample(0,n,random.nextFloat()*2-1);audio.setSample(1,n,0.25f+random.nextFloat()*0.25f);}
        audio.setSample(0,frames-1,-1);audio.setSample(1,frames/2,1);
        drum::WaveformData waveform;waveform.build(audio);
        for(int iteration=0;iteration<300;++iteration)
        {
            int begin=iteration==0?0:random.nextInt(frames),end=iteration==0?frames:begin+1+random.nextInt(frames-begin);
            for(int channel=0;channel<2;++channel)
            {
                float minimum=audio.getSample(channel,begin),maximum=minimum;
                for(int n=begin;n<end;++n) {minimum=std::min(minimum,audio.getSample(channel,n));maximum=std::max(maximum,audio.getSample(channel,n));}
                auto range=waveform.range(audio,channel,begin,end);
                check(range.minimum==minimum && range.maximum==maximum,"Cached waveform lost signed extrema");
            }
        }
        check(waveform.range(audio,0,frames,frames).maximum==0,"Empty waveform range invalid");
    }
    {
        juce::AudioBuffer<float> audio(1,11520000);audio.clear();
        audio.setSample(0,65,1);audio.setSample(0,66,-0.75f);audio.setSample(0,audio.getNumSamples()-1,-1);
        drum::WaveformData waveform;waveform.build(audio);
        auto range=waveform.range(audio,0,0,audio.getNumSamples());
        check(range.minimum==-1 && range.maximum==1,"Long sample overview lost impulses");
        check(waveform.range(audio,0,65,66).minimum==1 && waveform.range(audio,0,66,67).maximum==-0.75f,"Close zoom cannot resolve adjacent samples");
        check(waveform.bytes()<audio.getNumSamples()*sizeof(float)*0.07,"Waveform cache exceeds memory budget");
    }
    DrumAudioProcessor processor;
    auto& library=processor.getLibrary();auto& engine=processor.getSampleEngine();
    auto state=library.snapshot();auto kit=state.activeKit,group=drum::activeKit(state)->groups.front().id;
    auto added=library.addInstrument(kit,group);check(added.ok,"Waveform instrument failed");
    engine.requestLoad(kit,added.id,fixture());check(engine.waitForLoading(10000),"Waveform sample timed out");
    auto source=engine.waveformFor(added.id);check(source && source->audio.getNumChannels()==2,"UI source unavailable");
    DrumAudioProcessorEditor editor(processor);editor.setSize(1280,900);click(editor,"open-sound-editor");
    auto* soundEditor=dynamic_cast<SoundEditor*>(find(editor,"sound-editor"));check(soundEditor!=nullptr,"Sound editor missing");
    auto select=[&]{state=library.snapshot();soundEditor->select(kit,drum::findInstrument(*drum::activeKit(state),added.id));};
    select();auto* waveform=find(editor,"sample-waveform");check(waveform!=nullptr,"Waveform missing");
    auto* view=dynamic_cast<juce::ComboBox*>(find(editor,"waveform-view"));check(view!=nullptr,"Waveform view selector missing");view->setSelectedId(1,juce::sendNotificationSync);
    auto start=[&]{state=library.snapshot();return drum::findInstrument(*drum::activeKit(state),added.id)->controls.sound.start;};
    auto reset=[&]{check(library.setControls(kit,added.id,{}).ok,"Waveform trim reset failed");select();};
    capture(editor,"drum-waveform-fit.png");
    click(editor,"waveform-zoom-in");click(editor,"waveform-zoom-in");
    waveform->mouseDown(event(*waveform,0.25f));
    check(std::abs(start()-0.4375)<0.00001,"Zoomed marker uses wrong source position");
    reset();
    juce::MouseWheelDetails pan{};pan.deltaY=0.5f;
    waveform->mouseWheelMove(event(*waveform,0.25f),pan);
    waveform->mouseDown(event(*waveform,0.25f));
    check(std::abs(start()-0.3125)<0.00001,"Scrolled marker uses wrong source position");
    reset();click(editor,"waveform-fit");
    juce::MouseWheelDetails wheel{};wheel.deltaY=1;
    waveform->mouseWheelMove(event(*waveform,0.25f,juce::ModifierKeys::ctrlModifier),wheel);
    waveform->mouseDown(event(*waveform,0.25f));
    check(std::abs(start()-0.25)<0.00001,"Wheel zoom lost pointer anchor");
    reset();click(editor,"waveform-fit");
    auto* vertical=dynamic_cast<juce::ComboBox*>(find(editor,"waveform-vertical-zoom"));check(vertical!=nullptr,"Vertical zoom missing");
    const auto initialY=vertical->getSelectedId();
    waveform->mouseWheelMove(event(*waveform,0.25f,juce::ModifierKeys::shiftModifier),wheel);
    check(vertical->getSelectedId()==initialY+1,"Shift-wheel did not zoom vertically");
    waveform->mouseDown(event(*waveform,0.25f));check(std::abs(start()-0.25)<0.00001,"Vertical zoom moved horizontal position");
    reset();click(editor,"waveform-fit");
    // Inspect a transient, rather than the quiet tail at the middle of the file.
    juce::MouseWheelDetails detailZoom{};detailZoom.deltaY=(float)(std::log(128.0)/(5*std::log(1.4)));
    waveform->mouseWheelMove(event(*waveform,0.38f,juce::ModifierKeys::ctrlModifier),detailZoom);
    capture(editor,"drum-waveform-detail.png");
    editor.setSize(1920,1200);capture(*waveform,"drum-waveform-hidpi.png",2);
    editor.setSize(800,600);waveform->mouseDown(event(*waveform,0.25f));
    check(std::abs(start()-(0.38-0.13/128))<0.00001,"Resize lost zoomed source coordinates");
    reset();click(editor,"waveform-fit");capture(editor,"drum-waveform-compact.png");
    click(editor,"waveform-fit");waveform->mouseDown(event(*waveform,0.2f));
    check(std::abs(start()-0.2)<0.00001,"Fit or resize broke marker mapping");
    reset();for(int n=0;n<20;++n)click(editor,"waveform-zoom-in");
    waveform->mouseDown(event(*waveform,0.25f));
    check(std::abs(start()-(0.5-8.0/48000))<0.00001,"Deep zoom still limited to coarse overview");
    reset();
    // Replacing a sample on the same instrument must reset the old view, while
    // retained UI data stays valid if the worker retires the old playback bank.
    engine.requestLoad(kit,added.id,fixture(777,1));check(engine.waitForLoading(10000),"Waveform replacement timed out");select();
    waveform->mouseDown(event(*waveform,0.25f));check(std::abs(start()-0.25)<0.00001,"Sample replacement kept stale zoom");
    check(source->audio.getNumSamples()==48000 && source->waveform.range(source->audio,0,0,48000).maximum>0.5f,"Retired UI sample became invalid");
    engine.requestLoad(kit,added.id,fixture(1,1));check(engine.waitForLoading(10000),"One-frame sample timed out");select();
    click(editor,"waveform-zoom-in");capture(*waveform,"drum-waveform-one-frame.png");
    std::cout<<"Waveform extrema, stereo, long samples, zoom, pan, trim, resizing and replacement tests passed\n";
}
