#include "KitLibrary.h"
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
    const auto validId = [&ids](const juce::String& id) { return id.isNotEmpty() && id.length() <= 64 && ids.insert(id).second; };
    for (const auto& kit : state.kits)
    {
        if (!validId(kit.id) || !validName(kit.name) || kit.groups.empty() || kit.groups.size() > maxGroups || instrumentCount(kit) > maxInstruments) return false;
        std::set<int> notes;
        for (const auto& group : kit.groups)
        {
            if (!validId(group.id) || !validName(group.name) || group.colour < 0 || group.colour >= 8) return false;
            for (const auto& instrument : group.instruments)
                if (!validId(instrument.id) || !validName(instrument.name) || instrument.note < firstNote
                    || instrument.note >= firstNote + maxInstruments || !notes.insert(instrument.note).second
                    || !validControls(instrument.controls) || instrument.samplePath.length() > 32767
                    || (instrument.samplePath.isNotEmpty() && !juce::File::isAbsolutePath(instrument.samplePath))) return false;
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
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}
KitLibrary::KitLibrary() { state.kits.push_back(makeKit("Kit 1")); state.activeKit = state.kits.front().id; }
State KitLibrary::snapshot() const { const juce::ScopedLock guard(lock); return state; }
Result KitLibrary::edit(const std::function<Result(State&)>& operation)
{
    const juce::ScopedLock guard(lock);
    auto candidate = state;
    auto result = operation(candidate);
    if (!result.ok) return result;
    if (!validate(candidate)) return Result::fail("This change would exceed a limit or create an invalid assignment.");
    candidate.revision = state.revision + 1;
    candidate.kitEpoch = state.kitEpoch + (candidate.activeKit != state.activeKit ? 1 : 0);
    state = std::move(candidate); activeEpoch.store(state.kitEpoch);
    return result;
}
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
    return edit([&](State& s) {
        auto* source = find(s.kits, id);
        if (!source) return Result::fail("Kit no longer exists.");
        if (s.kits.size() == maxKits) return Result::fail("All 8 kit slots are in use.");
        auto copy = *source; copy.id = newId(); copy.name = copy.name.substring(0, 59) + " copy";
        for (auto& group : copy.groups) { group.id = newId(); for (auto& i : group.instruments) i.id = newId(); }
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
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = find(k->groups, groupId); if (!g) return Result::fail("Group no longer exists.");
        if (k->groups.size() == maxGroups || instrumentCount(*k) + (int)g->instruments.size() > maxInstruments) return Result::fail("Not enough group or instrument slots to copy this group.");
        auto copy = *g; copy.id = newId(); copy.name = copy.name.substring(0,59) + " copy"; copy.instruments.clear();
        auto sourceInstruments = g->instruments;
        k->groups.push_back(copy);
        for (auto i : sourceInstruments) { i.id = newId(); i.note = freeNote(*k); k->groups.back().instruments.push_back(i); }
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
        g->instruments.push_back(i); return Result::success(i.id);
    });
}
Result KitLibrary::duplicateInstrument(const juce::String& kitId, const juce::String& instrumentId)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        auto note = freeNote(*k); if (note < 0) return Result::fail("This kit already has 88 instruments.");
        auto copy = *find(g->instruments, instrumentId); copy.id = newId(); copy.name = copy.name.substring(0,59) + " copy"; copy.note = note;
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
        if (note < firstNote || note >= firstNote + maxInstruments) return Result::fail("Choose MIDI note 21 to 108.");
        for (auto& group : k->groups) for (auto& i : group.instruments) if (i.note == note && i.id != instrumentId) return Result::fail("That key is already assigned to " + i.name + ".");
        find(g->instruments, instrumentId)->note = note; return Result::success(instrumentId);
    });
}
Result KitLibrary::assignSample(const juce::String& kitId, const juce::String& instrumentId, const juce::String& path)
{
    return edit([&](State& s) {
        auto* k = find(s.kits, kitId); if (!k) return Result::fail("Kit no longer exists.");
        auto* g = containing(*k, instrumentId); if (!g) return Result::fail("Instrument no longer exists.");
        find(g->instruments, instrumentId)->samplePath = path;
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
        g->instruments.push_back(i); return Result::success(i.id);
    });
}
Result KitLibrary::sliceInstrument(const juce::String& kitId,const juce::String& instrumentId,int regions)
{
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
            auto slice=original; slice.id=newId(); slice.name=original.name.substring(0,52)+" "+juce::String(n+1);
            slice.note=freeNote(*kit); slice.controls.sound.start=original.controls.sound.start+n*width;
            slice.controls.sound.end=original.controls.sound.start+(n+1)*width;
            slice.controls.sound.loop=0; slice.controls.sound.mode=0;
            target.instruments.push_back(std::move(slice));
        }
        return Result::success(target.id);
    });
}
std::unique_ptr<juce::XmlElement> KitLibrary::toXml() const
{
    const auto s = snapshot(); auto xml = std::make_unique<juce::XmlElement>("DrumState");
    xml->setAttribute("schemaVersion", 7); xml->setAttribute("activeKit", s.activeKit);
    for (auto& k : s.kits)
    {
        auto* kit = xml->createNewChildElement("Kit"); kit->setAttribute("id", k.id); kit->setAttribute("name", k.name);
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
                for(int n=0;n<3;++n)if(const auto& shape=i.controls.sound.envelopes[(size_t)n];shape.count)
                {
                    auto* envelope=instrument->createNewChildElement("Envelope");envelope->setAttribute("index",n);envelope->setAttribute("hold",shape.hold);
                    for(int point=0;point<shape.count;++point)
                    {
                        const auto& p=shape.points[(size_t)point];auto* node=envelope->createNewChildElement("Point");
                        node->setAttribute("ms",p.ms);node->setAttribute("level",p.level);node->setAttribute("bend",p.bend);node->setAttribute("pivot",p.pivot);
                    }
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
        fresh.revision = state.revision + 1; fresh.kitEpoch = state.kitEpoch + 1; state = std::move(fresh); activeEpoch.store(state.kitEpoch); return true;
    }
    if (version < 2 || version > 7) return false;
    State candidate; candidate.activeKit = xml.getStringAttribute("activeKit");
    for (auto* k : xml.getChildIterator())
    {
        if (!k->hasTagName("Kit") || candidate.kits.size() == maxKits) return false;
        Kit kit { k->getStringAttribute("id"), k->getStringAttribute("name"), {} };
        int count = 0;
        for (auto* g : k->getChildIterator())
        {
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
                    if(version>=5) for(auto& p:soundParameters)
                        if((version>=6 || !introducedInVersion6(p)) && (version>=7 || p.member!=&SoundParameters::keyOverride))
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
                            const int index=integerAttribute(*e,"index");
                            if(!e->hasTagName("Envelope") || index<0 || index>2 || seen[(size_t)index] || e->getNumChildElements()<2 || e->getNumChildElements()>maxEnvelopePoints)return false;
                            seen[(size_t)index]=true;auto& shape=group.instruments.back().controls.sound.envelopes[(size_t)index];shape.hold=integerAttribute(*e,"hold");
                            for(auto* p:e->getChildIterator())
                            {
                                if(!p->hasTagName("Point") || p->getNumChildElements()!=0)return false;
                                shape.points[(size_t)shape.count++]={numberAttribute(*p,"ms"),numberAttribute(*p,"level"),numberAttribute(*p,"bend"),numberAttribute(*p,"pivot")};
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
    const juce::ScopedLock guard(lock); candidate.revision = state.revision + 1; candidate.kitEpoch = state.kitEpoch + 1; state = std::move(candidate); activeEpoch.store(state.kitEpoch); return true;
}
}
