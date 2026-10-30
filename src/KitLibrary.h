#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>
#include <vector>
#include "SoundParameters.h"

namespace drum
{
constexpr int maxKits = 8, maxGroups = 16, maxInstruments = 88, firstNote = 21;
struct InstrumentControls
{
    double volumeDb = 0.0, pan = 0.0, tuning = 0.0;
    bool mute = false, solo = false;
    int choke = 0;
    SoundParameters sound;
    std::array<MacroMapping,numMacros> macros {};
    bool operator==(const InstrumentControls&) const = default;
};
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
struct Instrument { juce::String id, name; int note = firstNote; juce::String samplePath; InstrumentControls controls; std::vector<EffectSlot> effects; };
struct Group { juce::String id, name; int colour = 0; std::vector<Instrument> instruments; };
struct Kit { juce::String id, name; std::vector<Group> groups; };
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
const Instrument* findInstrument(const Kit&, const juce::String&);
juce::String noteName(int);

// Only management/state threads take this lock. The audio callback never uses this model.
class KitLibrary
{
public:
    explicit KitLibrary(bool trackLayout=false);
    State snapshot() const;
    uint64_t activeKitEpoch() const { return activeEpoch.load(); }
    Result addKit(bool example = false);
    Result duplicateKit(const juce::String&);
    Result removeKit(const juce::String&);
    Result activateKit(const juce::String&);
    Result rename(const juce::String&, const juce::String&);
    Result addGroup(const juce::String& kitId);
    Result duplicateGroup(const juce::String& kitId, const juce::String& groupId);
    Result removeGroup(const juce::String& kitId, const juce::String& groupId);
    Result reorderGroup(const juce::String& kitId, const juce::String& groupId, int direction);
    Result addInstrument(const juce::String& kitId, const juce::String& groupId);
    Result duplicateInstrument(const juce::String& kitId, const juce::String& instrumentId);
    Result removeInstrument(const juce::String& kitId, const juce::String& instrumentId);
    Result moveInstrument(const juce::String& kitId, const juce::String& instrumentId, const juce::String& groupId);
    Result reorderInstrument(const juce::String& kitId, const juce::String& instrumentId, int direction);
    // Compatibility check: keys are derived from group/instrument order.
    Result assignNote(const juce::String& kitId, const juce::String& instrumentId, int note);
    Result assignSample(const juce::String& kitId, const juce::String& instrumentId, const juce::String& path);
    Result setControls(const juce::String& kitId, const juce::String& instrumentId, InstrumentControls);
    Result setEffects(const juce::String& kitId, const juce::String& instrumentId, const std::vector<EffectSlot>&);
    // Capture live effect settings before copying a kit/group/instrument.
    std::function<void()> beforeClone;
    std::function<void()> afterRestore;
    Result importInstrument(const juce::String& kitId,const juce::String& groupId,const Instrument& source);
    Result addSampleInstrument(const juce::String& kitId, const juce::String& groupId, const juce::String& path);
    Result sliceInstrument(const juce::String& kitId,const juce::String& instrumentId,int regions);
    std::unique_ptr<juce::XmlElement> toXml() const;
    bool restore(const juce::XmlElement&);
private:
    Result edit(const std::function<Result(State&)>&);
    const bool trackLayout;
    mutable juce::CriticalSection lock;
    State state;
    std::atomic<uint64_t> activeEpoch { 0 };
};
}
