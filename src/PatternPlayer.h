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
    int playStep() const{return displayedBeat<0?-1:(int)(displayedBeat.load()*4);}
    double playBeat() const{return displayedBeat.load();}
    void prepare(double newRate){rate=newRate;position=0;active.fill(false);accepted.fill(false);seenReset=0;}
    static Result exportMidi(const Kit& kit,const Pattern& clip,double bpm,const juce::File& file){
        if(!std::isfinite(bpm) || bpm<20 || bpm>999)return Result::fail("Invalid tempo.");
        juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::round(60000000.0/bpm)));sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(clip.numerator,clip.denominator));sequence.addEvent(juce::MidiMessage::textMetaEvent(3,clip.name));
        for(const auto& hit:clip.hits)if(hit.enabled)if(auto* instrument=findInstrument(kit,hit.instrumentId)){
            // MIDI has no probability/deviation attributes: export one stable realisation.
            uint64_t seed=(uint64_t)hit.id.hashCode64();auto range=probabilityRange(clip,hit);double draw=hit.groupMode?((uint64_t)hit.chanceGroup.hashCode64()%1000000)/1000000.0:(seed%1000000)/1000000.0;if(draw<range.first || draw>=range.second)continue;
            int velocity=juce::jlimit(1,127,hit.velocity+(hit.deviation?(int)((seed/1000000)%(2*hit.deviation+1))-hit.deviation:0));
            auto on=juce::MidiMessage::noteOn(1,instrument->note,(juce::uint8)velocity);on.setTimeStamp(hit.beat()*960);sequence.addEvent(on);
            auto off=juce::MidiMessage::noteOff(1,instrument->note,(juce::uint8)hit.releaseVelocity);off.setTimeStamp((hit.beat()+hit.duration)*960);sequence.addEvent(off);
        }
        auto end=juce::MidiMessage::endOfTrack();end.setTimeStamp(clip.end()*960);sequence.addEvent(end);sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);
        if(!file.getParentDirectory().createDirectory())return Result::fail("Cannot create MIDI folder.");juce::TemporaryFile temp(file);{juce::FileOutputStream stream(temp.getFile());if(!stream.openedOk() || !midi.writeTo(stream,0))return Result::fail("Cannot write MIDI file.");stream.flush();if(stream.getStatus().failed())return Result::fail("Cannot finish MIDI file.");}return temp.overwriteTargetFileWithTemporary()?Result::success():Result::fail("Cannot replace MIDI file.");
    }
    void process(juce::MidiBuffer& midi,int samples,double bpm){
        bool changed=false;{const juce::ScopedTryLock guard(scheduleLock);if(guard.isLocked() && pendingGeneration!=generation){*schedule=*pending;generation=pendingGeneration;changed=true;}}
        auto reset=phaseReset.load();bool restart=reset!=seenReset || changed;seenReset=reset;
        bool enabled=running.load() && schedule->epoch==library.activeKitEpoch();
        if(!enabled || restart){release(midi,0);if(!enabled){position=0;displayedBeat=-1;return;}position=schedule->looping?schedule->loopStart:0;cycleSeed=randomState++;}
        if(rate<=0 || samples<=0)return;double increment=juce::jlimit(20.0,999.0,bpm)/(60.0*rate),consumed=0;
        while(consumed<samples){double limit=schedule->looping?schedule->loopEnd:schedule->length;double amount=juce::jmin((double)samples-consumed,(limit-position)/increment);if(amount<1.e-7){release(midi,juce::jlimit(0,samples-1,(int)std::round(consumed)));if(schedule->looping){position=schedule->loopStart;cycleSeed=randomState++;continue;}running=false;displayedBeat=-1;break;}
            double end=position+amount*increment;
            for(int n=0;n<schedule->count;++n){const auto& e=schedule->events[(size_t)n];if(e.beat<position || e.beat>=end)continue;int offset=juce::jlimit(0,samples-1,(int)std::round(consumed+(e.beat-position)/increment));
                if(e.on){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;uint32_t value=randomState;if(e.group){value=e.group^cycleSeed;value^=value<<13;value^=value>>17;value^=value<<5;}double draw=(value%1000000)/1000000.0;bool play=draw>=e.low && draw<e.chance;accepted[(size_t)e.hit]=play;if(play){int velocity=juce::jlimit(1,127,e.velocity+(e.deviation?(int)((randomState/1000)%(2*e.deviation+1))-e.deviation:0));midi.addEvent(juce::MidiMessage::noteOn(1,e.note,(juce::uint8)velocity),offset);active[(size_t)e.note]=true;}}
                else if(accepted[(size_t)e.hit]){midi.addEvent(juce::MidiMessage::noteOff(1,e.note,(juce::uint8)e.velocity),offset);active[(size_t)e.note]=false;accepted[(size_t)e.hit]=false;}
            }
            consumed+=amount;position=end;displayedBeat=position;
        }
    }
