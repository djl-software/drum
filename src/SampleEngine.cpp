#include "SampleEngine.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace drum
{
static_assert(std::atomic<Sample*>::is_always_lock_free);
SampleEngine::SampleEngine(KitLibrary& model, std::function<void()> callback)
    : Thread("Drum sample loader"), library(model), onAssignment(std::move(callback))
{
    formats.registerBasicFormats();
    startThread();
}
SampleEngine::~SampleEngine()
{
    shutdown();
    // Processor lifetime guarantees no more audio callbacks here.
}
void SampleEngine::shutdown() { signalThreadShouldExit(); notify(); stopThread(-1); }
void SampleEngine::requestLoad(juce::String kit, juce::String instrument, juce::File file)
{
    if(outstanding.fetch_add(1)>=8)
    {
        outstanding.fetch_sub(1); setStatus(instrument,{false,false,"Sample queue is full; wait for current loads.",0}); return;
    }
    setStatus(instrument, { true, false, "Loading sample...", 0 });
    { const juce::ScopedLock guard(queueLock); requests.push_back({ std::move(kit), std::move(instrument), std::move(file) }); }
    notify();
}
SampleStatus SampleEngine::statusFor(const juce::String& id) const
{
    const juce::ScopedLock guard(statusLock);
    auto found=statuses.find(id); return found==statuses.end() ? SampleStatus{} : found->second;
}
void SampleEngine::setStatus(const juce::String& id, SampleStatus value)
{
    const juce::ScopedLock guard(statusLock); statuses[id]=std::move(value); statusEpoch.fetch_add(1);
}
bool SampleEngine::waitForLoading(int timeoutMs)
{
    const auto end=juce::Time::getMillisecondCounterHiRes()+timeoutMs;
    notify();
    do
    {
        if(outstanding.load()==0 && completedRevision.load()==library.snapshot().revision) return true;
        juce::Thread::sleep(2);
    } while(juce::Time::getMillisecondCounterHiRes()<end);
    return false;
}
std::shared_ptr<const Sample> SampleEngine::decode(const juce::File& file, juce::String& error)
{
    if(!file.existsAsFile()) { error="Missing sample. Use Load sample to locate it."; return {}; }
    auto key=file.getFullPathName()+"|"+juce::String(file.getLastModificationTime().toMilliseconds())+"|"+juce::String(file.getSize());
    if(auto cached=cache[key].lock()) return cached;
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader) { error="Unsupported or damaged audio file."; return {}; }
    if(reader->numChannels<1 || reader->numChannels>2 || !std::isfinite(reader->sampleRate)
        || reader->sampleRate<8000 || reader->sampleRate>384000 || reader->lengthInSamples<1
        || reader->lengthInSamples>reader->sampleRate*30.0
        || (uint64_t)reader->lengthInSamples*reader->numChannels*sizeof(float)>64u*1024u*1024u)
    { error="Use mono/stereo audio up to 30 seconds and 64 MB decoded."; return {}; }
    auto sample=std::make_shared<Sample>(); sample->rate=reader->sampleRate;
    sample->audio.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if(!reader->read(&sample->audio,0,sample->audio.getNumSamples(),0,true,true))
    { error="The audio file could not be read completely."; return {}; }
    for(int c=0;c<sample->audio.getNumChannels();++c)
        for(int n=0;n<sample->audio.getNumSamples();++n)
            if(!std::isfinite(sample->audio.getSample(c,n)) || std::abs(sample->audio.getSample(c,n))>64.0f) { error="Audio contains invalid sample values."; return {}; }
    cache[key]=sample; return sample;
}
void SampleEngine::run()
{
    uint64_t previous=UINT64_MAX;
    while(!threadShouldExit())
    {
        std::deque<Request> pending;
        { const juce::ScopedLock guard(queueLock); pending.swap(requests); }
        // Keep successful decodes alive until the new bank has taken ownership.
        std::vector<std::shared_ptr<const Sample>> loaded;
        for(auto& request:pending)
        {
            juce::String error; auto sample=decode(request.file,error);
            if(sample)
            {
                auto result=library.assignSample(request.kit,request.instrument,request.file.getFullPathName());
                if(result.ok) { if(onAssignment) onAssignment(); loaded.push_back(sample); setStatus(request.instrument,{false,true,"Loaded",sample->audio.getNumSamples()/sample->rate}); }
                else setStatus(request.instrument,{false,false,result.message,0});
            }
            else setStatus(request.instrument,{false,false,error,0});
        }
        auto state=library.snapshot();
        if(state.revision!=previous)
        {
            updateBank(state); previous=state.revision; completedRevision.store(previous);
        }
        outstanding.fetch_sub((int)pending.size());
        reclaim();
        if(!pending.empty()) statusEpoch.fetch_add(1);
        wait(20);
    }
}
void SampleEngine::updateBank(const State& state)
{
    auto next=std::make_unique<Bank>(); const auto* kit=activeKit(state); next->kitId=kit->id; next->kitEpoch=state.kitEpoch;
    size_t bytes=0; std::set<const Sample*> counted; double duration=0.0;
    for(auto& group:kit->groups) for(auto& instrument:group.instruments)
    {
        next->ids[(size_t)instrument.note]=instrument.id;
        if(instrument.samplePath.isEmpty()) { setStatus(instrument.id,{}); continue; }
        juce::String error; auto sample=decode(juce::File(instrument.samplePath),error);
        if(sample && !counted.contains(sample.get()))
        {
            if(bytes + sample->bytes()>512u*1024u*1024u) { sample.reset(); error="Active kit exceeds 512 MB of decoded samples."; }
            else { bytes+=sample->bytes(); counted.insert(sample.get()); }
        }
        next->samples[(size_t)instrument.note]=sample;
        if(sample)
        {
            auto seconds=sample->audio.getNumSamples()/sample->rate; duration=juce::jmax(duration,seconds);
            setStatus(instrument.id,{false,true,"Ready",seconds});
        }
        else setStatus(instrument.id,{false,false,error,0});
    }
    const auto* old=published.load();
    bool same=old && old->kitId==next->kitId && old->kitEpoch==next->kitEpoch && old->ids==next->ids;
    if(same) for(int n=0;n<128;++n) if(old->samples[(size_t)n]!=next->samples[(size_t)n]) { same=false; break; }
    if(same) return;
    tail.store(duration);
    auto* pointer=next.get(); banks.push_back(std::move(next)); published.store(pointer);
}
void SampleEngine::reclaim()
{
    auto* active=published.load(); auto* reading=hazard.load();
    std::erase_if(banks,[&](const auto& bank){ return bank.get()!=active && bank.get()!=reading; });
    std::erase_if(cache,[](const auto& entry){return entry.second.expired();});
}
void SampleEngine::prepare(double rate)
{
    hostRate=std::isfinite(rate) && rate>0 ? rate : 44100.0; release();
}
void SampleEngine::release()
{
    stopVoices(); current=nullptr; hazard.store(nullptr); previewNote.store(-1);
}
void SampleEngine::stopVoices(int channel) noexcept
{
    for(auto& voice:voices) if(channel==0 || voice.channel==channel) voice.sample=nullptr;
}
void SampleEngine::startVoice(int note,int channel,float velocity) noexcept
{
    if(!current || note<0 || note>127 || !current->samples[(size_t)note]) return;
    auto* voice=&voices.front();
    for(auto& candidate:voices)
    {
        if(!candidate.sample) { voice=&candidate; break; }
        if(candidate.age<voice->age) voice=&candidate;
    }
    *voice={ current->samples[(size_t)note].get(),0.0,velocity,channel,++clock };
}
void SampleEngine::render(juce::AudioBuffer<float>& output,int start,int end) noexcept
{
    for(auto& voice:voices)
    {
        if(!voice.sample) continue;
        const auto& sample=*voice.sample;
        const int length=sample.audio.getNumSamples(); const auto step=sample.rate/hostRate;
        for(int frame=start;frame<end;++frame)
        {
            const int index=(int)voice.position;
            if(index>=length) { voice.sample=nullptr; break; }
            const int next=juce::jmin(index+1,length-1); const float fraction=(float)(voice.position-index);
            // Short end ramp prevents a hard discontinuity on trimmed files.
            const float fade=(float)juce::jmin(1.0,(length-voice.position)/juce::jmax(1.0,sample.rate*0.002));
            auto value=[&](int channel) {
                const auto* data=sample.audio.getReadPointer(channel);
                return (data[index]+fraction*(data[next]-data[index]))*voice.velocity*fade;
            };
            const float left=value(0),right=sample.audio.getNumChannels()==2?value(1):left;
            if(output.getNumChannels()==1) output.addSample(0,frame,(left+right)*0.5f);
            else if(output.getNumChannels()>1) { output.addSample(0,frame,left); output.addSample(1,frame,right); }
            voice.position+=step;
        }
    }
}
void SampleEngine::process(juce::AudioBuffer<float>& output,const juce::MidiBuffer& midi) noexcept
{
    Bank* next=nullptr;
    do { next=published.load(); hazard.store(next); } while(next!=published.load());
    if(next!=current) { stopVoices(); current=next; }
    output.clear();
    if(!current || current->kitEpoch!=library.activeKitEpoch()) { stopVoices(); return; }
    if(panic.exchange(false)) stopVoices();
    if(auto note=previewNote.exchange(-1);note>=0) startVoice(note,1,0.8f);
    int position=0;
    for(const auto metadata:midi)
    {
        const int offset=juce::jlimit(position,output.getNumSamples(),metadata.samplePosition);
        render(output,position,offset); position=offset;
        // Inspect raw MIDI without allocating a MidiMessage for variable-length SysEx.
        if(metadata.numBytes<1) continue;
        const auto* data=metadata.data; const int command=data[0]&0xf0,channel=(data[0]&0x0f)+1;
        if(command==0x90 && metadata.numBytes>=3 && data[2]>0) startVoice(data[1],channel,data[2]/127.0f);
        else if(command==0xb0 && metadata.numBytes>=3 && (data[1]==120 || data[1]==123)) stopVoices(channel);
        // One-shot mode deliberately ignores note-off and velocity-zero note-on.
    }
    render(output,position,output.getNumSamples());
}
}
