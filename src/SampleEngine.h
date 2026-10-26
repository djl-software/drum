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
    SampleStatus statusFor(const juce::String& instrument) const;
    uint64_t statusRevision() const { return statusEpoch.load(); }
    // For deterministic offline tests. Never call from processBlock.
    bool waitForLoading(int timeoutMs);
    void prepare(double sampleRate);
    void release();
    void process(juce::AudioBuffer<float>&, const juce::MidiBuffer&) noexcept;
    void audition(int note) { previewNote.store(note); }
    void stop() { previewNote.store(-1); panic.store(true); }
    double tailSeconds() const { return tail.load(); }
private:
    struct Bank
    {
        juce::String kitId;
        uint64_t kitEpoch = 0;
        std::array<juce::String,128> ids;
        std::array<std::shared_ptr<const Sample>,128> samples;
    };
    struct Voice
    {
        const Sample* sample = nullptr;
        double position = 0.0;
        float velocity = 0.0f;
        int channel = 1;
        uint64_t age = 0;
    };
    struct Request { juce::String kit, instrument; juce::File file; };
    void run() override;
    std::shared_ptr<const Sample> decode(const juce::File&, juce::String& error);
    void setStatus(const juce::String&, SampleStatus);
    void updateBank(const State&);
    void reclaim();
    void startVoice(int note, int channel, float velocity) noexcept;
    void stopVoices(int channel = 0) noexcept;
    void render(juce::AudioBuffer<float>&, int start, int end) noexcept;
    KitLibrary& library;
    std::function<void()> onAssignment;
    juce::AudioFormatManager formats;
    mutable juce::CriticalSection queueLock, statusLock;
    std::deque<Request> requests;
    std::map<juce::String,SampleStatus> statuses;
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
    std::array<Voice,128> voices {};
    uint64_t clock = 0;
    double hostRate = 44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEngine)
};
}
