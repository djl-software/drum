#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SoundEditor.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
void near(float a,float b,const char* message) {check(std::abs(a-b)<0.00003f,message);}
juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;
    return nullptr;
}
template<class T>T& control(juce::Component& root,const char* id)
{
    auto* result=dynamic_cast<T*>(find(root,id));check(result!=nullptr,"Preview UI control missing");return *result;
}
void click(juce::Component& root,const juce::String& id) {control<juce::TextButton>(root,id.toRawUTF8()).onClick();}
void settle(juce::Component& root)
{
    auto start=juce::Time::getMillisecondCounterHiRes();
    while(juce::Time::getMillisecondCounterHiRes()-start<10000)
    {
        juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();
        if(juce::Time::getMillisecondCounterHiRes()-start>150 && control<juce::Label>(root,"waveform-status").getText().startsWith("One hit"))return;
    }
    throw std::runtime_error("Edited waveform did not update");
}
uint64_t pixels(juce::Component& root)
{
    auto& wave=control<juce::Component>(root,"sample-waveform");
    auto image=wave.createComponentSnapshot({10,26,wave.getWidth()-20,wave.getHeight()-58});
    uint64_t result=0;
    for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)result=result*31+image.getPixelAt(x,y).getARGB();
    return result;
}
void capture(juce::Component& component,const juce::String& name)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);
    check(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()),out),"Preview capture failed");
}
}
void runPreviewTests()
{
    // Load a real decoded fixture through the same bank used for audible tests.
    DrumAudioProcessor processor;auto& library=processor.getLibrary();auto& engine=processor.getSampleEngine();
    auto state=library.snapshot();auto kit=state.activeKit,group=drum::activeKit(state)->groups.front().id;
    auto id=library.addInstrument(kit,group).id;
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Preview percussion.wav");
    file.getParentDirectory().createDirectory();file.deleteFile();auto stream=file.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));
    check(writer!=nullptr,"Preview fixture writer failed");
    juce::AudioBuffer<float> audio(2,48000);
    for(int c=0;c<2;++c)for(int n=0;n<48000;++n)
    {
        const double t=n/48000.0;
        audio.setSample(c,n,(float)((0.5*std::sin(juce::MathConstants<double>::twoPi*(110+c*30)*t)+0.15*std::sin(juce::MathConstants<double>::twoPi*8000*t))*std::exp(-t*2)));
    }
    writer->writeFromAudioSampleBuffer(audio,0,48000);writer.reset();
    engine.requestLoad(kit,id,file);check(engine.waitForLoading(10000),"Preview fixture load failed");
    auto source=engine.waveformFor(id);check(source!=nullptr,"Preview source missing");
    drum::InstrumentControls controls;
    std::array<float,drum::numMacros> macros;macros.fill(0.5f);
    auto render=[&](int send=-1){auto result=drum::SampleEngine::renderPreview(source,controls,macros,123,48000,send);check(result.sample!=nullptr,"Offline preview failed");return result;};
    auto baseline=render();near(baseline.sample->audio.getSample(0,4000),source->audio.getSample(0,4000)*0.8f,"Preview velocity differs from Play");
    check(baseline.sample->audio.getNumSamples()==source->audio.getNumSamples(),"Preview added a frame to the source duration");
    controls.sound.sampleGain=-6.020599913;auto quiet=render();near(quiet.sample->peak,baseline.sample->peak*0.5f,"Gain preview auto-normalised away level change");
    controls={};controls.tuning=12;auto pitched=render();check(std::abs(pitched.sample->audio.getNumSamples()*2-baseline.sample->audio.getNumSamples())<=2,"Tuning preview duration incorrect");
    controls={};controls.sound.mode=2;controls.sound.ampAttack=100;auto attack=render();check(attack.sample->audio.getMagnitude(0,500)<baseline.sample->audio.getMagnitude(0,500)*0.12f,"Attack not reflected in preview");
    controls={};controls.sound.filterOn=1;controls.sound.filterType=0;controls.sound.cutoff=40;controls.sound.filterSlope=1;
    auto filtered=render();check(filtered.sample->audio.getRMSLevel(0,4000,10000)<baseline.sample->audio.getRMSLevel(0,4000,10000)*0.1f,"Filter not reflected in preview");
    controls={};controls.sound.lfoOn=1;controls.sound.lfoShape=2;controls.sound.lfoPhase=180;controls.sound.lfoVolume=1;controls.sound.lfoRate=0.01;
    check(render().sample->peak==0,"LFO not reflected in preview");
    controls={};controls.macros[0]={1,-12,0};macros[0]=0;
    auto macroQuiet=render();macros[0]=1;auto macroLoud=render();check(macroLoud.sample->peak>macroQuiet.sample->peak*3.9f,"Macro values ignored in preview");
    controls={};controls.sound.sendA=-6.020599913;auto sent=render(0);near(sent.sample->peak,baseline.sample->peak*0.5f,"Send preview level incorrect");
    check(render(1).sample->peak==0,"Disabled send preview is not silent");
    controls.mute=true;check(render().sample->peak==0,"Muted preview is not silent");
    controls={};int checks=0;
    auto cancelled=drum::SampleEngine::renderPreview(source,controls,macros,120,48000,-1,[&]{return ++checks>3;});
    check(!cancelled.sample && checks>3,"Superseded preview did not cancel");
    auto longSource=std::make_shared<drum::Sample>();longSource->rate=48000;longSource->peak=source->peak;longSource->audio.setSize(2,96000);
    for(int c=0;c<2;++c)for(int offset:{0,48000})longSource->audio.copyFrom(c,offset,source->audio,c,0,48000);
    longSource->waveform.build(longSource->audio);
    controls.sound.mode=2;controls.sound.loop=1;controls.tuning=-48;controls.sound.ampRelease=30000;
    auto bounded=drum::SampleEngine::renderPreview(longSource,controls,macros,120,8000);
    check(bounded.sample && bounded.truncated && bounded.sample->audio.getNumSamples()==240000,"Long release preview is not bounded and labelled");

    // Compare the preview sample-for-sample with actual audition output across
    // all DSP stages, macro mapping and auxiliary routing at multiple host rates.
    controls={};controls.volumeDb=-3;controls.pan=0.2;controls.tuning=-3;
    controls.sound.start=0.03;controls.sound.end=0.8;controls.sound.reverse=1;controls.sound.snap=1;controls.sound.normalise=1;
    controls.sound.mode=2;controls.sound.ampAttack=7;controls.sound.pitchAmount=3;controls.sound.pitchDecay=400;
    controls.sound.filterOn=1;controls.sound.cutoff=1300;controls.sound.filterAmount=2;controls.sound.filterAttack=10;controls.sound.drive=2;
    controls.sound.lfoOn=1;controls.sound.lfoSync=1;controls.sound.lfoVolume=0.3;controls.sound.lfoPitch=0.5;controls.sound.lfoPan=0.2;
    controls.sound.mono=1;controls.sound.attackOn=1;controls.sound.attackType=2;controls.sound.attackLevel=-6;controls.sound.attackDecay=45;
    controls.sound.output=4;controls.sound.sendA=-9;controls.macros[0]={4,-3,3};macros[0]=0.25;
    check(library.setControls(kit,id,controls).ok,"Preview comparison controls invalid");check(engine.waitForLoading(10000),"Preview comparison bank timed out");
    for(double rate:{44100.0,48000.0,96000.0})
    {
        engine.prepare(rate);engine.setTempo(123);engine.setMacros(macros);engine.audition(21);
        auto expected=drum::SampleEngine::renderPreview(source,controls,macros,123,rate);
        juce::AudioBuffer<float> output(2,257);juce::MidiBuffer midi;
        for(int block=0;block<20;++block)
        {
            engine.process(output,midi);
            for(int c=0;c<2;++c)for(int n=0;n<257;++n)near(output.getSample(c,n),expected.sample->audio.getSample(c,block*257+n),"Edited waveform differs from real audition DSP");
        }
    }
    engine.release();
    {
        drum::SoundPreview worker;drum::PreviewInput input;input.source=source;input.macros=macros;
        for(int n=0;n<30;++n) {input.controls.sound.sampleGain=-n*0.5;worker.request(input);}
        input.controls.sound.sampleGain=-6.020599913;input.rate=48000;worker.request(input);
        auto start=juce::Time::getMillisecondCounterHiRes();
        while(worker.pending() && juce::Time::getMillisecondCounterHiRes()-start<10000)juce::Thread::sleep(5);
        check(!worker.pending() && worker.latest().sample,"Latest preview failed to publish");
        near(worker.latest().sample->peak,baseline.sample->peak*0.5f,"Stale slider result replaced latest preview");
        input.source.reset();worker.request(input);
        while(worker.pending())juce::Thread::sleep(5);
        check(!worker.latest().sample,"Removed source retained stale preview");
    }
    controls={};controls.sound.mode=2;check(library.setControls(kit,id,controls).ok,"UI preview controls failed");check(engine.waitForLoading(10000),"UI preview bank timed out");
    for(int n=0;n<drum::numMacros;++n)processor.macro(n).setValueNotifyingHost(0.5f);
    engine.prepare(48000);
    DrumAudioProcessorEditor editor(processor);editor.setSize(1280,900);click(editor,"open-sound-editor");settle(editor);
    auto initial=pixels(editor);
    auto& wave=control<juce::Component>(editor,"sample-waveform");auto bounds=wave.getBoundsInParent();
    for(int page=0;page<7;++page)
    {
        click(editor,"edit-tab-"+juce::String(page));
        check(wave.isVisible() && wave.getBoundsInParent()==bounds,"Waveform moved or disappeared between tabs");
        check(pixels(editor)==initial,"Tab switch changed preview signal");
        capture(editor,"drum-preview-tab-"+juce::String(page)+".png");
    }
    click(editor,"edit-tab-0");control<juce::ComboBox>(editor,"edit-mono").setSelectedId(2,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=initial,"Mono playback did not update waveform");capture(editor,"drum-source-channels.png");
    control<juce::ComboBox>(editor,"edit-mono").setSelectedId(1,juce::sendNotificationSync);settle(editor);
    click(editor,"edit-tab-6");control<juce::ComboBox>(editor,"edit-attackOn").setSelectedId(2,juce::sendNotificationSync);
    control<juce::Slider>(editor,"edit-attackLevel").setValue(-3,juce::sendNotificationSync);
    control<juce::Slider>(editor,"edit-attackDecay").setValue(40,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=initial,"Attack controls did not update waveform");capture(editor,"drum-attack-layer.png");
    editor.setSize(800,600);capture(editor,"drum-attack-compact.png");editor.setSize(1280,900);
    control<juce::ComboBox>(editor,"edit-attackOn").setSelectedId(1,juce::sendNotificationSync);settle(editor);
    click(editor,"edit-tab-3");control<juce::ComboBox>(editor,"edit-lfoOn").setSelectedId(2,juce::sendNotificationSync);
    control<juce::Slider>(editor,"edit-lfoVolume").setValue(90,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=initial,"LFO controls did not update waveform");
    control<juce::ComboBox>(editor,"edit-lfoOn").setSelectedId(1,juce::sendNotificationSync);
    click(editor,"edit-tab-1");control<juce::Slider>(editor,"edit-pitchAmount").setValue(12,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=initial,"Pitch envelope did not update waveform");
    auto& view=control<juce::ComboBox>(editor,"waveform-view");view.setSelectedId(1,juce::sendNotificationSync);auto sourcePixels=pixels(editor);
    control<juce::Slider>(editor,"edit-pitchAmount").setValue(0,juce::sendNotificationSync);
    check(pixels(editor)==sourcePixels,"Pitch changed Source waveform");view.setSelectedId(2,juce::sendNotificationSync);settle(editor);
    click(editor,"edit-tab-1");control<juce::Slider>(editor,"edit-ampAttack").setValue(200,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=initial,"Envelope UI did not update waveform");capture(editor,"drum-preview-envelope.png");
    click(editor,"edit-tab-2");auto before=pixels(editor);
    control<juce::ComboBox>(editor,"edit-filterOn").setSelectedId(2,juce::sendNotificationSync);
    control<juce::Slider>(editor,"edit-cutoff").setValue(90,juce::sendNotificationSync);settle(editor);
    check(pixels(editor)!=before,"Filter UI did not update waveform");capture(editor,"drum-preview-filter.png");
    // Scroll the controls while the waveform remains pinned outside the viewport.
    auto* viewport=dynamic_cast<juce::Viewport*>(find(editor,"sound-controls"));check(viewport!=nullptr,"Control viewport missing");
    viewport->setViewPosition(0,300);check(wave.getBoundsInParent()==bounds,"Controls scrolling moved waveform");
    click(editor,"edit-tab-5");control<juce::ComboBox>(editor,"macro-target-0").setSelectedId(2,juce::sendNotificationSync);settle(editor);
    before=pixels(editor);processor.macro(0).setValueNotifyingHost(0.9f);settle(editor);
    check(pixels(editor)!=before,"Host macro change did not update waveform");
    editor.setSize(800,600);capture(editor,"drum-preview-compact.png");
    check(wave.isVisible() && viewport->getHeight()>150,"Compact preview leaves no usable control area");
    // Viewing changes must magnify quiet signals without changing audio/state.
    editor.setSize(1280,900);controls={};controls.volumeDb=-24;
    check(library.setControls(kit,id,controls).ok,"Quiet preview controls failed");check(engine.waitForLoading(10000),"Quiet preview bank timed out");
    state=library.snapshot();control<SoundEditor>(editor,"sound-editor").select(kit,drum::findInstrument(*drum::activeKit(state),id));
    click(editor,"edit-tab-1");settle(editor);
    const auto savedState=library.toXml()->toString();
    before=pixels(editor);capture(editor,"drum-waveform-quiet-before.png");
    auto& vertical=control<juce::ComboBox>(editor,"waveform-vertical-zoom");
    vertical.setSelectedId(6,juce::sendNotificationSync);
    check(pixels(editor)!=before,"Vertical zoom did not magnify quiet waveform");
    click(editor,"waveform-fit-vertical");const int fitted=vertical.getSelectedId();
    check(fitted>3,"Fit Y did not enlarge a quiet preview");
    capture(editor,"drum-waveform-quiet-fit.png");
    for(int tab=0;tab<7;++tab) {click(editor,"edit-tab-"+juce::String(tab));check(vertical.getSelectedId()==fitted,"Tab change reset vertical zoom");}
    auto& channels=control<juce::ComboBox>(editor,"waveform-channels");
    channels.setSelectedId(3,juce::sendNotificationSync);auto leftView=pixels(editor);
    channels.setSelectedId(4,juce::sendNotificationSync);check(pixels(editor)!=leftView,"Right channel view matches the different left signal");
    click(editor,"edit-tab-2");capture(editor,"drum-waveform-right.png");
    channels.setSelectedId(1,juce::sendNotificationSync);
    auto& divider=control<juce::Component>(editor,"waveform-divider");
    auto dragEvent=[&](int distance) {
        auto now=juce::Time::getCurrentTime();juce::Point<float> down(50,6),position(50,(float)(6+distance));
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&divider,&divider,now,down,now,1,distance!=0);
    };
    editor.setSize(1920,1200);const int previousHeight=wave.getHeight();
    divider.mouseDown(dragEvent(0));divider.mouseDrag(dragEvent(140));
    check(wave.getHeight()>previousHeight && viewport->getHeight()>=160,"Waveform height divider failed");
    editor.setSize(800,600);check(viewport->getHeight()>=160,"Tall waveform obscures compact controls");capture(editor,"drum-waveform-zoom-compact.png");
    divider.mouseDoubleClick(dragEvent(0));editor.setSize(1280,900);check(wave.getHeight()==280,"Waveform default height not restored");
    check(library.toXml()->toString()==savedState,"Waveform display controls changed instrument state");
    control<juce::Slider>(editor,"edit-sampleGain").setValue(-6,juce::sendNotificationSync);settle(editor);
    check(vertical.getSelectedId()==fitted,"A sound edit automatically rescaled the waveform");
    // Identical stereo output should fill one lane in Auto, without summing audio.
    auto monoFile=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Preview mono.wav");monoFile.deleteFile();
    auto monoStream=monoFile.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> monoWriter(format.createWriterFor(monoStream.release(),48000,1,24,{},0));
    check(monoWriter!=nullptr,"Mono preview writer failed");monoWriter->writeFromAudioSampleBuffer(audio,0,48000);monoWriter.reset();
    engine.requestLoad(kit,id,monoFile);check(engine.waitForLoading(10000),"Mono preview load failed");
    state=library.snapshot();control<SoundEditor>(editor,"sound-editor").select(kit,drum::findInstrument(*drum::activeKit(state),id));settle(editor);
    click(editor,"waveform-fit-vertical");auto monoView=pixels(editor);capture(editor,"drum-waveform-mono.png");
    channels.setSelectedId(2,juce::sendNotificationSync);check(pixels(editor)!=monoView,"Auto mono display still draws duplicate stereo lanes");
    click(editor,"close-sound-editor");
    std::cout<<"Processed waveform DSP equivalence, bounds, cancellation, all tabs, scrolling and live control updates passed\n";
}
