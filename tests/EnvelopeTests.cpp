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
juce::MouseEvent event(juce::Component& c,juce::Point<float> point,juce::Point<float> down,int mods=juce::ModifierKeys::leftButtonModifier)
{
    auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,now,down,now,1,point!=down};
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
        if(juce::Time::getMillisecondCounterHiRes()-start>200 && control<juce::Label>(root,"waveform-status").getText().startsWith("One hit"))return;
    }
    throw std::runtime_error("Envelope preview timed out");
}
void shapeTests()
{
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
    tones({220});near(drum::analyseTonal(audio,48000).midi,57,0.05,"A3 pitch detection failed");
    tones({261.625565,329.627557,391.995436});a=drum::analyseTonal(audio,48000);std::cout<<"C chord key estimate: "<<a.key<<"\n";check(a.key==0,"C major chord recognition failed");
    tones({233.081881,277.182631,349.228231});a=drum::analyseTonal(audio,48000);check(a.key==22,"Bb minor chord recognition failed");
    juce::Random random(123);for(int c=0;c<2;++c)for(int n=0;n<48000;++n)audio.setSample(c,n,random.nextFloat()*0.5f-0.25f);
    a=drum::analyseTonal(audio,48000);check(a.midi<0 && a.key<0,"Noise given a definite pitch/key");
    audio.clear();a=drum::analyseTonal(audio,48000);check(a.midi<0 && a.key<0 && a.complete,"Silence analysis incorrect");
    check(!drum::analyseTonal(audio,48000,[]{return true;}).complete,"Analysis cancellation ignored");
    check(drum::camelotCode(22)=="3A" && drum::camelotCode(21)=="8A" && drum::camelotCode(0)=="8B","Camelot mapping incorrect");
    check(drum::musicalNote(69)=="A4","Note octave incorrect");
}
}
void runEnvelopeTests()
{
    shapeTests();tonalTests();
    DrumAudioProcessor p;auto& library=p.getLibrary();auto state=library.snapshot();const auto kit=state.activeKit,group=drum::activeKit(state)->groups.front().id;
    const auto id=library.addInstrument(kit,group).id;
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Preview percussion.wav");
    p.getSampleEngine().requestLoad(kit,id,file);check(p.getSampleEngine().waitForLoading(10000),"Envelope sample load timed out");
    check(p.getSampleEngine().statusFor(id).ready,"Envelope fixture unavailable");p.getSampleEngine().prepare(48000);
    DrumAudioProcessorEditor editor(p);editor.setSize(1280,900);capture(editor,"drum-manual-main.png");click(editor,"open-sound-editor");click(editor,"edit-tab-1");settle(editor);
    auto current=[&]{auto s=library.snapshot();return drum::findInstrument(*drum::activeKit(s),id)->controls;};
    auto& soundEditor=control<SoundEditor>(editor,"sound-editor");
    auto refresh=[&]{auto s=library.snapshot();soundEditor.select(kit,drum::findInstrument(*drum::activeKit(s),id));};
    auto& amp=control<EnvelopeEditor>(editor,"envelope-amp");auto& pitch=control<EnvelopeEditor>(editor,"envelope-pitch");auto& filter=control<EnvelopeEditor>(editor,"envelope-filter");
    check(amp.isVisible() && pitch.isVisible() && !filter.isVisible(),"Envelope graphs shown on wrong page");
    auto& wave=control<juce::Component>(editor,"sample-waveform");const auto waveBounds=wave.getBounds();
    auto c=current();c.sound.ampAttack=80;c.sound.ampDecay=200;c.sound.ampSustain=0.4;c.sound.ampRelease=200;c.sound.pitchAttack=80;c.sound.pitchDecay=200;c.sound.pitchSustain=0.2;
    check(library.setControls(kit,id,c).ok,"Envelope fixture controls failed");refresh();
    // Click on an actual line point (time 180 in the decay, fitted span 528ms).
    auto onLine=[&](EnvelopeEditor& graph,const drum::EnvelopeShape& shape,int segment,double fraction)
    {
        const auto a=shape.points[segment-1],b=shape.points[segment];const double ms=a.ms+(b.ms-a.ms)*fraction;
        return juce::Point<float>{35+(float)(ms/(shape.points[shape.count-1].ms*1.1))*(graph.getWidth()-55),
            (float)(graph.getHeight()-40)-(float)drum::envelopeSegment(a.level,b,fraction)*(graph.getHeight()-96)};
    };
    auto line=onLine(pitch,drum::adsrShape(80,200,0.2,100),2,0.5);
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
    click(editor,"envelope-amp-activate");drag(control<juce::Component>(editor,"envelope-amp-point-1"),{10,12});check(current().sound.mode==2 && current().sound.envelopes[0].count==4,"Amplitude shape failed");
    const auto saved=library.toXml()->toString();click(editor,"envelope-amp-zoom-in");click(editor,"envelope-amp-zoom-out");click(editor,"envelope-amp-fit");
    check(library.toXml()->toString()==saved,"View zoom changes sound");
    settle(editor);capture(editor,"drum-envelopes.png");editor.setSize(800,600);capture(editor,"drum-envelopes-compact.png");editor.setSize(1280,900);
    click(editor,"edit-tab-2");check(filter.isVisible() && !amp.isVisible() && wave.getBounds()==waveBounds,"Filter graph moved waveform");
    c=current();c.sound.filterOn=1;c.sound.cutoff=1500;c.sound.filterAmount=3;c.sound.filterAttack=30;check(library.setControls(kit,id,c).ok,"Filter setup failed");refresh();
    drag(control<juce::Component>(editor,"envelope-filter-point-2"),{30,-10});check(current().sound.envelopes[2].count==4,"Filter graph did not save edits");settle(editor);capture(editor,"drum-filter-envelope.png");
    for(int tab:{0,3,4,5,6}){click(editor,("edit-tab-"+juce::String(tab)).toRawUTF8());settle(editor);capture(editor,("drum-manual-tab-"+juce::String(tab)+".png").toRawUTF8());}
    click(editor,"edit-tab-1");
    control<juce::ComboBox>(editor,"edit-keyOverride").setSelectedId(24,juce::sendNotificationSync);settle(editor);
    const auto note=control<juce::Label>(editor,"note-indicator").getText();
    check(note.contains("Trigger:") && note.contains("MIDI 21") && note.contains("3A") && note.contains("manual"),"Trigger/pitch/Camelot labels missing");
    capture(editor,"drum-note-indicator.png");
    c=current();c.macros[0]={7,100,900};check(library.setControls(kit,id,c).ok,"Envelope mapping failed");refresh();
    auto& ampA=control<juce::Component>(editor,"envelope-amp-point-1");check(!ampA.isEnabled(),"Macro-mapped handle editable");
    const auto base=current().sound.envelopes[0];auto macroImage=pixels(amp);p.macro(0).setValueNotifyingHost(0.9f);settle(editor);
    check(pixels(amp)!=macroImage,"Host macro did not redraw envelope");drag(ampA,{30,0});check(current().sound.envelopes[0]==base,"Mapped envelope overwritten");
    c=current();c.macros[0]={};check(library.setControls(kit,id,c).ok,"Envelope mapping removal failed");refresh();
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
    instrument->deleteAllChildElements();xml->setAttribute("schemaVersion",6);check(copy.restore(*xml),"Version 6 migration failed");
    state=copy.snapshot();check(drum::findInstrument(*drum::activeKit(state),id)->controls.sound.envelopes[0].count==0,"Legacy state gained custom envelope");
    std::cout<<"Free point/curve editing, early release, sound analysis, waveform, macros and schema recall passed\n";
}
