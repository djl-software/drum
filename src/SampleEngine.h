#pragma once
#include "KitLibrary.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <deque>
#include <map>

namespace drum
{
struct Sample
{
    juce::AudioBuffer<float> audio;
    double rate = 44100.0;
    float peak = 0.0f;
    std::array<float,512> waveform {};
    size_t bytes() const { return (size_t)audio.getNumChannels() * (size_t)audio.getNumSamples() * sizeof(float); }
};
struct SampleStatus
{
    bool loading = false, ready = false;
    juce::String message;
    double seconds = 0.0;
};
class SampleEngine final : private juce::Thread
{
public:
    explicit SampleEngine(KitLibrary&, std::function<void()> onAssignment = {});
    void shutdown();
    ~SampleEngine() override;
    void requestLoad(juce::String kit, juce::String instrument, juce::File file);
    void requestImport(juce::String kit, juce::String group, juce::File file);
    SampleStatus statusFor(const juce::String& instrument) const;
    std::array<float,512> waveformFor(const juce::String& instrument) const;
    uint64_t statusRevision() const { return statusEpoch.load(); }
    // For deterministic offline tests. Never call from processBlock.
    bool waitForLoading(int timeoutMs);
    void prepare(double sampleRate);
    void release();
    void process(juce::AudioBuffer<float>&, const juce::MidiBuffer&) noexcept;
    void setTempo(double bpm) noexcept { tempo = juce::jlimit(20.0,999.0,bpm); }
    void setMacros(const std::array<float,numMacros>& values) noexcept { macroValues=values; }
    void setOutputChannels(const std::array<int,22>& channels, int mainChannels) noexcept { busChannels=channels; mainWidth=mainChannels; }
    void audition(int note) { previewNote.store(note); }
    void stop() { previewNote.store(-1); panic.store(true); }
    double tailSeconds() const { return tail.load(); }
private:
    struct RenderControls
    {
        float left = 1, right = 1;
        double pitch = 1;
        int choke = 0;
        bool audible = true;
        SoundParameters sound;
        int start=0,end=1,loopStart=0,loopEnd=1;
        double sampleGain=1;
        std::array<float,6> sends {};
        InstrumentControls source;
        bool operator==(const RenderControls&) const = default;
    };
    struct Bank
    {
        juce::String kitId;
        uint64_t kitEpoch = 0;
        uint64_t sampleGeneration = 0;
        std::array<juce::String,128> ids;
        std::array<std::shared_ptr<const Sample>,128> samples;
        std::array<RenderControls,128> controls;
    };
    struct Voice
    {
        const Sample* sample = nullptr;
        double position = 0.0;
        float velocity = 0.0f;
        int channel = 1;
        uint64_t age = 0;
        int note = 0, choke = 0, releaseRemaining = -1;
        int start=0,end=1,loopStart=0,loopEnd=1;
        double elapsed=0,phase=0;
        float randomPan=0,randomLfo=0;
        bool held=true,preview=false;
        SoundParameters sound;
        juce::ADSR amp,filterEnvelope,pitchEnvelope;
        // Two cascaded TPT state-variable sections, with independent stereo state.
        std::array<std::array<double,2>,2> filter1 {},filter2 {};
        int filterCounter=0;
        double filterG=0,filterK=1;
        juce::SmoothedValue<float> left, right;
        juce::SmoothedValue<double> step;
    };
    struct Request { juce::String kit, instrument, group; juce::File file; };
    void enqueue(Request);
    void run() override;
    std::shared_ptr<const Sample> decode(const juce::File&, juce::String& error);
    void setStatus(const juce::String&, SampleStatus);
    void updateBank(const State&);
    void reclaim();
    void startVoice(int note, int channel, float velocity) noexcept;
    void releaseNote(int note, int channel) noexcept;
    void configureEnvelopes(Voice&) noexcept;
    void stopVoices(int channel = 0) noexcept;
    void fadeVoice(Voice&) noexcept;
    void updateVoices() noexcept;
    void updateRenderControls() noexcept;
    void render(juce::AudioBuffer<float>&, int start, int end) noexcept;
    KitLibrary& library;
    std::function<void()> onAssignment;
    juce::AudioFormatManager formats;
    mutable juce::CriticalSection queueLock, statusLock;
    std::deque<Request> requests;
    std::map<juce::String,SampleStatus> statuses;
    std::map<juce::String,std::array<float,512>> waveforms;
    std::map<juce::String,std::weak_ptr<const Sample>> cache;
    std::vector<std::unique_ptr<Bank>> banks;
    // One audio reader. Hazard stays published between callbacks to protect active voices.
    // Only the worker allocates/reclaims banks and sample buffers.
    std::atomic<Bank*> published { nullptr }, hazard { nullptr };
    std::atomic<uint64_t> statusEpoch { 0 }, completedRevision { UINT64_MAX };
    std::atomic<int> outstanding { 0 }, previewNote { -1 };
    std::atomic<bool> panic { false };
    std::atomic<double> tail { 0.0 };
    Bank* current = nullptr;
    // Fixed sizes after construction; storage is off the processor's stack.
    std::vector<Voice> voices = std::vector<Voice>(128);
    std::vector<RenderControls> renderControls = std::vector<RenderControls>(128);
    std::array<float,numMacros> macroValues {},previousMacros {};
    uint64_t clock = 0;
    uint64_t sampleGeneration = UINT64_MAX;
    int releaseSamples = 96;
    double hostRate = 44100.0;
    double tempo=120,freePhase=0;
    std::array<double,16> pitchBend {};
    std::array<int,22> busChannels {0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    int mainWidth=2;
    uint32_t randomState=0x12345678;
    float random() noexcept { randomState=1664525u*randomState+1013904223u; return (float)(randomState>>8)/8388607.5f-1.0f; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEngine)
};
}