private:
    static std::pair<double,double> probabilityRange(const Pattern& clip,const PatternHit& hit){
        if(hit.groupMode==0)return {0,hit.chance};
        if(hit.groupMode==1){for(const auto& other:clip.hits)if(other.enabled && other.chanceGroup==hit.chanceGroup)return {0,other.chance};return {0,0};}
        double sum=0,before=0;bool found=false;for(const auto& other:clip.hits)if(other.enabled && other.chanceGroup==hit.chanceGroup){sum+=other.chance;if(other.id==hit.id)found=true;else if(!found)before+=other.chance;}
        return sum>0?std::pair<double,double>{before/sum,(before+hit.chance)/sum}:std::pair<double,double>{0,0};
    }
    struct Event{double beat=0,chance=1;int note=0,velocity=0,deviation=0,hit=0;bool on=false;double low=0;uint32_t group=0;bool operator==(const Event&) const = default;};
    struct Schedule{std::array<Event,8192*2> events{};int count=0;double length=4,loopStart=0,loopEnd=4;bool looping=true;uint64_t epoch=0;};
    void release(juce::MidiBuffer& midi,int offset){for(int n=0;n<128;++n)if(active[(size_t)n]){midi.addEvent(juce::MidiMessage::noteOff(1,n),offset);active[(size_t)n]=false;}accepted.fill(false);}
    void timerCallback() override{if(running)compile();}
    void compile(){if(library.currentRevision()==revision && selected==compiled)return;auto state=library.snapshot();revision=state.revision;compiled=selected;auto* kit=activeKit(state);const Pattern* clip=nullptr;if(kit)for(const auto& p:kit->patterns)if(p.id==selected)clip=&p;if(!clip){running=false;return;}
        auto next=std::make_unique<Schedule>();next->length=clip->end();next->loopStart=clip->loopStart;next->loopEnd=clip->loopFinish();next->looping=clip->looping;next->epoch=state.kitEpoch;int index=0;for(const auto& hit:clip->hits){if(hit.enabled)if(auto* instrument=findInstrument(*kit,hit.instrumentId)){auto range=probabilityRange(*clip,hit);next->events[(size_t)next->count++]={hit.beat(),range.second,instrument->note,hit.velocity,hit.deviation,index,true,range.first,hit.groupMode?(uint32_t)(hit.chanceGroup.hashCode()|1):0};next->events[(size_t)next->count++]={hit.beat()+hit.duration,1,instrument->note,hit.releaseVelocity,0,index,false};}++index;}
        std::sort(next->events.begin(),next->events.begin()+next->count,[](const auto& a,const auto& b){return a.beat==b.beat?!a.on && b.on:a.beat<b.beat;});const juce::ScopedLock guard(scheduleLock);if(next->length==pending->length && next->loopStart==pending->loopStart && next->loopEnd==pending->loopEnd && next->looping==pending->looping && next->epoch==pending->epoch && next->count==pending->count && std::equal(next->events.begin(),next->events.begin()+next->count,pending->events.begin()))return;*pending=*next;++pendingGeneration;
    }
    KitLibrary& library;juce::String selected,compiled;uint64_t revision=UINT64_MAX;std::atomic<bool> running{false};std::atomic<double> displayedBeat{-1};std::atomic<unsigned> phaseReset{0};unsigned seenReset=0;uint32_t randomState=0x57ade132,cycleSeed=1;
    juce::CriticalSection scheduleLock;std::unique_ptr<Schedule> pending=std::make_unique<Schedule>(),schedule=std::make_unique<Schedule>();uint64_t pendingGeneration=0,generation=0;double position=0,rate=44100;std::array<bool,128> active{};std::array<bool,8192> accepted{};
};
}
