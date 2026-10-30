#pragma once
#include "KitLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
namespace drum {
class PatternPlayer final:private juce::Timer {
public:
    explicit PatternPlayer(KitLibrary& model):library(model){startTimerHz(25);}
    ~PatternPlayer() override{stopTimer();}
    void start(const juce::String& id){selected=id;phaseReset.fetch_add(1);running=true;compile();}
    void stop(){running=false;}
    bool isRunning() const{return running.load();}
    int playStep() const{return displayedStep.load();}
    void prepare(double newRate){rate=newRate;phase=0;active.fill(false);seenReset=0;}
    static Result exportMidi(const Kit& kit,const Pattern& pattern,double bpm,const juce::File& file){
        if(!std::isfinite(bpm) || bpm<20 || bpm>999)return Result::fail("Invalid tempo.");
        juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::round(60000000.0/bpm)));sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4,4));sequence.addEvent(juce::MidiMessage::textMetaEvent(3,pattern.name));
        for(const auto& hit:pattern.hits)if(auto* instrument=findInstrument(kit,hit.instrumentId)){
            auto on=juce::MidiMessage::noteOn(1,instrument->note,(juce::uint8)hit.velocity);on.setTimeStamp(hit.step*240.0);sequence.addEvent(on);
            auto off=juce::MidiMessage::noteOff(1,instrument->note);off.setTimeStamp(hit.step*240.0+216);sequence.addEvent(off);
        }
        auto end=juce::MidiMessage::endOfTrack();end.setTimeStamp(pattern.steps*240.0);sequence.addEvent(end);sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);
        if(!file.getParentDirectory().createDirectory())return Result::fail("Cannot create MIDI folder.");juce::TemporaryFile temp(file);{juce::FileOutputStream stream(temp.getFile());if(!stream.openedOk() || !midi.writeTo(stream,0))return Result::fail("Cannot write MIDI file.");stream.flush();if(stream.getStatus().failed())return Result::fail("Cannot finish MIDI file.");}return temp.overwriteTargetFileWithTemporary()?Result::success():Result::fail("Cannot replace MIDI file.");
    }
    void process(juce::MidiBuffer& midi,int samples,double bpm){
        bool scheduleChanged=false;{const juce::ScopedTryLock guard(scheduleLock);if(guard.isLocked() && pendingGeneration!=generation){schedule=pending;generation=pendingGeneration;scheduleChanged=true;}}
        auto reset=phaseReset.load();bool restart=reset!=seenReset || scheduleChanged;seenReset=reset;
        bool enabled=running.load() && schedule.epoch==library.activeKitEpoch();
        if(!enabled || restart){for(int n=0;n<128;++n)if(active[(size_t)n]){midi.addEvent(juce::MidiMessage::noteOff(1,n),0);active[(size_t)n]=false;}if(!enabled){phase=0;displayedStep=-1;return;}phase=0;}
        if(rate<=0 || samples<=0)return;bpm=juce::jlimit(20.0,999.0,bpm);double increment=bpm/(60.0*rate),end=phase+samples*increment,length=schedule.steps*.25;
        for(int cycle=(int)std::floor(phase/length);cycle<=(int)std::floor(end/length);++cycle)for(int n=0;n<schedule.count;++n){const auto& event=schedule.events[(size_t)n];double beat=cycle*length+event.beat;if(beat<phase || beat>=end)continue;int offset=juce::jlimit(0,samples-1,(int)std::round((beat-phase)/increment));midi.addEvent(event.on?juce::MidiMessage::noteOn(1,event.note,(juce::uint8)event.velocity):juce::MidiMessage::noteOff(1,event.note),offset);active[(size_t)event.note]=event.on;}
        phase=std::fmod(end,length);displayedStep=(int)(phase*4);
    }
private:
    struct Event{double beat=0;int note=0,velocity=0;bool on=false;bool operator==(const Event&) const = default;};
    struct Schedule{std::array<Event,88*64*2> events{};int count=0,steps=16;uint64_t epoch=0;};
    void timerCallback() override{if(running)compile();}
    void compile(){if(library.currentRevision()==revision && selected==compiled)return;auto state=library.snapshot();if(state.revision==revision && selected==compiled)return;revision=state.revision;compiled=selected;auto* kit=activeKit(state);const Pattern* pattern=nullptr;if(kit)for(const auto& p:kit->patterns)if(p.id==selected)pattern=&p;if(!pattern){running=false;return;}
        auto next=std::make_unique<Schedule>();next->steps=pattern->steps;next->epoch=state.kitEpoch;for(const auto& hit:pattern->hits)if(auto* instrument=findInstrument(*kit,hit.instrumentId)){next->events[(size_t)next->count++]={hit.step*.25,instrument->note,hit.velocity,true};next->events[(size_t)next->count++]={hit.step*.25+.225,instrument->note,0,false};}
        std::sort(next->events.begin(),next->events.begin()+next->count,[](const auto& a,const auto& b){return a.beat<b.beat;});const juce::ScopedLock guard(scheduleLock);if(next->steps==pending.steps && next->epoch==pending.epoch && next->count==pending.count && std::equal(next->events.begin(),next->events.begin()+next->count,pending.events.begin()))return;pending=*next;++pendingGeneration;
    }
    KitLibrary& library;juce::String selected,compiled;uint64_t revision=UINT64_MAX;std::atomic<bool> running{false};std::atomic<int> displayedStep{-1};std::atomic<unsigned> phaseReset{0};unsigned seenReset=0;
    juce::CriticalSection scheduleLock;Schedule pending,schedule;uint64_t pendingGeneration=0,generation=0;double phase=0,rate=44100;std::array<bool,128> active{};
};
}
