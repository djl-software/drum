#pragma once
#include "MeterPeakState.h"
#include "KitLibrary.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#if JUCE_MAC
#include "../../Mac/src/GroupRouting.h"
#endif

namespace drum
{
// One graph keeps every instrument independent and compensates different chain
// latencies before the instrument outputs are mixed. Graph edits run off audio.
class EffectRack final : private juce::Timer, private juce::AudioProcessorListener
{
public:
#if JUCE_MAC
    static constexpr int processingLanes=macProcessingLanes;
#else
    static constexpr int processingLanes=maxInstruments+6;
#endif
    static constexpr int channels = processingLanes * 2;
    EffectRack(KitLibrary&, std::function<void()> changed, std::function<void(int)> latency);
    ~EffectRack() override;
    void prepare(double rate, int blockSize);
    void release();
    void routingChanged(){sync();changed();}
    void sync(); // Message thread; also used by deterministic host tests.
    void capture(); // Non-audio thread, including host state-save callbacks.
    void stateRestored();
    bool active() const noexcept { return hasEffects.load(); }
    double tailSeconds() const noexcept { return tail.load(); }
    void process(juce::AudioBuffer<float>&, uint64_t kitEpoch, juce::AudioPlayHead*, bool nonRealtime);
    void setSendLevels(int note,const std::array<float,6>& levels) noexcept {for(int n=0;n<6;++n)sendLevels[(size_t)(note-firstNote)][(size_t)n].store(levels[(size_t)n],std::memory_order_relaxed);}
    void setMixerLevels(int index,float left,float right) noexcept {mixerLevels[(size_t)index][0].store(left);mixerLevels[(size_t)index][1].store(right);}
    std::array<float,2> mixerPeaks(int index) const noexcept {return {mixerMeters[(size_t)index][0].load(),mixerMeters[(size_t)index][1].load()};}
    std::array<float,2> mixerMaximum(int index) const noexcept {return {mixerMaxima[(size_t)index][0].load(),mixerMaxima[(size_t)index][1].load()};}
    std::array<float,2> mixerHold(int index) const noexcept {return {mixerHolds[(size_t)index][0].load(),mixerHolds[(size_t)index][1].load()};}
    void resetMixerMaximum(int index) noexcept {for(auto& p:mixerMaxima[(size_t)index])p.store(0);resetHolds[(size_t)index].store(true);}
    void setPeakHoldSeconds(double seconds) noexcept {peakHoldSeconds.store(seconds);}
    void reset() noexcept { resetGeneration.fetch_add(1); }
    juce::OwnedArray<juce::PluginDescription> discover(const juce::File&);
    Result returnSend(const juce::String& kit,int source,int destination,double db);
    Result returnMain(const juce::String& kit,int source,bool enabled);
    Result add(const juce::String& kit, const juce::String& instrument, const juce::PluginDescription&, int position = -1);
    Result remove(const juce::String& kit, const juce::String& instrument, const juce::String& slot);
    Result move(const juce::String& kit, const juce::String& instrument, const juce::String& slot, int position);
    Result bypass(const juce::String& kit, const juce::String& instrument, const juce::String& slot, bool);
    Result replaceChain(const juce::String&,const juce::String&,std::vector<EffectSlot>);
    std::vector<EffectSlot> copyChain(const juce::String&,const juce::String&);
    void retry(const juce::String& slot);
    void openEditor(const juce::String& slot,juce::Component* owner=nullptr);
    void hideEditors(); // Message thread only.
    bool supportsSidechain(const juce::String& slot) const;
    Result sidechain(const juce::String& kit,const juce::String& instrument,const juce::String& slot,int note,double gain,double mix,bool mute);
    juce::String status(const juce::String& slot) const;
    std::array<float,2> peaks(const juce::String& slot) const;
    juce::AudioProcessor* instance(const juce::String& slot) const; // Message thread only.
private:
    class SendSplitter;
    class HostedProcessor;
    class PluginWindow;
    struct Runtime;
    using Graph = juce::AudioProcessorGraph;
    void timerCallback() override;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override;
    Result edit(const juce::String&, const juce::String&, const std::function<Result(std::vector<EffectSlot>&)>&);
    KitLibrary& library;
    std::function<void()> changed;
    std::function<void(int)> latency;
    juce::VST3PluginFormat format;
    Graph graph;
    Graph::Node::Ptr input, output;
    std::array<Graph::Node::Ptr,processingLanes> splitters;
#if JUCE_MAC
    std::vector<Graph::Node::Ptr> groupFaders;
    juce::StringArray activeSharedReturns;
#endif
    std::array<std::array<std::atomic<float>,6>,processingLanes> sendLevels{};
    std::array<std::array<std::atomic<float>,2>,processingLanes> mixerLevels{},mixerMeters{},mixerMaxima{},mixerHolds{};
    std::array<std::atomic<bool>,processingLanes> resetHolds{};
    std::atomic<double> peakHoldSeconds{1};
    std::array<std::atomic<bool>,processingLanes> returnMainEnabled{};
    std::array<ReturnRouting,6> returnRouting{};
    mutable juce::CriticalSection managementLock;
    std::map<juce::String, std::unique_ptr<Runtime>> nodes;
    std::vector<std::pair<int, std::vector<EffectSlot>>> layout;
    std::atomic<bool> hasEffects {false}, dirty {false}, latencyDirty {false}, restored {false}, offline {false};
    std::atomic<uint64_t> graphEpoch {UINT64_MAX}, resetGeneration {0};
    std::atomic<double> rate {44100}, tail {0};
    std::atomic<int> blockSize {512};
    std::array<std::atomic<bool>,maxInstruments> mutedSidechainSources {};
    juce::String routingTopology;
    uint64_t seenRevision = UINT64_MAX;
    int captureTicks = 0;
    juce::MidiBuffer noMidi;
};
}
