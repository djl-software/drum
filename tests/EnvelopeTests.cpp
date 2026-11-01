#include "LegacyTestState.h"
#include "PluginEditor.h"
#include "SoundEditor.h"
#include "VoiceEnvelope.h"
#include "TonalAnalysis.h"
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool v,const char* message) {if(!v)throw std::runtime_error(message);}
void near(double a,double b,double tolerance,const char* message){check(std::abs(a-b)<tolerance,message);}
juce::Component* find(juce::Component& root,const juce::String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
template<class T>T& control(juce::Component& root,const char* id)
{auto* c=dynamic_cast<T*>(find(root,id));check(c!=nullptr,"Envelope control missing");return *c;}
void click(juce::Component& root,const char* id) {control<juce::TextButton>(root,id).onClick();}
juce::MouseEvent event(juce::Component& c,juce::Point<float> point,juce::Point<float> down,int mods=juce::ModifierKeys::leftButtonModifier,int heldMs=0)
{
    auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,now,down,now-juce::RelativeTime::milliseconds(heldMs),1,point!=down};
}
void drag(juce::Component& c,juce::Point<float> delta)
{
    const auto down=c.getLocalBounds().getCentre().toFloat(),target=c.getPosition().toFloat()+down+delta;
    c.mouseDown(event(c,down,down));c.mouseDrag(event(c,target-c.getPosition().toFloat(),down));c.mouseUp(event(c,target-c.getPosition().toFloat(),down));
}
uint64_t pixels(juce::Component& c)
{
    auto image=c.createComponentSnapshot(c.getLocalBounds());uint64_t hash=0;
    for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)hash=hash*31+image.getPixelAt(x,y).getARGB();
    return hash;
}
void capture(juce::Component& c,const char* name)
{
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);
    check(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),out),"Envelope screenshot failed");
}
void settle(juce::Component& root)
{
    const auto start=juce::Time::getMillisecondCounterHiRes();
    while(juce::Time::getMillisecondCounterHiRes()-start<10000)
    {
        juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();
        if(juce::Time::getMillisecondCounterHiRes()-start>200 && (control<juce::Label>(root,"waveform-status").getText().startsWith("One hit") || control<juce::Label>(root,"waveform-status").getText().startsWith("Source")))return;
    }
    throw std::runtime_error("Envelope preview timed out");
}
void shapeTests()
{
    for(double a:{0.0,0.25,1.0})for(double b:{0.0,0.7,1.0})for(double t:{0.02,0.25,0.5,0.8,0.98})for(double level:{0.0,0.05,0.5,0.95,1.0})
    {
        drum::EnvelopePoint right{100,b};drum::bendEnvelopeSegment(a,right,t,level);
        near(drum::envelopeSegment(a,right,t),level,0.001,"Curve does not follow the grabbed level");
        drum::EnvelopeShape shape;shape.count=2;shape.points[0]={0,a};shape.points[1]=right;
        check(drum::validEnvelope(shape),"Weighted curve is invalid");
        for(double fraction:{0.1,0.43,0.9})
        {
            auto split=shape;check(drum::insertEnvelopePoint(split,1,100*fraction),"Cannot insert point into weighted curve");
            for(int ms=0;ms<=100;++ms)
            {
                const auto value=drum::envelopeValue(shape,ms);check(value>=-1e-9 && value<=1+1e-9,"Curve exceeds envelope level limits");
                near(value,drum::envelopeValue(split,ms),1e-5,"Adding point changes weighted curve");
            }
        }
    }
    for(double pivot:{0.08,0.3,0.5,0.92})for(double bend:{-0.3,0.0,0.4})
    {
        auto s=drum::adsrShape(100,200,0.2,300);s.points[2].pivot=pivot;s.points[2].bend=bend;
        for(double fraction:{0.01,0.23,0.8,0.99})
        {
            auto split=s;check(drum::insertEnvelopePoint(split,2,100+fraction*200) && drum::validEnvelope(split),"Curve insertion invalid");
            for(int ms=0;ms<600;++ms)near(drum::envelopeValue(s,ms),drum::envelopeValue(split,ms),1e-7,"Adding point changes existing curve");
        }
    }
    auto full=drum::adsrShape(100,200,0.2,300);
    while(full.count<32)check(drum::insertEnvelopePoint(full,full.count-1,full.points[full.count-2].ms+5),"Cannot fill envelope");
    check(!drum::insertEnvelopePoint(full,1,50),"Envelope exceeds 32 points");
    auto invalid=full;invalid.points[1].ms=-1;check(!drum::validEnvelope(invalid),"Negative time accepted");

    for(double rate:{44100.0,48000.0,96000.0})
    {
        drum::VoiceEnvelope env;env.setSampleRate(rate);env.setParameters({0.1f,0.2f,0.25f,0.3f});
        auto s=drum::adsrShape(100,200,0.25,300);s.points[1].bend=-0.2;env.setShape(s);env.noteOn();
        float last=0;
        for(int n=0;n<(int)(rate*0.05);++n)last=env.getNextSample();
        const double releaseFrom=last;env.noteOff();
        for(int n=1;n<=(int)(rate*0.3);++n)
            near(env.getNextSample(),releaseFrom*(1-n/(rate*0.3)),0.0001,"Early release does not start at current level");
        for(int n=0;n<(int)(rate*0.004);++n)env.getNextSample();
        check(!env.isActive(),"Envelope never ends");
        env.reset();env.noteOn();
        for(int n=0;n<(int)(rate*0.8);++n)last=env.getNextSample();
        near(last,0.25,0.00001,"Envelope did not hold sustain");
        s.points[3].level=0.8;env.setShape(s);env.noteOff();
        for(int n=0;n<(int)(rate*0.31);++n)last=env.getNextSample();
        check(!env.isActive() && last==0,"Nonzero final point leaves stuck voice");
    }
}
void tonalTests()
{
    juce::AudioBuffer<float> audio(2,48000);
    auto tones=[&](std::initializer_list<double> frequencies)
    {
        for(int n=0;n<48000;++n){double v=0;for(double hz:frequencies)v+=0.2*std::sin(juce::MathConstants<double>::twoPi*hz*n/48000);audio.setSample(0,n,(float)v);audio.setSample(1,n,(float)-v);}
    };
    tones({440});auto a=drum::analyseTonal(audio,48000);near(a.midi,69,0.05,"A4 pitch detection failed");check(a.key==-1 && !a.varying,"Single tone invented major/minor");
    check(drum::tileKey(a)=="A (est.)","Tile did not show strongest spectrum note");
    drum::TonalAnalysis minor;minor.complete=true;minor.key=21;minor.noteEnergy[9]=1;
    check(drum::tileKey(minor)=="A (est.)","Minor key tile label incorrect");
    check(drum::tileKey(minor,2)=="B (est.)","Transposed tile label incorrect");
    drum::TonalAnalysis uncertain;uncertain.complete=true;uncertain.noteEnergy[3]=1;uncertain.noteEnergy[9]=.2f;check(drum::tileKey(uncertain)=="D# (est.)","Uncertain tile did not use spectral estimate");check(drum::strongestNote(uncertain,2)==5,"Transposed spectrum estimate incorrect");check(drum::strongestNote(drum::TonalAnalysis{})==-1,"Silence invented a key");
    auto strongest=[](const drum::TonalAnalysis& result){return (int)std::distance(result.noteEnergy.begin(),std::max_element(result.noteEnergy.begin(),result.noteEnergy.end()));};
    check(strongest(a)==9 && a.noteEnergy[9]>.99f,"Key spectrum did not map A4 to A");
    tones({220});check(strongest(drum::analyseTonal(audio,48000))==9,"Key spectrum did not combine octaves");
    tones({261.625565});check(strongest(drum::analyseTonal(audio,48000))==0,"Key spectrum does not start with C");
    tones({440*std::pow(2.0,1.0/12)});check(strongest(drum::analyseTonal(audio,48000))==10,"Transposed tone did not move to A sharp");
    tones({32.703196});check(strongest(drum::analyseTonal(audio,48000))==0,"Low C was omitted from the Key spectrum");
    for(int n=0;n<48000;++n){audio.setSample(0,n,.2f*(float)std::sin(juce::MathConstants<double>::twoPi*440*n/48000));audio.setSample(1,n,.2f*(float)std::sin(juce::MathConstants<double>::twoPi*261.625565*n/48000));}auto stereo=drum::analyseTonal(audio,48000);check(stereo.noteEnergy[0]>.5f && stereo.noteEnergy[9]>.5f,"Key spectrum lost one stereo channel");
    juce::AudioBuffer<float> late(1,480000);late.clear();for(int n=240000;n<288000;++n)late.setSample(0,n,.2f*(float)std::sin(juce::MathConstants<double>::twoPi*440*n/48000));check(drum::analyseTonal(late,48000).noteEnergy[9]>.9f,"Analysis ignored audio beyond four seconds");
    tones({220});near(drum::analyseTonal(audio,48000).midi,57,0.05,"A3 pitch detection failed");
    tones({261.625565,329.627557,391.995436});a=drum::analyseTonal(audio,48000);std::cout<<"C chord key estimate: "<<a.key<<"\n";check(a.key==0,"C major chord recognition failed");
    tones({233.081881,277.182631,349.228231});a=drum::analyseTonal(audio,48000);check(a.key==22,"Bb minor chord recognition failed");
    juce::Random random(123);for(int c=0;c<2;++c)for(int n=0;n<48000;++n)audio.setSample(c,n,random.nextFloat()*0.5f-0.25f);
    a=drum::analyseTonal(audio,48000);check(a.midi<0 && a.key<0,"Noise given a definite pitch/key");check(drum::strongestNote(a)>=0 && drum::tileKey(a).endsWith("(est.)"),"Noise tile did not receive a spectrum estimate");
    audio.clear();a=drum::analyseTonal(audio,48000);check(a.midi<0 && a.key<0 && a.complete,"Silence analysis incorrect");
    check(std::all_of(a.noteEnergy.begin(),a.noteEnergy.end(),[](float v){return v==0;}),"Silence retained a stale Key spectrum");
    check(!drum::analyseTonal(audio,48000,[]{return true;}).complete,"Analysis cancellation ignored");
    check(drum::camelotCode(22)=="3A" && drum::camelotCode(21)=="8A" && drum::camelotCode(0)=="8B","Camelot mapping incorrect");
    check(drum::musicalNote(69)=="A4","Note octave incorrect");
    juce::AudioBuffer<float> padded(1,480000);padded.clear();for(int n=0;n<10000;++n){double t=n/48000.0;padded.setSample(0,n,(float)(.5*std::exp(-12*t)*std::sin(juce::MathConstants<double>::twoPi*(55*t+30*.02*(1-std::exp(-t/.02))))));}auto kick=drum::analyseTonal(padded,48000);check(kick.midi>30 && kick.midi<38,"Padded kick lost its tail pitch");check(drum::strongestNote(kick)>=0,"Pitched kick has no spectrum estimate");

}
}
void runEnvelopeTests()
{
    shapeTests();tonalTests();
    const auto diagnostics=juce::SystemStats::getEnvironmentVariable("DJL_DRUM_ANALYSE_FILES",{});
    if(diagnostics.isNotEmpty()){juce::AudioFormatManager formats;formats.registerBasicFormats();for(const auto& path:juce::StringArray::fromLines(juce::File(diagnostics).loadFileAsString())){std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(path)));if(!reader)continue;juce::AudioBuffer<float> audio((int)reader->numChannels,(int)reader->lengthInSamples);reader->read(&audio,0,audio.getNumSamples(),0,true,true);auto a=drum::analyseTonal(audio,reader->sampleRate);std::cout<<juce::File(path).getFileName()<<": midi="<<a.midi<<" range="<<a.lowMidi<<".."<<a.highMidi<<" key="<<a.key<<" confidence="<<a.confidence<<" spectrum="<<drum::spectrumKey(a)<<"\n";}}

    DrumAudioProcessor p;auto& library=p.getLibrary();auto state=library.snapshot();const auto kit=state.activeKit,group=drum::activeKit(state)->groups.front().id;
    const auto id=library.addInstrument(kit,group).id;
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Preview percussion.wav");
    p.getSampleEngine().requestLoad(kit,id,file);check(p.getSampleEngine().waitForLoading(10000),"Envelope sample load timed out");
    check(p.getSampleEngine().statusFor(id).ready,"Envelope fixture unavailable");check(p.getSampleEngine().waveformFor(id)->tonal.complete,"Imported sample was not analysed");p.getSampleEngine().prepare(48000);
    DrumAudioProcessorEditor editor(p);editor.setVisible(true);editor.setSize(1280,900);capture(editor,"drum-manual-main.png");click(editor,"open-sound-editor");
    check(control<juce::TextButton>(editor,"edit-tab-1").getButtonText()=="Envelopes","Envelope tab name incorrect");
    int lastTabX=-1;for(int tab:{0,1,2,6,3,8,4,5,7}){const int x=control<juce::TextButton>(editor,("edit-tab-"+juce::String(tab)).toRawUTF8()).getX();check(x>lastTabX,"Edit tabs out of order");lastTabX=x;}
    check(!control<juce::Slider>(editor,"edit-start").isShowing() && !control<juce::Slider>(editor,"edit-end").isShowing(),"Separate trim sliders still visible");
    check(control<juce::Component>(editor,"sample-waveform").getHeight()>240,"Sample waveform lost its usable height");check(control<SoundEditor>(editor,"sound-editor").getY()>=control<juce::Component>(editor,"library-menu").getBottom(),"Sound editor overlaps navigation");
    auto sampleField=[&](const char* id){return find(editor,id)->getParentComponent()->getBounds();};
    check(sampleField("edit-sampleGain").getY()==sampleField("edit-normalise").getY() && sampleField("edit-normalise").getX()>sampleField("edit-reverse").getX() && sampleField("edit-snap").getY()>sampleField("edit-normalise").getY(),"Sample controls not four per row");
    const int sampleWaveHeight=control<juce::Component>(editor,"sample-waveform").getHeight();capture(editor,"drum-sample-layout.png");click(editor,"edit-tab-1");settle(editor);check(control<juce::Component>(editor,"sample-waveform").getHeight()==sampleWaveHeight,"Envelope waveform does not match Sample");
    auto current=[&]{auto s=library.snapshot();return drum::findInstrument(*drum::activeKit(s),id)->controls;};
    auto& soundEditor=control<SoundEditor>(editor,"sound-editor");
    click(editor,"edit-tab-8");check(!control<juce::Component>(editor,"sample-waveform").isVisible() && control<juce::Component>(editor,"key-spectrum").getBounds()==control<juce::Component>(editor,"sample-waveform").getBounds(),"Tune did not replace full waveform area");check(control<juce::TextButton>(editor,"edit-tab-8").getButtonText()=="Tune","Tune tab name incorrect");check(control<juce::TextButton>(editor,"edit-tab-8").getX()>control<juce::TextButton>(editor,"edit-tab-2").getX() && control<juce::TextButton>(editor,"edit-tab-8").getX()>control<juce::TextButton>(editor,"edit-tab-3").getX(),"Tune tab order incorrect");check(control<juce::Component>(editor,"key-spectrum").isVisible(),"Key spectrum not visible on Key tab");
    control<juce::Slider>(editor,"key-coarse").setValue(3,juce::sendNotificationSync);control<juce::Slider>(editor,"key-fine").setValue(17.5,juce::sendNotificationSync);
    near(current().tuning,3,.001,"Key coarse did not affect instrument tuning");near(current().sound.detune,17.5,.001,"Key fine did not affect playback detune");settle(editor);capture(editor,"drum-key-tab.png");
    check(find(editor,"auto-tune")==nullptr && find(editor,"target-key")==nullptr,"Auto Tune controls remain");
    juce::MemoryBlock keyState;p.getStateInformation(keyState);DrumAudioProcessor recalledKey;recalledKey.setStateInformation(keyState.getData(),(int)keyState.getSize());auto recalledState=recalledKey.getLibrary().snapshot();auto recalledControls=drum::findInstrument(*drum::activeKit(recalledState),id)->controls;near(recalledControls.tuning,3,.001,"Key tuning was not saved");near(recalledControls.sound.detune,17.5,.001,"Key fine tuning was not saved");
    p.getSampleEngine().audition(21);juce::AudioBuffer<float> cursorAudio(2,4800);juce::MidiBuffer cursorMidi;p.getSampleEngine().process(cursorAudio,cursorMidi);check(p.getSampleEngine().playbackPosition(21).active,"Waveform cursor fixture inactive");capture(editor,"drum-tune-cursor.png");p.getSampleEngine().stop();p.getSampleEngine().process(cursorAudio,cursorMidi);

    click(editor,"reset-edit-page");near(current().tuning,0,.001,"Key reset did not reset coarse");near(current().sound.detune,0,.001,"Key reset did not reset fine");click(editor,"edit-tab-1");
    auto refresh=[&]{auto s=library.snapshot();soundEditor.select(kit,drum::findInstrument(*drum::activeKit(s),id));};
    auto& amp=control<EnvelopeEditor>(editor,"envelope-amp");auto& pitch=control<EnvelopeEditor>(editor,"envelope-pitch");auto& filter=control<EnvelopeEditor>(editor,"envelope-filter");
    check(amp.isVisible() && !pitch.isVisible() && !filter.isVisible(),"Envelope graphs shown on wrong page");
    auto shown=[&](const char* id){for(auto* c=&control<juce::Component>(editor,id);c!=nullptr;c=c->getParentComponent())if(!c->isVisible())return false;return true;};
    check(shown("edit-velocityVolume") && shown("edit-randomPan") && !shown("edit-velocityPitch") && !shown("edit-bendRange") && !shown("edit-detune"),"Amplitude mode shows wrong controls");
    check(control<juce::Slider>(editor,"edit-velocityVolume").getSliderStyle()==juce::Slider::LinearBar,"Amplitude control is not a value bar");
    auto& wave=control<juce::Component>(editor,"sample-waveform");const auto waveBounds=wave.getBounds();
    auto c=current();c.sound.ampAttack=80;c.sound.ampDecay=200;c.sound.ampSustain=0.4;c.sound.ampRelease=200;c.sound.pitchAttack=80;c.sound.pitchDecay=200;c.sound.pitchSustain=0.2;
    check(library.setControls(kit,id,c).ok,"Envelope fixture controls failed");refresh();
    // Click on an actual line point (time 180 in the decay, fitted span 528ms).
    auto onLine=[&](EnvelopeEditor& graph,const drum::EnvelopeShape& shape,int segment,double fraction)
    {
        const auto a=shape.points[segment-1],b=shape.points[segment];const double ms=a.ms+(b.ms-a.ms)*fraction;
        return graph.pointFor({ms,drum::envelopeSegment(a.level,b,fraction)});
    };
    click(editor,"overlay-pitch");settle(editor);check(pitch.isVisible() && !amp.isVisible(),"Pitch overlay selection failed");check(!shown("edit-velocityVolume") && shown("edit-velocityPitch") && shown("edit-bendRange") && shown("edit-randomPan") && !shown("edit-detune"),"Pitch mode shows wrong controls");check(control<juce::Slider>(editor,"envelope-pitch-amount").getSliderStyle()==juce::Slider::LinearBar,"Pitch depth is not a value bar");
    auto line=onLine(pitch,drum::adsrShape(80,200,0.2,100),2,0.5);
    check(editor.getComponentAt(editor.getLocalPoint(&pitch,line.roundToInt()))==&pitch,"Envelope line is intercepted by another component");
    pitch.mouseDown(event(pitch,line,line));pitch.mouseUp(event(pitch,line,line));
    check(current().sound.envelopes[1].count==5 && current().sound.pitchAmount==0,"Click did not add a silent-preserving point");
    const auto beforeWave=pixels(wave);
    drag(control<juce::Component>(editor,"envelope-pitch-point-2"),{18,-15});
    check(current().sound.pitchAmount==12,"Zero pitch depth not made audible");
    settle(editor);check(pixels(wave)!=beforeWave,"Pitch shape did not change waveform");
    auto shaped=current().sound.envelopes[1];check(shaped.points[2].ms>180 && shaped.points[2].level>0.6,"Point cannot move freely in both axes");
    line=onLine(pitch,shaped,2,0.35);
    pitch.mouseDown(event(pitch,line,line));pitch.mouseDrag(event(pitch,line+juce::Point<float>{8,12},line));pitch.mouseUp(event(pitch,line+juce::Point<float>{8,12},line));
    auto curved=current().sound.envelopes[1];check(curved.count==5 && std::abs(curved.points[2].bend)>0.01,"Drag line did not curve without adding point");
    check(curved.points[2].pivot!=0.5,"Curve ignored clicked position");
    settle(editor);capture(editor,"drum-envelope-curves.png");
    auto& point=control<juce::Component>(editor,"envelope-pitch-point-2");const auto centre=point.getLocalBounds().getCentre().toFloat();
    point.mouseDoubleClick(event(point,centre,centre));check(current().sound.envelopes[1].count==4,"Double click did not remove point");
    point.mouseDown(event(point,centre,centre,juce::ModifierKeys::rightButtonModifier));check(current().sound.envelopes[1].hold==2,"Set hold point failed");
    check(!control<juce::Slider>(editor,"edit-pitchAttack").isEnabled(),"Legacy ADSR overwrites custom points");
    control<juce::Slider>(editor,"envelope-pitch-amount").setValue(-7,juce::sendNotificationSync);check(current().sound.pitchAmount==-7,"Graph depth slider failed");
    click(editor,"overlay-amplitude");settle(editor);
    auto& ampHandle=control<juce::Component>(editor,"envelope-amp-point-1");
    check(editor.getComponentAt(editor.getLocalPoint(&ampHandle,ampHandle.getLocalBounds().getCentre()))==&ampHandle,"Amplitude point cannot receive mouse events");
    drag(ampHandle,{10,12});check(current().sound.mode==2 && current().sound.envelopes[0].count==4,"Amplitude drag failed to activate processing");
    auto ampShape=current().sound.envelopes[0];line=onLine(amp,ampShape,2,0.4);
    amp.mouseDown(event(amp,line,line));amp.mouseDrag(event(amp,line+juce::Point<float>{10,-12},line));amp.mouseUp(event(amp,line,line));
    check(current().sound.envelopes[0].count==4 && current().sound.envelopes[0]!=ampShape,"Immediate line drag did not bend amplitude curve");
    ampShape=current().sound.envelopes[0];line=onLine(amp,ampShape,2,0.4);
    amp.mouseDown(event(amp,line,line));amp.mouseUp(event(amp,line,line));check(current().sound.envelopes[0].count==5,"Amplitude line click did not add point");
    drag(control<juce::Component>(editor,"envelope-amp-point-1"),{250,0});
    check(current().sound.envelopes[0].points[1].ms>ampShape.points[1].ms,"Point could not cross neighbour");
    const auto saved=library.toXml()->toString();click(editor,"envelope-amp-zoom-in");click(editor,"envelope-amp-zoom-out");click(editor,"envelope-amp-fit");
    check(library.toXml()->toString()==saved,"View zoom changes sound");
    settle(editor);capture(editor,"drum-envelopes.png");editor.setSize(800,600);capture(editor,"drum-envelopes-compact.png");editor.setSize(1280,900);
    click(editor,"edit-tab-1");click(editor,"filter-envelope-tab");check(filter.isVisible() && !amp.isVisible() && wave.isVisible() && filter.getY()==wave.getY(),"Filter overlay missing from Envelopes");
    c=current();c.sound.filterOn=1;c.sound.cutoff=1500;c.sound.filterAmount=3;c.sound.filterAttack=30;check(library.setControls(kit,id,c).ok,"Filter setup failed");refresh();
    drag(control<juce::Component>(editor,"envelope-filter-point-2"),{30,-10});check(current().sound.envelopes[2].count==4,"Filter graph did not save edits");settle(editor);capture(editor,"drum-filter-envelope.png");
    for(int tab:{0,3,4,5,6}){click(editor,("edit-tab-"+juce::String(tab)).toRawUTF8());settle(editor);capture(editor,("drum-manual-tab-"+juce::String(tab)+".png").toRawUTF8());}
    click(editor,"edit-tab-1");click(editor,"overlay-amplitude");
    c=current();c.sound.keyOverride=23;check(library.setControls(kit,id,c).ok,"Legacy label fixture failed");refresh();settle(editor);
    const auto note=control<juce::Label>(editor,"note-indicator").getText();
    std::array<float,drum::numMacros> displayMacros{};for(int n=0;n<drum::numMacros;++n)displayMacros[(size_t)n]=p.macro(n).get();auto displayPreview=drum::SampleEngine::renderPreview(p.getSampleEngine().waveformFor(id),current(),displayMacros,120,48000);check(displayPreview.sample!=nullptr,"Display preview unavailable");auto displayAnalysis=drum::analyseTonal(displayPreview.sample->audio,48000);
    check(note.contains("Trigger:") && note.contains("MIDI 21") && note.contains("Key (spectrum estimate): "+drum::spectrumKey(displayAnalysis)) && !note.contains("manual"),"Display did not follow highest spectrum note");
    capture(editor,"drum-note-indicator.png");
    c=current();c.macros[0]={7,100,900};check(library.setControls(kit,id,c).ok,"Envelope mapping failed");refresh();
    auto& ampA=control<juce::Component>(editor,"envelope-amp-point-1");check(!ampA.isEnabled(),"Macro-mapped handle editable");
    const auto base=current().sound.envelopes[0];auto macroImage=pixels(amp);p.macro(0).setValueNotifyingHost(0.9f);settle(editor);
    check(pixels(amp)!=macroImage,"Host macro did not redraw envelope");drag(ampA,{30,0});check(current().sound.envelopes[0]==base,"Mapped envelope overwritten");
    auto baked=c.sound;baked.ampAttack=100.0+800.0*p.macro(0).get();drum::resolveEnvelopeMacros(baked,c.macros);
    click(editor,"envelope-amp-activate");check(current().macros[0].target==0 && current().sound.envelopes[0]==baked.envelopes[0],"Unlink timing failed to preserve effective curve");
    // Reproduce the user's saved Set: Macro 1 -> amp decay at 50%, with Trigger playback.
    c=current();c.sound.mode=0;c.sound.envelopes[0]={};c.sound.ampAttack=0;c.sound.ampDecay=100;c.sound.ampSustain=1;c.sound.ampRelease=100;c.macros[0]={8,0,20000};c.macros[1]={1,-20,0};p.macro(0).setValueNotifyingHost(0.5f);
    check(library.setControls(kit,id,c).ok,"Mapped decay fixture failed");refresh();
    check(!ampA.isEnabled() && control<juce::TextButton>(editor,"envelope-amp-activate").getButtonText()=="Unlink timing","Mapped amplitude has no clear unlock action");
    click(editor,"envelope-amp-activate");check(ampA.isEnabled() && current().macros[0].target==0 && current().macros[1]==c.macros[1] && current().sound.ampDecay==10000 && current().sound.mode==0,"Unlink changed unrelated settings or lost current timing");
    auto source=p.getSampleEngine().waveformFor(id);auto dry=drum::SampleEngine::renderPreview(source,current(),{},120,48000);
    drag(ampA,{60,amp.getPlotBounds().getHeight()*.35f});auto wet=drum::SampleEngine::renderPreview(source,current(),{},120,48000);
    check(current().sound.mode==2 && current().sound.envelopes[0].count==4 && wet.sample && dry.sample && wet.sample->peak<dry.sample->peak*0.8f,"Unlocked amplitude drag did not shape playback");
    check(wave.getX()==amp.getX() && wave.getY()==amp.getY(),"Amplitude is not superimposed on waveform");
    // Each graph has independent, gesture-sized history and obeys the original
    // file's duration even when the playable region has been trimmed much shorter.
    const double fullMs=1000.0*source->audio.getNumSamples()/source->rate;
    for(int n=0;n<3;++n)
    {
        const juce::String prefix=n==0?"envelope-amp":n==1?"envelope-pitch":"envelope-filter";
        click(editor,"edit-tab-1");if(n==2)click(editor,"filter-envelope-tab");else click(editor,n==0?"overlay-amplitude":"overlay-pitch");
        c=current();c.macros={};c.sound.start=0.2;c.sound.end=0.3;c.sound.envelopes[(size_t)n]=drum::adsrShape(100,150,0.4,150);
        check(library.setControls(kit,id,c).ok,"Navigation fixture failed");refresh();
        auto& graph=control<EnvelopeEditor>(editor,prefix.toRawUTF8());
        auto& handle=control<juce::Component>(editor,(prefix+"-point-1").toRawUTF8());
        check(!handle.getBounds().intersects(control<juce::TextButton>(editor,(prefix+"-reset").toRawUTF8()).getBounds()),"Envelope toolbar overlaps top-level point grip");
        const auto before=current();auto anchor=handle.getBounds().getCentre().toFloat();const auto initialX=anchor.x;
        juce::MouseWheelDetails wheel{};wheel.deltaY=0.4f;
        graph.mouseWheelMove(event(graph,anchor,anchor,juce::ModifierKeys::ctrlModifier),wheel);
        check(std::abs(handle.getBounds().getCentreX()-initialX)<2,"Envelope zoom lost pointer anchor");
        wheel.deltaY=-0.15f;graph.mouseWheelMove(event(graph,anchor,anchor,0),wheel);
        check(handle.getBounds().getCentreX()<initialX-5,"Envelope wheel did not pan");
        const auto panned=handle.getBounds().getCentreX();
        graph.mouseDown(event(graph,anchor,anchor,juce::ModifierKeys::middleButtonModifier));
        graph.mouseDrag(event(graph,anchor+juce::Point<float>{20,0},anchor,juce::ModifierKeys::middleButtonModifier));graph.mouseUp(event(graph,anchor,anchor));
        check(handle.getBounds().getCentreX()>panned+10,"Middle drag did not pan envelope");
        check(current()==before,"Navigation changed sound");{click(editor,"waveform-fit");settle(editor);}
        auto downPoint=handle.getLocalBounds().getCentre().toFloat(),startPosition=handle.getPosition().toFloat();
        handle.mouseDown(event(handle,downPoint,downPoint));
        for(int move:{10,20,35})handle.mouseDrag(event(handle,startPosition+downPoint+juce::Point<float>{(float)move,10}-handle.getPosition().toFloat(),downPoint));
        handle.mouseUp(event(handle,downPoint,downPoint));const auto after=current();
        check(after.sound.envelopes[(size_t)n]!=before.sound.envelopes[(size_t)n],"History gesture did not edit");
        click(editor,(prefix+"-undo").toRawUTF8());check(current()==before,"Undo did not restore entire drag and activation");
        check(!control<juce::TextButton>(editor,(prefix+"-undo").toRawUTF8()).isEnabled(),"Drag created multiple undo entries");
        click(editor,(prefix+"-redo").toRawUTF8());check(current()==after,"Redo did not restore envelope edit");
        click(editor,(prefix+"-reset").toRawUTF8());check(current().sound.envelopes[(size_t)n].count==0,"Envelope reset left custom points");
        click(editor,(prefix+"-undo").toRawUTF8());check(current()==after,"Reset cannot be undone");
        drag(handle,{(float)graph.getWidth()*1000,0});
        const auto bounded=current().sound.envelopes[(size_t)n];
        near(bounded.points[(size_t)bounded.count-1].ms,fullMs,0.001,"Envelope limit uses trimmed audio or exceeds original duration");
        for(int k=0;k<bounded.count;++k)check(bounded.points[(size_t)k].ms<=fullMs,"Envelope node exceeds sample length");
        check(!control<juce::TextButton>(editor,(prefix+"-redo").toRawUTF8()).isEnabled(),"New edit retained stale redo");
        click(editor,(prefix+"-adsr").toRawUTF8());
        const char* decay=n==0?"edit-ampDecay":n==1?"edit-pitchDecay":"edit-filterDecay";
        control<juce::Slider>(editor,decay).setValue(20000,juce::sendNotificationSync);
        const auto limited=current().sound;
        const auto total=n==0?limited.ampAttack+limited.ampDecay+limited.ampRelease:n==1?limited.pitchAttack+limited.pitchDecay+limited.pitchRelease:limited.filterAttack+limited.filterDecay+limited.filterRelease;
        check(total<=fullMs+0.00001,"Numeric ADSR bypassed original sample limit");
    }
    // Reset gestures from the value label and an already-open text editor must
    // reach the same default and notification path as the slider itself.
    auto resetNumber=[&](const char* key,double changed,double expected,bool editing=false) {
        auto& slider=control<ResetSlider>(editor,key);slider.setValue(changed,juce::sendNotificationSync);
        juce::Label* label=nullptr;for(auto* child:slider.getChildren())if(auto* l=dynamic_cast<juce::Label*>(child))label=l;
        check(label!=nullptr,"Slider value label missing");
        juce::Component* target=label;
        if(editing){label->showEditor();target=label->getCurrentTextEditor();check(target!=nullptr,"Numeric entry unavailable");}
        slider.mouseDoubleClick(event(*target,{2,2},{2,2}));check(slider.isUpdatePending(),"Value-box double click not routed");slider.handleUpdateNowIfNeeded();
        near(slider.getValue(),expected,0.0001,"Numeric double click did not restore default");
    };
    click(editor,"edit-tab-0");resetNumber("edit-sampleGain",7,0);check(current().sound.sampleGain==0,"Numeric reset did not reach sound state");
    resetNumber("edit-fadeOut",25,2,true);check(current().sound.fadeOut==2,"Open editor reset did not reach sound state");
    click(editor,"edit-tab-1");resetNumber("envelope-pitch-amount",5,0);check(current().sound.pitchAmount==0,"Envelope amount default incorrect");
    resetNumber("edit-ampRelease",0,100);check(current().sound.ampRelease==100 && current().sound.ampAttack+current().sound.ampDecay+current().sound.ampRelease<=fullMs+0.00001,"Bounded timing reset did not retain its default");
    click(editor,"edit-tab-2");click(editor,"filter-eq-tab");click(editor,"eq-band-4");resetNumber("eq-frequency",3000,1500);resetNumber("eq-output",6,0);resetNumber("eq-q",3,0.70710678);
    click(editor,"edit-tab-5");resetNumber("macro-value-0",75,50);near(p.macro(0).get(),0.5,0.0001,"Macro numeric reset did not notify host");
    click(editor,"edit-tab-1");
    c=current();c.sound.envelopes[0]=drum::adsrShape(100,150,0.4,150);check(library.setControls(kit,id,c).ok,"Recall shape fixture failed");refresh();
    auto down=ampA.getLocalBounds().getCentre().toFloat();ampA.mouseDown(event(ampA,down,down));const auto original=current();
    const auto second=library.addInstrument(kit,group).id;state=library.snapshot();soundEditor.select(kit,drum::findInstrument(*drum::activeKit(state),second));
    ampA.mouseDrag(event(ampA,down+juce::Point<float>{70,0},down));ampA.mouseUp(event(ampA,down,down));
    state=library.snapshot();check(drum::findInstrument(*drum::activeKit(state),second)->controls.sound==drum::SoundParameters{} && current()==original,"Gesture crossed instrument selection");
    juce::MemoryBlock memory;p.getStateInformation(memory);DrumAudioProcessor recalled;recalled.setStateInformation(memory.getData(),(int)memory.getSize());state=recalled.getLibrary().snapshot();
    check(drum::findInstrument(*drum::activeKit(state),id)->controls==original,"Custom curves and key not recalled");
    auto xml=library.toXml();drum::KitLibrary copy;check(copy.restore(*xml),"Library recall failed");
    auto* instrument=xml->getFirstChildElement()->getFirstChildElement()->getFirstChildElement();
    auto* envelope=instrument->getFirstChildElement();check(envelope!=nullptr,"Custom envelope not serialized");
    envelope->getFirstChildElement()->setAttribute("pivot",2.0);
    const auto prior=copy.toXml()->toString();check(!copy.restore(*xml) && copy.toXml()->toString()==prior,"Malformed shape was accepted/partially applied");
    xml=library.toXml();instrument=xml->getFirstChildElement()->getFirstChildElement()->getFirstChildElement();
    stripChainEffects(*xml);xml->setAttribute("schemaVersion",8);check(copy.restore(*xml),"Version 8 envelope migration failed");
    state=copy.snapshot();for(const auto& shape:drum::findInstrument(*drum::activeKit(state),id)->controls.sound.envelopes)for(int n=0;n<shape.count;++n)check(shape.points[(size_t)n].weight==1,"Version 8 envelope changed curvature model");
    xml=library.toXml();instrument=xml->getFirstChildElement()->getFirstChildElement()->getFirstChildElement();
    instrument->getFirstChildElement()->getFirstChildElement()->setAttribute("weight",0.0);check(!copy.restore(*xml),"Invalid curve weight accepted");
    xml=library.toXml();instrument=xml->getFirstChildElement()->getFirstChildElement()->getFirstChildElement();
    instrument->deleteAllChildElements();stripChainEffects(*xml);xml->setAttribute("schemaVersion",6);check(copy.restore(*xml),"Version 6 migration failed");
    state=copy.snapshot();check(drum::findInstrument(*drum::activeKit(state),id)->controls.sound.envelopes[0].count==0,"Legacy state gained custom envelope");
    std::cout<<"Free point/curve editing, early release, sound analysis, waveform, macros and schema recall passed\n";
}
