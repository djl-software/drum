#pragma once
#include "SampleEngine.h"
#include <optional>

namespace drum
{
struct PreviewInput
{
    std::shared_ptr<const Sample> source;
    InstrumentControls controls;
    std::array<float,numMacros> macros {};
    double bpm=120,rate=44100;
    int send=-1;
    bool operator==(const PreviewInput&) const = default;
};
// One cancellable worker per open editor. No decoding, DSP or waiting in paint
// or processBlock; only a result for the latest requested settings is published.
class SoundPreview final : private juce::Thread
{
public:
    SoundPreview():Thread("Drum waveform preview") {startThread();}
    ~SoundPreview() override {signalThreadShouldExit();generation.fetch_add(1);notify();stopThread(-1);}
    void request(PreviewInput input)
    {
        const juce::ScopedLock guard(lock);
        if(requested && *requested==input) return;
        requested=std::move(input);generation.fetch_add(1);notify();
    }
    PreviewResult latest() const
    {
        const juce::ScopedLock guard(lock);
        return completed==generation.load()?result:PreviewResult{};
    }
    bool pending() const
    {
        const juce::ScopedLock guard(lock);
        return requested && requested->source && completed!=generation.load();
    }
private:
    void run() override
    {
        uint64_t previous=0;
        while(!threadShouldExit())
        {
            if(generation.load()==previous) {wait(100);continue;}
            // Coalesce slider drags before starting an expensive render.
            wait(40);
            PreviewInput input;uint64_t revision;
            {
                const juce::ScopedLock guard(lock);
                if(!requested) continue;
                input=*requested;revision=generation.load();
            }
            auto cancelled=[&]{return threadShouldExit() || generation.load()!=revision;};
            PreviewResult rendered;
            try {rendered=SampleEngine::renderPreview(input.source,input.controls,input.macros,input.bpm,input.rate,input.send,cancelled);}
            catch(...) {rendered.error="Preview unavailable";}
            {
                const juce::ScopedLock guard(lock);
                if(!cancelled()) {result=std::move(rendered);completed=revision;}
            }
            previous=revision;
        }
    }
    mutable juce::CriticalSection lock;
    std::optional<PreviewInput> requested;
    PreviewResult result;
    std::atomic<uint64_t> generation {0};
    uint64_t completed=0;
};
}
