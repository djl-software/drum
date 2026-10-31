#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
namespace
{
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
juce::Component& find(juce::Component& root,const juce::String& id)
{
    std::function<juce::Component*(juce::Component&)> search=[&](juce::Component& c)->juce::Component* {if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* result=search(*child))return result;return nullptr;};
    auto* result=search(root);check(result!=nullptr,"Pad control missing");return *result;
}
juce::MouseEvent mouse(juce::Component& c,juce::Point<float> point,bool down=true)
{auto now=juce::Time::getCurrentTime();return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&c,&c,now,point,now,1,false};}
uint64_t pixels(juce::Component& c,juce::Rectangle<int> area)
{auto image=c.createComponentSnapshot(area);uint64_t hash=0;for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)hash=hash*31+image.getPixelAt(x,y).getARGB();return hash;}
void tick(int ms=50){juce::Thread::sleep(ms);juce::Timer::callPendingTimersSynchronously();}
void capture(juce::Component& c,const char* name)
{auto file=juce::File::getCurrentWorkingDirectory().getChildFile(name);file.deleteFile();juce::FileOutputStream out(file);check(juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),out),"Pad capture failed");}
}
void runPadTests()
{
    DrumAudioProcessor processor;auto& library=processor.getLibrary();auto& engine=processor.getSampleEngine();
    auto state=library.snapshot();const auto kit=state.activeKit,group=drum::activeKit(state)->groups[0].id;
    const char* names[]{"Kick","Snare","Closed hi-hat","Open hi-hat","Low tom","Mid tom","High tom","Crash cymbal"};
    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("playback-test-data/Pad hit.wav");file.getParentDirectory().createDirectory();file.deleteFile();
    auto stream=file.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));check(writer!=nullptr,"Pad fixture failed");
    juce::AudioBuffer<float> fixture(1,48000);for(int n=0;n<48000;++n)fixture.setSample(0,n,0.25f);writer->writeFromAudioSampleBuffer(fixture,0,48000);writer.reset();
    for(int n=0;n<8;++n){auto id=library.addInstrument(kit,group).id;check(library.rename(id,names[n]).ok,"Pad rename failed");engine.requestLoad(kit,id,file);}
    check(engine.waitForLoading(10000),"Pads did not load");processor.prepareToPlay(48000,256);
    DrumAudioProcessorEditor editor(processor);editor.setSize(1280,900);editor.setVisible(true);
    auto& grid=find(editor,"instrument-grid");auto& piano=find(editor,"piano-keyboard");juce::AudioBuffer<float> output(2,256);juce::MidiBuffer midi;
    processor.processBlock(output,midi);tick();
    auto* name=dynamic_cast<juce::TextEditor*>(&find(editor,"instrument-name"));check(name!=nullptr,"Pad name missing");
    for(auto size:{juce::Point<int>{1280,900},juce::Point<int>{1080,860},juce::Point<int>{800,600}})
    {
        editor.setSize(size.x,size.y);const int side=(grid.getWidth()-36)/4;
        for(int n:{0,3,4,7})
        {
            const juce::Point<float> point{float((n%4)*(side+12)+side/2),float((n/4)*(side+12)+side/2)};
            grid.mouseDown(mouse(grid,point));processor.processBlock(output,midi);
            check(name->getText()==names[n],"Four-across pad hit testing selected wrong instrument");
            check(output.getMagnitude(0,256)>0.15f,"Pad press did not play sample");check((engine.noteActivity(21+n)&0x10000)!=0,"Pad press missing from piano activity");
            grid.mouseUp(mouse(grid,point,false));processor.processBlock(output,midi);check((engine.noteActivity(21+n)&0x10000)==0,"Pad release stuck");
            engine.stop();processor.processBlock(output,midi);
        }
    }
    editor.setSize(1280,900);const int side=(grid.getWidth()-36)/4;juce::Point<float> point{float(side/2),float(side/2)};
    // Select before taking a baseline: activity must visibly differ from selection alone.
    // Allow the five-frame flash plus the three-frame key return to finish before recording the idle image.
    grid.mouseDown(mouse(grid,point));grid.mouseUp(mouse(grid,point,false));engine.stop();processor.processBlock(output,midi);for(int n=0;n<10;++n)tick();
    const auto area=juce::Rectangle<int>{0,0,side,side};auto idlePad=pixels(grid,area),idlePiano=pixels(piano,piano.getLocalBounds());
    midi.addEvent(juce::MidiMessage::noteOn(2,21,(juce::uint8)110),0);processor.processBlock(output,midi);tick();
    check(output.getMagnitude(0,256)>0.15f,"Incoming MIDI did not play the sample");
    check(pixels(grid,area)!=idlePad && pixels(piano,piano.getLocalBounds())!=idlePiano,"MIDI failed to highlight both pad and piano");
    for(int n=0;n<7;++n)tick();check(pixels(grid,area)!=idlePad,"Held MIDI note stopped highlighting");
    capture(editor,"drum-pads-midi.png");
    const int keyX=(int)(piano.getWidth()/104.0f);
    auto heldImage=piano.createComponentSnapshot(piano.getLocalBounds());
    check(heldImage.getPixelAt(keyX,2).getBrightness()<0.2f,"White key top did not move down");
    midi.addEvent(juce::MidiMessage::noteOn(2,22,(juce::uint8)110),0);processor.processBlock(output,midi);tick();tick();
    capture(editor,"drum-piano-pressed.png");
    midi.addEvent(juce::MidiMessage::noteOff(2,22),0);processor.processBlock(output,midi);for(int n=0;n<8;++n)tick();
    midi.addEvent(juce::MidiMessage::noteOn(3,21,(juce::uint8)90),0);midi.addEvent(juce::MidiMessage::noteOff(2,21),64);processor.processBlock(output,midi);tick();
    check((engine.noteActivity(21)&7)==4,"One channel's release cleared another held channel");
    midi.addEvent(juce::MidiMessage::noteOff(3,21),0);processor.processBlock(output,midi);for(int n=0;n<7;++n)tick();
    check(pixels(grid,area)==idlePad && pixels(piano,piano.getLocalBounds())==idlePiano,"Released MIDI highlight stuck");
    midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)100),12);midi.addEvent(juce::MidiMessage::noteOff(1,21),24);processor.processBlock(output,midi);tick();
    check(pixels(grid,area)!=idlePad && pixels(piano,piano.getLocalBounds())!=idlePiano,"Single-block MIDI hit was invisible");
    for(int n=0;n<7;++n)tick();check(pixels(grid,area)==idlePad,"Short hit flash never cleared");
    // A mouse key must select and sound the corresponding pad too.
    juce::Point<float> keyPoint{piano.getWidth()/104.0f,piano.getHeight()*0.9f};
    piano.mouseDown(mouse(piano,keyPoint));processor.processBlock(output,midi);check(output.getMagnitude(0,256)>0.15f,"Piano press did not play");
    check(pixels(grid,area)!=idlePad,"Piano press failed to highlight pad");piano.mouseUp(mouse(piano,keyPoint,false));processor.processBlock(output,midi);
    engine.stop();processor.processBlock(output,midi);for(int n=0;n<7;++n)tick();
    capture(editor,"drum-pads.png");editor.setSize(800,600);capture(editor,"drum-pads-compact.png");
    // UI gate release must not release a physical MIDI key on the same note/channel.
    state=library.snapshot();auto id=drum::activeKit(state)->groups[0].instruments[0].id;drum::InstrumentControls controls;controls.sound.mode=1;
    check(library.setControls(kit,id,controls).ok && engine.waitForLoading(10000),"Gate pad setup failed");
    engine.keyboardNote(21,true);midi.addEvent(juce::MidiMessage::noteOn(1,21,(juce::uint8)127),0);processor.processBlock(output,midi);
    engine.keyboardNote(21,false);processor.processBlock(output,midi);processor.processBlock(output,midi);
    check(output.getMagnitude(0,256)>0.24f,"Mouse release cut a MIDI voice");
    midi.addEvent(juce::MidiMessage::noteOff(1,21),0);processor.processBlock(output,midi);processor.processBlock(output,midi);
    check(output.getMagnitude(0,256)==0,"Gate note-off did not stop playback");
    engine.audition(128);engine.audition(-1);processor.processBlock(output,midi);check(output.getMagnitude(0,256)==0,"Invalid audition note was played");
    // Stop cancels queued mouse hits; all-notes-off clears held channel indicators.
    engine.keyboardNote(21,true);engine.stop();processor.processBlock(output,midi);check(output.getMagnitude(0,256)==0,"Stop replayed queued pad hit");
    midi.addEvent(juce::MidiMessage::noteOn(4,21,(juce::uint8)127),0);processor.processBlock(output,midi);
    midi.addEvent(juce::MidiMessage::allNotesOff(4),0);processor.processBlock(output,midi);check((engine.noteActivity(21)&0x1ffff)==0,"All notes off left held indicator");
    check(library.rename(kit,"House drums").ok,"Kit rename failed");
    for(const char* title:{"Acoustic","808","Live percussion","A deliberately long kit name that needs an ellipsis","Breaks","Jungle","Hip hop"})
    {auto result=library.addKit();check(result.ok && library.rename(result.id,title).ok,"Named kit setup failed");}
    check(library.activateKit(kit).ok,"Kit reactivation failed");tick();tick();
    for(auto size:{juce::Point<int>{1280,900},juce::Point<int>{800,600}})
    {
        editor.setSize(size.x,size.y);int previousRight=0;
        for(int n=0;n<8;++n)
        {
            auto& button=dynamic_cast<juce::TextButton&>(find(editor,"kit-"+juce::String(n)));
            check(button.getWidth()>=80 && button.getX()>=previousRight,"Library tabs overlap");
            check(button.getTooltip().isNotEmpty() && button.getButtonText()==button.getTooltip()+" *","Library tab lost its full name or unsaved marker");
            check(button.getRight()<=button.getParentComponent()->getWidth(),"Library tabs overflow their scrollable row");previousRight=button.getRight();
        }
        check(find(editor,"kit-2").getWidth()<find(editor,"kit-0").getWidth(),"Short kit name wastes tab width");
        capture(editor,size.x==1280?"drum-kit-names.png":"drum-kit-names-compact.png");
    }
    std::cout<<"Four-across pads, mouse/piano playback, MIDI hold/flash, channel isolation and Stop passed\n";
}
