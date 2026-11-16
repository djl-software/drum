#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include "SoundParameters.h"

namespace drum
{
constexpr int maxKits = 8, maxGroups = 16, maxInstruments = 88, firstNote = 21;
struct SendProcessing
{
    SoundParameters sound;
    double tuning=0;
    bool operator==(const SendProcessing& other) const {
        if(tuning!=other.tuning)return false;
        for(const auto& p:soundParameters)if(isSendShapingParameter(p))if(sound.*p.member!=other.sound.*p.member)return false;
        for(int n=0;n<3;++n)if(sound.envelopes[n]!=other.sound.envelopes[n])return false;
        return true;
    }
};
// Load only the explicitly selected sections; defaults retain full-preset API compatibility.
struct InstrumentSettingsSelection {
    bool sample=true,amplitude=true,pitch=true,filter=true,equalizer=true,lfo=true,attack=true,
         tuning=true,mixer=true,routing=true,sendLevels=true,sendShapes=true,macros=true,effects=true;
    bool any() const {return sample||amplitude||pitch||filter||equalizer||lfo||attack||tuning||mixer||routing||sendLevels||sendShapes||macros||effects;}
};
struct SampleEdit {juce::String path;double start=0,end=1,loopStart=0,loopEnd=1,loop=0;};
struct InstrumentControls
{
    double volumeDb = 0.0, pan = 0.0, tuning = 0.0;
    bool mute = false, solo = false;
    int choke = 0;
    SoundParameters sound;
    std::array<MacroMapping,numMacros> macros {};
    std::array<std::shared_ptr<const SendProcessing>,6> sendProcessing{};
    bool operator==(const InstrumentControls& other) const {
        if(volumeDb!=other.volumeDb || pan!=other.pan || tuning!=other.tuning || mute!=other.mute || solo!=other.solo || choke!=other.choke || sound!=other.sound || macros!=other.macros)return false;
        for(int n=0;n<6;++n){const auto& a=sendProcessing[n];const auto& b=other.sendProcessing[n];if(bool(a)!=bool(b) || (a && a!=b && *a!=*b))return false;}return true;
    }
};
// New user-created sounds start with every optional processing stage off.
// The plain controls struct retains legacy defaults for schema migration.
inline InstrumentControls newInstrumentControls(){InstrumentControls c;c.sound.bypassSample=c.sound.bypassAmplitude=c.sound.bypassPitch=c.sound.bypassChain=1;return c;}
// Independent send shaping overrides only envelope, tone-filter and pitch.
inline InstrumentControls controlsForSend(const InstrumentControls& source,int index)
{
    auto result=source;if(index<0 || index>=6 || !source.sendProcessing[index])return result;
    const auto& edit=*source.sendProcessing[index];const auto& s=edit.sound;
    result.tuning=edit.tuning;
    for(const auto& p:soundParameters)if(isSendShapingParameter(p))result.sound.*p.member=s.*p.member;
    for(int n=0;n<3;++n)result.sound.envelopes[n]=s.envelopes[n];
    // Sound-shaping macros belong to the main sound. Mixer/send macros remain shared.
    for(auto& m:result.macros)if(m.target==3 || (m.target>3 && m.target<15))m={};
    return result;
}
// Neutralize only the render copy. Saved edits and bypass switches remain intact.
inline InstrumentControls processingControls(InstrumentControls c)
{
    auto& s=c.sound;const SoundParameters neutral;
    if(s.bypassSample!=0)for(const auto& p:soundParameters)if(p.page==0 && p.member!=&SoundParameters::mode && p.member!=&SoundParameters::voices && p.member!=&SoundParameters::retrigger)s.*p.member=neutral.*p.member;
    if(s.bypassPitch!=0){c.tuning=0;s.detune=s.pitchAmount=s.velocityPitch=s.bendRange=s.lfoPitch=0;}
    return c;
}
constexpr int maxEffects = 16;
constexpr size_t maxEffectStorage = 192u * 1024u * 1024u; // Encoded state, across all kits.
struct EffectSlot
{
    juce::String id, description, state;
    bool bypassed = false;
    int sidechainNote = -1;
    double sidechainGain = 0, sidechainMix = 1;
    bool sidechainMute = false;
    bool operator==(const EffectSlot&) const = default;
};
struct Instrument { juce::String id, name; int note = firstNote; juce::String samplePath; InstrumentControls controls; std::vector<EffectSlot> effects; juce::String notes; };
struct MixerControls { double volumeDb=0,pan=0; bool mute=false,solo=false; int output=0; bool operator==(const MixerControls&) const = default; };
inline bool validMixerControls(const MixerControls& c){return std::isfinite(c.volumeDb) && c.volumeDb>=-70 && c.volumeDb<=12 && std::isfinite(c.pan) && c.pan>=-1 && c.pan<=1 && c.output>=0 && c.output<16;}
struct PatternHit { juce::String instrumentId; int step=0, velocity=100; juce::String id=juce::Uuid().toString(); double start=-1,duration=.225,chance=1; int deviation=0,releaseVelocity=64; bool enabled=true; juce::String chanceGroup; int groupMode=0; double beat() const{return start<0?step*.25:start;} bool operator==(const PatternHit&) const = default; };
struct Pattern { juce::String id,name; int steps=16; std::vector<PatternHit> hits; double loopStart=0,loopEnd=-1; bool looping=true; int numerator=4,denominator=4; double end() const{return steps*.25;} double loopFinish() const{return loopEnd<0?end():loopEnd;} bool operator==(const Pattern&) const = default; };
struct ReturnRouting {std::array<double,6> sends{sendFloor,sendFloor,sendFloor,sendFloor,sendFloor,sendFloor};bool main=true;MixerControls mixer;juce::String group;bool operator==(const ReturnRouting&) const = default;};
struct Group { juce::String id, name; int colour = 0; std::vector<Instrument> instruments; MixerControls mixer;std::array<double,6> sends{sendFloor,sendFloor,sendFloor,sendFloor,sendFloor,sendFloor}; Instrument chain;std::array<Instrument,6> returns;std::array<ReturnRouting,6> returnRouting{}; };
struct Kit { juce::String id, name; std::vector<Group> groups; std::vector<Pattern> patterns; int groupBoundary=0; std::array<Instrument,6> returns = []{std::array<Instrument,6> result;for(int n=0;n<6;++n){result[n].id="return-"+juce::String(n);result[n].name="Send "+juce::String::charToString((juce::juce_wchar)('A'+n));result[n].note=firstNote+maxInstruments+n;}return result;}(); std::array<juce::String,numMacros> macroNames{}; std::array<ReturnRouting,6> returnRouting{}; };
inline bool validReturnRouting(const std::array<ReturnRouting,6>& routes){std::array<int,6> inputs{};std::array<bool,6> used{};for(const auto& route:routes)if(!validMixerControls(route.mixer))return false;for(int a=0;a<6;++a)for(int b=0;b<6;++b){double level=routes[(size_t)a].sends[(size_t)b];if(!std::isfinite(level) || level<sendFloor || level>sendCeiling || (a==b && level>sendFloor))return false;if(level>sendFloor)++inputs[(size_t)b];}for(int step=0;step<6;++step){int node=-1;for(int n=0;n<6;++n)if(!used[(size_t)n] && inputs[(size_t)n]==0){node=n;break;}if(node<0)return false;used[(size_t)node]=true;for(int n=0;n<6;++n)if(routes[(size_t)node].sends[(size_t)n]>sendFloor)--inputs[(size_t)n];}return true;}
inline juce::String sendLabel(const Kit& kit,int index){auto letter=juce::String::charToString((juce::juce_wchar)('A'+index));auto name=kit.returns[(size_t)index].name;return name=="Send "+letter?name:letter+": "+name;}
struct State { std::vector<Kit> kits; juce::String activeKit; uint64_t revision = 0, kitEpoch = 0; };
struct Result
{
    bool ok = false;
    juce::String message, id;
    static Result fail(juce::String message) { return { false, std::move(message), {} }; }
    static Result success(juce::String id = {}) { return { true, {}, std::move(id) }; }
};
int instrumentCount(const Kit&);
const Kit* activeKit(const State&);
const Group* findGroup(const Kit&, const juce::String&);
inline int returnIndex(const juce::String& id){for(int n=0;n<6;++n)if(id=="return-"+juce::String(n))return n;return -1;}
inline constexpr int groupBusBase=maxInstruments+6, effectLanes=groupBusBase+maxGroups*7;
inline juce::String groupChainId(const juce::String& id){return "group:"+id;}
inline juce::String groupReturnId(const juce::String& id,int n){return groupChainId(id)+":return-"+juce::String(n);}
inline const std::array<ReturnRouting,6>* routesFor(const Kit& kit,const juce::String& target,int& index){
    index=returnIndex(target);if(index>=0)return &kit.returnRouting;
    for(const auto& group:kit.groups)for(int n=0;n<6;++n)if(target==groupReturnId(group.id,n)){index=n;return &group.returnRouting;}return nullptr;
}
bool validProcessingRouting(const Kit&);
inline std::vector<const Instrument*> effectTargets(const Kit& kit){std::vector<const Instrument*> result;for(const auto& g:kit.groups)for(const auto& i:g.instruments)result.push_back(&i);for(const auto& r:kit.returns)result.push_back(&r);
#if JUCE_MAC
for(const auto& g:kit.groups){result.push_back(&g.chain);for(const auto& r:g.returns)result.push_back(&r);}
#endif
return result;}
const Instrument* findInstrument(const Kit&, const juce::String&);
juce::String noteName(int);

// Only management/state threads take this lock. The audio callback never uses this model.
class KitLibrary
{
public:
    explicit KitLibrary(bool trackLayout=false);
    State snapshot() const;
    uint64_t currentRevision() const{const juce::ScopedLock guard(lock);return state.revision;}
    uint64_t activeKitEpoch() const { return activeEpoch.load(); }
    Result addKit(bool example = false);
    Result addPattern(const juce::String& kitId);
    Result setPattern(const juce::String& kitId,const Pattern&);
    Result removePattern(const juce::String& kitId,const juce::String& patternId);
    Result duplicateKit(const juce::String&);
    Result removeKit(const juce::String&);
    Result activateKit(const juce::String&);
    Result rename(const juce::String&, const juce::String&);
    Result setGroupBoundary(const juce::String& kitId,int semitones);
    Result addGroup(const juce::String& kitId);
    Result duplicateGroup(const juce::String& kitId, const juce::String& groupId);
    Result removeGroup(const juce::String& kitId, const juce::String& groupId);
    Result reorderGroup(const juce::String& kitId, const juce::String& groupId, int direction);
    Result addInstrument(const juce::String& kitId, const juce::String& groupId);
    Result duplicateInstrument(const juce::String& kitId, const juce::String& instrumentId);
    Result removeInstrument(const juce::String& kitId, const juce::String& instrumentId);
    Result moveInstrument(const juce::String& kitId, const juce::String& instrumentId, const juce::String& groupId);
    Result positionInstrument(const juce::String& kitId, const juce::String& instrumentId, int position);
    Result reorderInstrument(const juce::String& kitId, const juce::String& instrumentId, int direction);
    // Compatibility check: keys are derived from group/instrument order.
    Result assignNote(const juce::String& kitId, const juce::String& instrumentId, int note);
    Result applySampleEdit(const juce::String& kitId,const juce::String& instrumentId,const SampleEdit&);
    Result assignSample(const juce::String& kitId, const juce::String& instrumentId, const juce::String& path);
    Result applyInstrumentSettings(const juce::String& kitId,const juce::String& instrumentId,const Instrument& source,const InstrumentSettingsSelection& selection = {});
    Result setControls(const juce::String& kitId, const juce::String& instrumentId, InstrumentControls);
    Result setGroupMixer(const juce::String& kitId,const juce::String& groupId,MixerControls);
    Result setGroupSend(const juce::String& kitId,const juce::String& groupId,int send,double db);
    Result setSendDestination(const juce::String& kit,const juce::String& target,bool main,const juce::String& group);
    Result setSendRoute(const juce::String& kit,const juce::String& target,int destination,double db);
    Result setReturnMixer(const juce::String& kitId,int index,MixerControls);
    Result setReturnLevel(const juce::String& kitId,int source,int destination,double db);
    Result setReturnMain(const juce::String& kitId,int source,bool enabled);
    Result setReturnName(const juce::String& kitId,int index,const juce::String& name);
    Result setMacroName(const juce::String& kitId,int index,const juce::String& name);
    Result setNotes(const juce::String& kitId,const juce::String& instrumentId,const juce::String& notes);
    Result setEffects(const juce::String& kitId, const juce::String& instrumentId, const std::vector<EffectSlot>&);
    // Capture live effect settings before copying a kit/group/instrument.
    std::function<void()> beforeClone;
    std::function<void()> afterRestore;
    Result importGroupedInstrument(const juce::String& kitId,const Group& sourceGroup,const Instrument& source,bool reuseEmptyGroup=true);
    Result importInstrument(const juce::String& kitId,const juce::String& groupId,const Instrument& source,bool preserveSidechains=false);
    Result addSampleInstrument(const juce::String& kitId, const juce::String& groupId, const juce::String& path);
    Result sliceInstrument(const juce::String& kitId,const juce::String& instrumentId,int regions);
    std::unique_ptr<juce::XmlElement> toXml() const;
    bool restore(const juce::XmlElement&);
    bool applyLiveState(const juce::XmlElement&);
private:
    Result edit(const std::function<Result(State&)>&);
    const bool trackLayout;
    mutable juce::CriticalSection lock;
    State state;
    std::atomic<uint64_t> activeEpoch { 0 };
};
}
