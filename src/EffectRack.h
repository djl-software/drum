#pragma once
#include "KitLibrary.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

namespace drum
{
// One graph keeps every instrument independent and compensates different chain
// latencies before the instrument outputs are mixed. Graph edits run off audio.
class EffectRack final : private juce::Timer, private juce::AudioProcessorListener
{
public:
    static constexpr int channels = maxInstruments * 2;
    EffectRack(KitLibrary&, std::function<void()> changed, std::function<void(int)> latency);
    ~EffectRack() override;
    void prepare(double rate, int blockSize);
    void release();
    void sync(); // Message thread; also used by deterministic host tests.
    void capture(); // Non-audio thread, including host state-save callbacks.
    void stateRestored();
    bool active() const noexcept { return hasEffects.load(); }
    double tailSeconds() const noexcept { return tail.load(); }
    void process(juce::AudioBuffer<float>&, uint64_t kitEpoch, juce::AudioPlayHead*, bool nonRealtime);
    void reset() noexcept { resetGeneration.fetch_add(1); }
    juce::OwnedArray<juce::PluginDescription> discover(const juce::File&);
    Result add(const juce::String& kit, const juce::String& instrument, const juce::PluginDescription&, int position = -1);
    Result remove(const juce::String& kit, const juce::String& instrument, const juce::String& slot);
    Result move(const juce::String& kit, const juce::String& instrument, const juce::String& slot, int position);
    Result bypass(const juce::String& kit, const juce::String& instrument, const juce::String& slot, bool);
    Result replaceChain(const juce::String&,const juce::String&,std::vector<EffectSlot>);
    std::vector<EffectSlot> copyChain(const juce::String&,const juce::String&);
    void retry(const juce::String& slot);
    void openEditor(const juce::String& slot);
    bool supportsSidechain(const juce::String& slot) const;
    Result sidechain(const juce::String& kit,const juce::String& instrument,const juce::String& slot,int note,double gain,double mix,bool mute);
    juce::String status(const juce::String& slot) const;
    juce::AudioProcessor* instance(const juce::String& slot) const; // Message thread only.
private:
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
    mutable juce::CriticalSection managementLock;
    std::map<juce::String, std::unique_ptr<Runtime>> nodes;
    std::vector<std::pair<int, std::vector<EffectSlot>>> layout;
    std::atomic<bool> hasEffects {false}, dirty {false}, latencyDirty {false}, restored {false}, offline {false};
    std::atomic<uint64_t> graphEpoch {UINT64_MAX}, resetGeneration {0};
    std::atomic<double> rate {44100}, tail {0};
    std::atomic<int> blockSize {512};
    std::array<std::atomic<bool>,maxInstruments> mutedSidechainSources {};
    uint64_t seenRevision = UINT64_MAX;
    int captureTicks = 0;
    juce::MidiBuffer noMidi;
};
}
