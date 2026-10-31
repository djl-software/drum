#include "KitLibrary.h"
#include "BuiltinEffects.h"
#include <algorithm>
#include <array>
#include <set>
#include <cmath>
#include <cstdlib>

namespace drum
{
namespace
{
juce::String newId() { return juce::Uuid().toString(); }
Group makeGroup(juce::String name, int colour = 0) { return { newId(), std::move(name), colour, {} }; }
Kit makeKit(juce::String name) { return { newId(), std::move(name), { makeGroup("Group 1") } }; }
template <typename T> T* find(std::vector<T>& values, const juce::String& id)
{
    for (auto& value : values) if (value.id == id) return &value;
    return nullptr;
}
int freeNote(const Kit& kit, int preferred = firstNote)
{
    std::array<bool, maxInstruments> used {};
    for (auto& group : kit.groups) for (auto& instrument : group.instruments) used[(size_t)(instrument.note - firstNote)] = true;
    if (preferred >= firstNote && preferred < firstNote + maxInstruments && !used[(size_t)(preferred - firstNote)]) return preferred;
    for (int i = 0; i < maxInstruments; ++i) if (!used[(size_t)i]) return i + firstNote;
    return -1;
}
// Notes are derived from the flattened group/pad order. Resolve sidechain
// references through the previous note map before reusing any vacated key.
void compactKeyboard(Kit& kit)
{
    std::array<int,128> moved;moved.fill(-1);int next=firstNote;
    for(auto& group:kit.groups)for(auto& instrument:group.instruments)moved[(size_t)instrument.note]=next++;
    next=firstNote;
    for(auto& group:kit.groups)for(auto& instrument:group.instruments){
        instrument.note=next++;
        for(auto& effect:instrument.effects)if(effect.sidechainNote>=0){
            effect.sidechainNote=moved[(size_t)effect.sidechainNote];
            if(effect.sidechainNote<0)effect.sidechainMute=false;
        }
    }
}
std::array<juce::String,128> keyboardLayout(const Kit* kit)
{
    std::array<juce::String,128> result{};
    if(kit)for(const auto& group:kit->groups)for(const auto& instrument:group.instruments)result[(size_t)instrument.note]=instrument.id;
    return result;
}
Group* containing(Kit& kit, const juce::String& id)
{
    for (auto& group : kit.groups) if (find(group.instruments, id)) return &group;
    return nullptr;
}
bool validName(const juce::String& name) { return name.isNotEmpty() && name == name.trim() && name.length() <= 64; }
bool validControls(const InstrumentControls& c)
{
    for(auto& m:c.macros) if(!validMapping(m)) return false;
    return std::isfinite(c.volumeDb) && c.volumeDb >= -60 && c.volumeDb <= 12
        && std::isfinite(c.pan) && c.pan >= -1 && c.pan <= 1
        && std::isfinite(c.tuning) && c.tuning >= -48 && c.tuning <= 48 && c.choke >= 0 && c.choke <= 16
        && validSound(c.sound);
}
double numberAttribute(const juce::XmlElement& xml, const char* name)
{
    auto text=xml.getStringAttribute(name); auto* start=text.toRawUTF8(); char* end=nullptr;
    const double value=std::strtod(start,&end);
    return end!=start && *end=='\0' ? value : std::numeric_limits<double>::quiet_NaN();
}
bool validate(const State& state)
{
    if (state.kits.empty() || state.kits.size() > maxKits || !activeKit(state)) return false;
    std::set<juce::String> ids;
    size_t effectStorage=0;
    const auto validId = [&ids](const juce::String& id) { return id.isNotEmpty() && id.length() <= 64 && ids.insert(id).second; };
    for (const auto& kit : state.kits)
    {
        if (!validId(kit.id) || !validName(kit.name) || kit.groups.empty() || kit.groups.size() > maxGroups || instrumentCount(kit) > maxInstruments) return false;
        if(kit.patterns.size()>16)return false;
        for(const auto& pattern:kit.patterns){
            if(!validId(pattern.id) || !validName(pattern.name) || (pattern.steps<1 || pattern.steps>4096) || pattern.hits.size()>8192 || !std::isfinite(pattern.loopStart) || !std::isfinite(pattern.loopEnd) || pattern.loopStart<0 || pattern.loopFinish()<=pattern.loopStart || pattern.loopFinish()>pattern.end() || pattern.numerator<1 || pattern.numerator>32 || (pattern.denominator!=1 && pattern.denominator!=2 && pattern.denominator!=4 && pattern.denominator!=8 && pattern.denominator!=16))return false;
            std::set<juce::String> noteIds;
            for(const auto& hit:pattern.hits)if(!findInstrument(kit,hit.instrumentId) || !std::isfinite(hit.start) || !std::isfinite(hit.duration) || !std::isfinite(hit.chance) || hit.beat()<0 || hit.beat()>=pattern.end() || hit.duration<=0 || hit.duration>pattern.end() || hit.beat()+hit.duration>pattern.end()+1.e-8 || hit.chance<0 || hit.chance>1 || hit.groupMode<0 || hit.groupMode>2 || hit.chanceGroup.length()>64 || (hit.groupMode>0 && hit.chanceGroup.isEmpty()) || hit.deviation<0 || hit.deviation>127 || hit.releaseVelocity<0 || hit.releaseVelocity>127 || hit.velocity<1 || hit.velocity>127 || hit.id.isEmpty() || hit.id.length()>64 || !noteIds.insert(hit.id).second)return false;
        }
        std::set<int> notes;
        for (const auto& group : kit.groups)
        {
            if (!validId(group.id) || !validName(group.name) || group.colour < 0 || group.colour >= 8) return false;
            for (const auto& instrument : group.instruments)
            {
                if (!validId(instrument.id) || !validName(instrument.name) || instrument.note < firstNote
                    || instrument.note >= firstNote + maxInstruments || !notes.insert(instrument.note).second
                    || !validControls(instrument.controls) || instrument.samplePath.length() > 32767
                    || (instrument.samplePath.isNotEmpty() && !juce::File::isAbsolutePath(instrument.samplePath))) return false;
                if(instrument.effects.size()>maxEffects)return false;
                for(const auto& effect:instrument.effects)
                {
                    if((effect.sidechainNote!=-1 && (effect.sidechainNote<firstNote || effect.sidechainNote>=firstNote+maxInstruments))
                        || !std::isfinite(effect.sidechainGain) || effect.sidechainGain < -60 || effect.sidechainGain>24
                        || !std::isfinite(effect.sidechainMix) || effect.sidechainMix<0 || effect.sidechainMix>1)return false;
                    if(!validId(effect.id) || effect.description.length()>65536 || effect.description.isEmpty()
                        || effect.state.length()>24*1024*1024 || !juce::parseXML(effect.description))return false;
                    effectStorage+=effect.description.getNumBytesAsUTF8()+effect.state.getNumBytesAsUTF8();
                    if(effectStorage>maxEffectStorage)return false;
                }
            }
        }
    }
    return true;
}
int integerAttribute(const juce::XmlElement& element, const char* name)
{
    const auto value = element.getStringAttribute(name);
    return value.isNotEmpty() && value.length() <= 3 && value.containsOnly("0123456789") ? value.getIntValue() : -1;
}
}
int instrumentCount(const Kit& kit) { int n = 0; for (auto& g : kit.groups) n += (int)g.instruments.size(); return n; }
const Kit* activeKit(const State& state) { for (auto& k : state.kits) if (k.id == state.activeKit) return &k; return nullptr; }
const Group* findGroup(const Kit& kit, const juce::String& id) { for (auto& g : kit.groups) if (g.id == id) return &g; return nullptr; }
const Instrument* findInstrument(const Kit& kit, const juce::String& id)
{
    for (auto& g : kit.groups) for (auto& i : g.instruments) if (i.id == id) return &i;
    return nullptr;
}
juce::String noteName(int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    // Match Live's trigger-note convention: MIDI 60 is C3, MIDI 21 is A-1.
    return juce::String(names[note % 12]) + juce::String(note / 12 - 2);
}
KitLibrary::KitLibrary(bool track):trackLayout(track) { state.kits.push_back(makeKit("Kit 1")); state.activeKit = state.kits.front().id; }
State KitLibrary::snapshot() const { const juce::ScopedLock guard(lock); return state; }
Result KitLibrary::edit(const std::function<Result(State&)>& operation)
{
    const juce::ScopedLock guard(lock);
    auto candidate = state;
    auto result = operation(candidate);
    if (!result.ok) return result;
    for(auto& kit:candidate.kits)for(auto& pattern:kit.patterns)std::erase_if(pattern.hits,[&](const auto& hit){return findInstrument(kit,hit.instrumentId)==nullptr;});
    if (!validate(candidate)) return Result::fail("This change would exceed a limit or create an invalid assignment.");
    for(auto& kit:candidate.kits){compactKeyboard(kit);}
    candidate.revision = state.revision + 1;
    const bool keyboardChanged=keyboardLayout(activeKit(candidate))!=keyboardLayout(activeKit(state));
    candidate.kitEpoch = state.kitEpoch + (candidate.activeKit != state.activeKit || keyboardChanged ? 1 : 0);
    state = std::move(candidate); activeEpoch.store(state.kitEpoch);
    return result;
}
Result KitLibrary::addPattern(const juce::String& kitId){return edit([&](State& state){auto* kit=find(state.kits,kitId);if(!kit || kit->patterns.size()>=16)return Result::fail("A kit can contain up to 16 patterns.");Pattern pattern{newId(),"Clip "+juce::String((int)kit->patterns.size()+1),16,{}};kit->patterns.push_back(pattern);return Result::success(pattern.id);});}
Result KitLibrary::setPattern(const juce::String& kitId,const Pattern& pattern){return edit([&](State& state){auto* kit=find(state.kits,kitId);auto* target=kit?find(kit->patterns,pattern.id):nullptr;if(!target)return Result::fail("Pattern no longer exists.");*target=pattern;return Result::success(pattern.id);});}
Result KitLibrary::removePattern(const juce::String& kitId,const juce::String& id){return edit([&](State& state){auto* kit=find(state.kits,kitId);if(!kit || !find(kit->patterns,id))return Result::fail("Pattern no longer exists.");std::erase_if(kit->patterns,[&](const auto& p){return p.id==id;});return Result::success();});}
Result KitLibrary::addKit(bool example)
{
    return edit([=](State& s) {
        if (s.kits.size() == maxKits) return Result::fail("All 8 kit slots are in use.");
        auto kit = makeKit(example ? "Studio Kit" : "Kit " + juce::String((int)s.kits.size() + 1));
        if (example)
        {
            kit.groups.clear();
            const char* groupNames[] = { "Kicks", "Snares & claps", "Hi-hats", "Percussion" };
            const char* names[][3] = { { "Deep Kick", "Punch Kick", "Acoustic Kick" }, { "Dry Snare", "Rimshot", "Wide Clap" },
                                      { "Closed Hat", "Open Hat", "Pedal Hat" }, { "Low Tom", "Shaker", "Tambourine" } };
            int note = firstNote;
            for (int g = 0; g < 4; ++g)
            {
                auto group = makeGroup(groupNames[g], g);
                for (auto name : names[g]) group.instruments.push_back({ newId(), name, note++ });
                kit.groups.push_back(std::move(group));
            }
        }
        s.activeKit = kit.id; s.kits.push_back(kit); return Result::success(kit.id);
    });
}
Result KitLibrary::duplicateKit(const juce::String& id)
{
    if(beforeClone)beforeClone();
    return edit([&](State& s) {
        auto* source = find(s.kits, id);
        if (!source) return Result::fail("Kit no longer exists.");
        if (s.kits.size() == maxKits) return Result::fail("All 8 kit slots are in use.");
        auto copy = *source; copy.id = newId(); copy.name = copy.name.substring(0, 59) + " copy";
        for (auto& group : copy.groups) { group.id = newId(); for (auto& i : group.instruments) { auto old=i.id;i.id = newId();for(auto& pattern:copy.patterns)for(auto& hit:pattern.hits)if(hit.instrumentId==old)hit.instrumentId=i.id;for(auto& effect:i.effects)effect.id=newId(); } }
        for(auto& pattern:copy.patterns)pattern.id=newId();
        s.activeKit = copy.id; s.kits.push_back(copy); return Result::success(copy.id);
    });
}
Result KitLibrary::removeKit(const juce::String& id)
{
    return edit([&](State& s) {
        if (s.kits.size() == 1) return Result::fail("Keep at least one kit.");
        if (!find(s.kits, id)) return Result::fail("Kit no longer exists.");
        std::erase_if(s.kits, [&](const Kit& k) { return k.id == id; });
        if (s.activeKit == id) s.activeKit = s.kits.front().id;
        return Result::success(s.activeKit);
    });
}
Result KitLibrary::activateKit(const juce::String& id)
{
    return edit([&](State& s) { if (!find(s.kits, id)) return Result::fail("Kit no longer exists."); s.activeKit = id; return Result::success(id); });
}
Result KitLibrary::rename(const juce::String& id, const juce::String& text)
{
    return edit([&](State& s) {
        auto name = text.trim(); if (!validName(name)) return Result::fail("Use a name between 1 and 64 characters.");
        for (auto& k : s.kits)
        {
            if (k.id == id) { k.name = name; return Result::success(id); }
            for (auto& g : k.groups)
            {
                if (g.id == id) { g.name = name; return Result::success(id); }
                if (auto* i = find(g.instruments, id)) { i->name = name; return Result::success(id); }
            }
        }
        return Result::fail("Item no longer exists.");
    });
}
Result KitLibrary::addGroup(const juce::String& kitId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        if (k->groups.size() == maxGroups) return Result::fail("This kit already has 16 groups.");
        auto g = makeGroup("Group " + juce::String((int)k->groups.size() + 1), (int)k->groups.size() % 8);
        k->groups.push_back(g); return Result::success(g.id);
    });
}
Result KitLibrary::duplicateGroup(const juce::String& kitId, const juce::String& groupId)
{
    if(beforeClone)beforeClone();
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = find(k->groups, groupId); if (!g) return Result::fail("Group no longer exists.");
        if (k->groups.size() == maxGroups || instrumentCount(*k) + (int)g->instruments.size() > maxInstruments) return Result::fail("Not enough group or instrument slots to copy this group.");
        auto copy = *g; copy.id = newId(); copy.name = copy.name.substring(0,59) + " copy"; copy.instruments.clear();
        auto sourceInstruments = g->instruments;
        k->groups.push_back(copy);
        for (auto i : sourceInstruments) { i.id = newId(); for(auto& effect:i.effects)effect.id=newId(); i.note = freeNote(*k); k->groups.back().instruments.push_back(i); }
        return Result::success(copy.id);
    });
}
Result KitLibrary::removeGroup(const juce::String& kitId, const juce::String& groupId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        if (k->groups.size() == 1) return Result::fail("Keep at least one group in each kit.");
        if (!find(k->groups, groupId)) return Result::fail("Group no longer exists.");
        std::erase_if(k->groups, [&](const Group& g) { return g.id == groupId; }); return Result::success();
    });
}
Result KitLibrary::reorderGroup(const juce::String& kitId, const juce::String& groupId, int direction)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        for (int n = 0; n < (int)k->groups.size(); ++n) if (k->groups[(size_t)n].id == groupId)
        {
            int to = n + direction; if (to < 0 || to >= (int)k->groups.size()) return Result::fail("Group is already at that end.");
            std::swap(k->groups[(size_t)n], k->groups[(size_t)to]); return Result::success(groupId);
        }
        return Result::fail("Group no longer exists.");
    });
}
Result KitLibrary::addInstrument(const juce::String& kitId, const juce::String& groupId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = find(k->groups, groupId); if (!g) return Result::fail("Group no longer exists.");
        auto note = freeNote(*k, g->instruments.empty() ? firstNote : g->instruments.back().note + 1);
        if (note < 0) return Result::fail("This kit already has 88 instruments.");
        Instrument i { newId(), "Instrument " + juce::String(instrumentCount(*k) + 1), note };
        i.effects=defaultChain();
        g->instruments.push_back(i); return Result::success(i.id);
    });
}
Result KitLibrary::duplicateInstrument(const juce::String& kitId, const juce::String& instrumentId)
{
    if(beforeClone)beforeClone();
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        auto note = freeNote(*k); if (note < 0) return Result::fail("This kit already has 88 instruments.");
        auto copy = *find(g->instruments, instrumentId); copy.id = newId(); copy.name = copy.name.substring(0,59) + " copy"; copy.note = note; for(auto& effect:copy.effects)effect.id=newId();
        g->instruments.push_back(copy); return Result::success(copy.id);
    });
}
Result KitLibrary::removeInstrument(const juce::String& kitId, const juce::String& instrumentId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        std::erase_if(g->instruments, [&](const Instrument& i) { return i.id == instrumentId; }); return Result::success();
    });
}
Result KitLibrary::moveInstrument(const juce::String& kitId, const juce::String& instrumentId, const juce::String& groupId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* from = containing(*k, instrumentId); auto* to = find(k->groups, groupId);
        if (!from || !to) return Result::fail("Instrument or group no longer exists.");
        if (from == to) return Result::success(instrumentId);
        auto copy = *find(from->instruments, instrumentId); to->instruments.push_back(copy);
        std::erase_if(from->instruments, [&](const Instrument& i) { return i.id == instrumentId; }); return Result::success(instrumentId);
    });
}
Result KitLibrary::reorderInstrument(const juce::String& kitId, const juce::String& instrumentId, int direction)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        for (int n = 0; n < (int)g->instruments.size(); ++n) if (g->instruments[(size_t)n].id == instrumentId)
        {
            int to = n + direction; if (to < 0 || to >= (int)g->instruments.size()) return Result::fail("Instrument is already at that end.");
            std::swap(g->instruments[(size_t)n], g->instruments[(size_t)to]); return Result::success(instrumentId);
        }
        return Result::fail("Instrument no longer exists.");
    });
}
Result KitLibrary::assignNote(const juce::String& kitId, const juce::String& instrumentId, int note)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        if(find(g->instruments,instrumentId)->note!=note)return Result::fail("Keyboard keys follow kit order. Move the instrument or its group to change its key.");
        return Result::success(instrumentId);
    });
}
Result KitLibrary::assignSample(const juce::String& kitId, const juce::String& instrumentId, const juce::String& path)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        auto* instrument = find(g->instruments, instrumentId);
        const auto suffix = instrument->name.fromFirstOccurrenceOf("Instrument ", false, false);
        const bool placeholder = instrument->name.startsWith("Instrument ")
            && suffix.isNotEmpty() && suffix.containsOnly("0123456789");
        const auto previousName = juce::File::isAbsolutePath(instrument->samplePath)
            ? juce::File(instrument->samplePath).getFileNameWithoutExtension().trim().substring(0,64) : juce::String();
        if (juce::File::isAbsolutePath(path) && (placeholder || instrument->name == previousName))
        {
            auto name = juce::File(path).getFileNameWithoutExtension().trim().substring(0,64);
            if (name.isNotEmpty()) instrument->name = name;
        }
        instrument->samplePath = path;
        return Result::success(instrumentId);
    });
}
Result KitLibrary::setControls(const juce::String& kitId, const juce::String& instrumentId, InstrumentControls controls)
{
    return edit([&](State& s) {
        if(!validControls(controls)) return Result::fail("Invalid instrument controls.");
        auto* k=find(s.kits,kitId); if(!k) return Result::fail("Kit no longer exists.");
        auto* g=containing(*k,instrumentId); if(!g) return Result::fail("Instrument no longer exists.");
        find(g->instruments,instrumentId)->controls=controls; return Result::success(instrumentId);
    });
}
Result KitLibrary::importGroupedInstrument(const juce::String& kitId,const Group& sourceGroup,const Instrument& source)
{
    return edit([&](State& state){
        auto* kit=find(state.kits,kitId);if(!kit)return Result::fail("Open or create a kit first.");
        int note=freeNote(*kit);if(note<0)return Result::fail("This kit already has 88 instruments.");
        Group* destination=nullptr;for(auto& group:kit->groups)if(group.name==sourceGroup.name){destination=&group;break;}
        if(!destination){
            if(instrumentCount(*kit)==0 && kit->groups.size()==1){destination=&kit->groups.front();destination->name=sourceGroup.name;destination->colour=sourceGroup.colour;}
            else {if(kit->groups.size()>=maxGroups)return Result::fail("This kit already has 16 groups.");kit->groups.push_back(makeGroup(sourceGroup.name,sourceGroup.colour));destination=&kit->groups.back();}
        }
        auto copy=source;copy.id=newId();copy.note=note;for(auto& effect:copy.effects){effect.id=newId();effect.sidechainNote=-1;effect.sidechainMute=false;}
        destination->instruments.push_back(std::move(copy));return Result::success(destination->instruments.back().id);
    });
}
Result KitLibrary::importInstrument(const juce::String& kitId,const juce::String& groupId,const Instrument& source)
{
    return edit([&](State& state){auto* kit=find(state.kits,kitId);if(!kit)return Result::fail("Track no longer exists.");auto* group=find(kit->groups,groupId);if(!group)return Result::fail("Track group no longer exists.");int note=freeNote(*kit);if(note<0)return Result::fail("This track already has 88 sounds.");auto copy=source;copy.id=newId();copy.note=note;for(auto& effect:copy.effects){effect.id=newId();effect.sidechainNote=-1;effect.sidechainMute=false;}group->instruments.push_back(copy);return Result::success(copy.id);});
}
Result KitLibrary::addSampleInstrument(const juce::String& kitId, const juce::String& groupId, const juce::String& path)
{
    return edit([&](State& s) {
        auto* k=find(s.kits,kitId); if(!k) return Result::fail("Kit no longer exists.");
        auto* g=find(k->groups,groupId); if(!g) return Result::fail("Group no longer exists.");
        auto note=freeNote(*k,g->instruments.empty()?firstNote:g->instruments.back().note+1);
        if(note<0) return Result::fail("This kit already has 88 instruments.");
        if(!juce::File::isAbsolutePath(path)) return Result::fail("The dropped file has no absolute path.");
        auto name=juce::File(path).getFileNameWithoutExtension().trim().substring(0,64);
        Instrument i{newId(),name.isEmpty()?"Sample":name,note,path};
        i.effects=defaultChain();
        g->instruments.push_back(i); return Result::success(i.id);
    });
}
Result KitLibrary::sliceInstrument(const juce::String& kitId,const juce::String& instrumentId,int regions)
{
    if(beforeClone)beforeClone();
    return edit([&](State& s) {
        auto* kit=find(s.kits,kitId); if(!kit) return Result::fail("Kit no longer exists.");
        auto* source=findInstrument(*kit,instrumentId);
        if(!source || source->samplePath.isEmpty()) return Result::fail("Load a sample before slicing.");
        if(regions<2 || regions>64 || instrumentCount(*kit)+regions>maxInstruments || kit->groups.size()>=maxGroups)
            return Result::fail("Slicing needs a free group and enough free instrument slots (up to 88 per kit).");
        const auto original=*source; auto group=makeGroup((original.name.substring(0,52)+" slices"));
        const double width=(original.controls.sound.end-original.controls.sound.start)/regions;
        // Add the group before allocating notes so each slice reserves its own key.
        kit->groups.push_back(std::move(group)); auto& target=kit->groups.back();
        for(int n=0;n<regions;++n)
        {
            auto slice=original; slice.id=newId(); for(auto& effect:slice.effects)effect.id=newId(); slice.name=original.name.substring(0,52)+" "+juce::String(n+1);
            slice.note=freeNote(*kit); slice.controls.sound.start=original.controls.sound.start+n*width;
            slice.controls.sound.end=original.controls.sound.start+(n+1)*width;
            slice.controls.sound.loop=0; slice.controls.sound.mode=0;
            target.instruments.push_back(std::move(slice));
        }
        return Result::success(target.id);
    });
}
Result KitLibrary::setEffects(const juce::String& kitId,const juce::String& instrumentId,const std::vector<EffectSlot>& effects)
{
    return edit([&](State& state) {
        auto* kit=find(state.kits,kitId);if(!kit)return Result::fail("Kit no longer exists.");
        auto* group=containing(*kit,instrumentId);if(!group)return Result::fail("Instrument no longer exists.");
        find(group->instruments,instrumentId)->effects=effects;return Result::success(instrumentId);
    });
}
std::unique_ptr<juce::XmlElement> KitLibrary::toXml() const
{
    const auto s = snapshot(); auto xml = std::make_unique<juce::XmlElement>("DrumState");
    xml->setAttribute("schemaVersion", 14); xml->setAttribute("activeKit", s.activeKit);
    for (auto& k : s.kits)
    {
        auto* kit = xml->createNewChildElement("Kit"); kit->setAttribute("id", k.id); kit->setAttribute("name", k.name);
        for(const auto& p:k.patterns){auto* pattern=kit->createNewChildElement("Pattern");pattern->setAttribute("id",p.id);pattern->setAttribute("name",p.name);pattern->setAttribute("steps",p.steps);pattern->setAttribute("loopStart",p.loopStart);pattern->setAttribute("loopEnd",p.loopEnd);pattern->setAttribute("looping",p.looping);pattern->setAttribute("numerator",p.numerator);pattern->setAttribute("denominator",p.denominator);for(const auto& hit:p.hits){auto* note=pattern->createNewChildElement("Hit");note->setAttribute("instrument",hit.instrumentId);note->setAttribute("step",hit.step);note->setAttribute("velocity",hit.velocity);note->setAttribute("id",hit.id);note->setAttribute("start",hit.start);note->setAttribute("duration",hit.duration);note->setAttribute("chance",hit.chance);note->setAttribute("deviation",hit.deviation);note->setAttribute("releaseVelocity",hit.releaseVelocity);note->setAttribute("enabled",hit.enabled);note->setAttribute("chanceGroup",hit.chanceGroup);note->setAttribute("groupMode",hit.groupMode);}}
        for (auto& g : k.groups)
        {
            auto* group = kit->createNewChildElement("Group"); group->setAttribute("id", g.id); group->setAttribute("name", g.name); group->setAttribute("colour", g.colour);
            for (auto& i : g.instruments)
            {
                auto* instrument = group->createNewChildElement("Instrument"); instrument->setAttribute("id", i.id);
                instrument->setAttribute("name", i.name); instrument->setAttribute("note", i.note);
                instrument->setAttribute("samplePath", i.samplePath);
                instrument->setAttribute("volumeDb",i.controls.volumeDb); instrument->setAttribute("pan",i.controls.pan);
                instrument->setAttribute("tuning",i.controls.tuning); instrument->setAttribute("mute",i.controls.mute?1:0);
                instrument->setAttribute("solo",i.controls.solo?1:0); instrument->setAttribute("choke",i.controls.choke);
                for (auto& p : soundParameters) instrument->setAttribute(juce::String("edit_")+p.key,i.controls.sound.*(p.member));
                const auto& eq=i.controls.sound.equalizer;
                instrument->setAttribute("eqOn",eq.enabled?1:0);instrument->setAttribute("eqOutput",juce::String(eq.output,17,true));
                for(int n=0;n<8;++n)
                {
                    const auto prefix="eq"+juce::String(n);const auto& b=eq.bands[(size_t)n];
                    instrument->setAttribute(prefix+"on",b.enabled?1:0);instrument->setAttribute(prefix+"type",b.type);
                    instrument->setAttribute(prefix+"hz",juce::String(b.frequency,17,true));instrument->setAttribute(prefix+"gain",juce::String(b.gain,17,true));instrument->setAttribute(prefix+"q",juce::String(b.q,17,true));
                }
                for(int n=0;n<3;++n)if(const auto& shape=i.controls.sound.envelopes[(size_t)n];shape.count)
                {
                    auto* envelope=instrument->createNewChildElement("Envelope");envelope->setAttribute("index",n);envelope->setAttribute("hold",shape.hold);
                    for(int point=0;point<shape.count;++point)
                    {
                        const auto& p=shape.points[(size_t)point];auto* node=envelope->createNewChildElement("Point");
                        node->setAttribute("ms",juce::String(p.ms,17,true));node->setAttribute("level",juce::String(p.level,17,true));node->setAttribute("bend",juce::String(p.bend,17,true));node->setAttribute("pivot",juce::String(p.pivot,17,true));node->setAttribute("weight",juce::String(p.weight,17,true));
                    }
                }
                for(const auto& effect:i.effects)
                {
                    auto* node=instrument->createNewChildElement("Effect");node->setAttribute("id",effect.id);
                    node->setAttribute("bypassed",effect.bypassed?1:0);
                    node->setAttribute("sidechainNote",effect.sidechainNote);node->setAttribute("sidechainGain",effect.sidechainGain);
                    node->setAttribute("sidechainMix",effect.sidechainMix);node->setAttribute("sidechainMute",effect.sidechainMute);
                    node->addChildElement(juce::parseXML(effect.description).release());
                    node->createNewChildElement("State")->addTextElement(effect.state);
                }
                for(int n=0;n<numMacros;++n)
                {
                    const auto key="map"+juce::String(n); const auto& m=i.controls.macros[(size_t)n];
                    instrument->setAttribute(key+"target",m.target);instrument->setAttribute(key+"min",m.minimum);instrument->setAttribute(key+"max",m.maximum);
                }
            }
        }
    }
    return xml;
}
bool KitLibrary::restore(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("DrumState")) return false;
    const auto version = integerAttribute(xml, "schemaVersion");
    if (version == 1 && xml.getNumChildElements() == 0)
    {
        KitLibrary initial; auto fresh = initial.snapshot(); const juce::ScopedLock guard(lock);
        fresh.revision = state.revision + 1; fresh.kitEpoch = state.kitEpoch + 1; state = std::move(fresh); activeEpoch.store(state.kitEpoch); if(afterRestore)afterRestore(); return true;
    }
    if (version < 2 || version > 14) return false;
    State candidate; candidate.activeKit = xml.getStringAttribute("activeKit");
    for (auto* k : xml.getChildIterator())
    {
        if (!k->hasTagName("Kit") || candidate.kits.size() == maxKits) return false;
        Kit kit { k->getStringAttribute("id"), k->getStringAttribute("name"), {} };
        int count = 0;
        for (auto* g : k->getChildIterator())
        {
            if(version>=13 && g->hasTagName("Pattern")){
                Pattern pattern{g->getStringAttribute("id"),g->getStringAttribute("name"),integerAttribute(*g,"steps"),{}};
                if(kit.patterns.size()>=16 || g->getNumChildElements()>8192)return false;
                for(auto* hit:g->getChildIterator()){if(!hit->hasTagName("Hit") || hit->getNumChildElements())return false;pattern.hits.push_back({hit->getStringAttribute("instrument"),integerAttribute(*hit,"step"),integerAttribute(*hit,"velocity")});if(version<14)pattern.hits.back().id=pattern.id+"-note-"+juce::String((int)pattern.hits.size());if(version>=14){auto& h=pattern.hits.back();h.id=hit->getStringAttribute("id");h.start=numberAttribute(*hit,"start");h.duration=numberAttribute(*hit,"duration");h.chance=numberAttribute(*hit,"chance");h.deviation=integerAttribute(*hit,"deviation");h.releaseVelocity=integerAttribute(*hit,"releaseVelocity");h.enabled=hit->getBoolAttribute("enabled",true);h.chanceGroup=hit->getStringAttribute("chanceGroup");h.groupMode=hit->getIntAttribute("groupMode",0);}}
                if(version>=14){pattern.loopStart=numberAttribute(*g,"loopStart");pattern.loopEnd=numberAttribute(*g,"loopEnd");pattern.looping=g->getBoolAttribute("looping",true);pattern.numerator=integerAttribute(*g,"numerator");pattern.denominator=integerAttribute(*g,"denominator");}kit.patterns.push_back(std::move(pattern));continue;
            }
            if (!g->hasTagName("Group") || kit.groups.size() == maxGroups) return false;
            Group group { g->getStringAttribute("id"), g->getStringAttribute("name"), integerAttribute(*g, "colour"), {} };
            for (auto* i : g->getChildIterator())
            {
                if (!i->hasTagName("Instrument") || ++count > maxInstruments || (version<7 && i->getNumChildElements()!=0)) return false;
                group.instruments.push_back({ i->getStringAttribute("id"), i->getStringAttribute("name"), integerAttribute(*i, "note"), version >= 3 ? i->getStringAttribute("samplePath") : juce::String() });
                if(version>=4)
                {
                    auto mute=integerAttribute(*i,"mute"),solo=integerAttribute(*i,"solo");
                    if(mute<0 || mute>1 || solo<0 || solo>1) return false;
                    group.instruments.back().controls={numberAttribute(*i,"volumeDb"),numberAttribute(*i,"pan"),numberAttribute(*i,"tuning"),mute!=0,solo!=0,integerAttribute(*i,"choke")};
                    if(version>=8)
                    {
                        auto& eq=group.instruments.back().controls.sound.equalizer;
                        const auto on=integerAttribute(*i,"eqOn");if(on<0 || on>1)return false;
                        eq.enabled=on!=0;eq.output=numberAttribute(*i,"eqOutput");
                        for(int n=0;n<8;++n)
                        {
                            const auto prefix="eq"+juce::String(n);auto& b=eq.bands[(size_t)n];
                            const auto bandOn=integerAttribute(*i,(prefix+"on").toRawUTF8());if(bandOn<0 || bandOn>1)return false;
                            b.enabled=bandOn!=0;b.type=integerAttribute(*i,(prefix+"type").toRawUTF8());
                            b.frequency=numberAttribute(*i,(prefix+"hz").toRawUTF8());b.gain=numberAttribute(*i,(prefix+"gain").toRawUTF8());b.q=numberAttribute(*i,(prefix+"q").toRawUTF8());
                        }
                    }
                    if(version>=5) for(auto& p:soundParameters)
                        if((version>=6 || !introducedInVersion6(p)) && (version>=7 || p.member!=&SoundParameters::keyOverride) && (version>=10 || p.member!=&SoundParameters::attackPosition))
                            group.instruments.back().controls.sound.*(p.member)=numberAttribute(*i,(juce::String("edit_")+p.key).toRawUTF8());
                    if(version>=5) for(int n=0;n<numMacros;++n)
                    {
                        auto& m=group.instruments.back().controls.macros[(size_t)n]; const auto key="map"+juce::String(n);
                        m={integerAttribute(*i,(key+"target").toRawUTF8()),numberAttribute(*i,(key+"min").toRawUTF8()),numberAttribute(*i,(key+"max").toRawUTF8())};
                    }
                    if(version>=7)
                    {
                        std::array<bool,3> seen {};
                        for(auto* e:i->getChildIterator())
                        {
                            if(version>=11 && e->hasTagName("Effect"))
                            {
                                auto* description=e->getChildByName("PLUGIN");auto* saved=e->getChildByName("State");
                                const int bypassed=integerAttribute(*e,"bypassed");
                                if(!description || !saved || e->getNumChildElements()!=2 || bypassed<0 || bypassed>1)return false;
                                auto encoded=saved->getAllSubText();juce::MemoryBlock decoded;
                                if(encoded.length()>24*1024*1024 || (encoded.isNotEmpty() && !decoded.fromBase64Encoding(encoded)))return false;
                                group.instruments.back().effects.push_back({e->getStringAttribute("id"),description->toString(),encoded,bypassed!=0});
                                auto& effect=group.instruments.back().effects.back();effect.sidechainNote=e->getIntAttribute("sidechainNote",-1);
                                effect.sidechainGain=e->getDoubleAttribute("sidechainGain",0);effect.sidechainMix=e->getDoubleAttribute("sidechainMix",1);
                                effect.sidechainMute=e->getBoolAttribute("sidechainMute",false);
                                continue;
                            }
                            const int index=integerAttribute(*e,"index");
                            if(!e->hasTagName("Envelope") || index<0 || index>2 || seen[(size_t)index] || e->getNumChildElements()<2 || e->getNumChildElements()>maxEnvelopePoints)return false;
                            seen[(size_t)index]=true;auto& shape=group.instruments.back().controls.sound.envelopes[(size_t)index];shape.hold=integerAttribute(*e,"hold");
                            for(auto* p:e->getChildIterator())
                            {
                                if(!p->hasTagName("Point") || p->getNumChildElements()!=0)return false;
                                shape.points[(size_t)shape.count++]={numberAttribute(*p,"ms"),numberAttribute(*p,"level"),numberAttribute(*p,"bend"),numberAttribute(*p,"pivot"),version>=9?numberAttribute(*p,"weight"):1.0};
                            }
                        }
                    }
                }
            }
            kit.groups.push_back(std::move(group));
        }
        candidate.kits.push_back(std::move(kit));
    }
    if (!validate(candidate)) return false;
    for(auto& kit:candidate.kits){compactKeyboard(kit);}
    const juce::ScopedLock guard(lock); candidate.revision = state.revision + 1; candidate.kitEpoch = state.kitEpoch + 1; state = std::move(candidate); activeEpoch.store(state.kitEpoch); if(afterRestore)afterRestore(); return true;
}
}
