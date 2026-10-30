#include "EffectRack.h"
#include "SampleEngine.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace drum
{
static_assert(std::atomic<Sample*>::is_always_lock_free);
namespace
{
double attackWindow(double elapsed,double duration) noexcept
{
    const double t=elapsed/duration;
    return std::sin(juce::MathConstants<double>::halfPi*juce::jmin(1.0,elapsed/0.0002))
        *std::exp(-6*t)*(1-t)*(1-t);
}
double attackOscillator(int type,double hz,double rate,double coefficient,
    double& phase,double& filteredNoise,uint32_t& seed) noexcept
{
    const double angle=phase*juce::MathConstants<double>::twoPi;double value;
    if(type==2)
    {
        seed=1664525u*seed+1013904223u;
        filteredNoise+=coefficient*((double)(seed>>8)/8388607.5-1-filteredNoise);value=filteredNoise;
    }
    else value=type==0?(std::sin(angle)+0.4*std::sin(2*angle))/1.4:std::sin(angle);
    phase+=hz/rate;if(phase>=1)phase-=std::floor(phase);
    return value;
}
}
float SampleEngine::calibratedAttackPeak(const SoundParameters& s,double rate)
{
    if(s.attackOn==0)return 1;
    const auto key=std::tuple{s.attackType,s.attackTone,s.attackDecay,rate};
    if(auto found=attackPeaks.find(key);found!=attackPeaks.end())return found->second;
    const double hz=juce::jmin(s.attackTone,rate*(s.attackType==0?0.22:0.44));
    const double coefficient=1-std::exp(-juce::MathConstants<double>::twoPi*hz/rate),duration=s.attackDecay*0.001;
    double phase=0,noise=0;uint32_t seed=0x7f4a7c15;float peak=0;
    for(int frame=0;frame<(int)std::ceil(duration*rate);++frame)
    {
        const float value=(float)attackOscillator((int)s.attackType,hz,rate,coefficient,phase,noise,seed)
            *(float)attackWindow(frame/rate,duration);
        peak=juce::jmax(peak,std::abs(value));
    }
    peak=juce::jmax(peak,1.0e-12f);
    if(attackPeaks.size()>=1024)attackPeaks.clear();
    attackPeaks[key]=peak;return peak;
}
SampleEngine::SampleEngine(KitLibrary& model, std::function<void()> callback)
    : SampleEngine(model,std::move(callback),true) {}
SampleEngine::SampleEngine(KitLibrary& model, std::function<void()> callback, bool startLoader)
    : Thread("Drum sample loader"), library(model), onAssignment(std::move(callback))
{
    if(startLoader) { formats.registerBasicFormats(); startThread(); }
}
SampleEngine::~SampleEngine()
{
    shutdown();
    // Processor lifetime guarantees no more audio callbacks here.
}
void SampleEngine::shutdown() { signalThreadShouldExit(); notify(); stopThread(-1); }
void SampleEngine::requestLoad(juce::String kit, juce::String instrument, juce::File file)
{
    enqueue({std::move(kit),std::move(instrument),{},std::move(file)});
}
void SampleEngine::requestImport(juce::String kit, juce::String group, juce::File file)
{
    enqueue({std::move(kit),{},std::move(group),std::move(file)});
}
void SampleEngine::enqueue(Request request)
{
    const auto id=request.instrument.isEmpty()?request.group:request.instrument;
    if(outstanding.fetch_add(1)>=88)
    {
        outstanding.fetch_sub(1); setStatus(id,{false,false,"Sample queue is full; wait for current loads.",0}); return;
    }
    setStatus(id, { true, false, "Loading sample...", 0 });
    { const juce::ScopedLock guard(queueLock); requests.push_back(std::move(request)); }
    notify();
}
SampleStatus SampleEngine::statusFor(const juce::String& id) const
{
    const juce::ScopedLock guard(statusLock);
    auto found=statuses.find(id); return found==statuses.end() ? SampleStatus{} : found->second;
}
std::shared_ptr<const Sample> SampleEngine::waveformFor(const juce::String& id) const
{
    const juce::ScopedLock guard(statusLock);
    auto found=waveforms.find(id); return found==waveforms.end()?nullptr:found->second.lock();
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
        if(outstanding.load()==0 && completedRevision.load()==library.snapshot().revision && completedRate.load()==displayedRate.load()) return true;
        juce::Thread::sleep(2);
    } while(juce::Time::getMillisecondCounterHiRes()<end);
    return false;
}
std::shared_ptr<const Sample> SampleEngine::decode(const juce::File& file, juce::String& error)
{
    if(file.hasFileExtension("adv;adg;alc;amxd;vstpreset"))
    { error="This is an instrument preset. Drag its audio sample, or render the sound to WAV in Live."; return {}; }
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
    sample->waveform.build(sample->audio);
    for(int c=0;c<sample->audio.getNumChannels();++c)
    {
        const auto range=sample->waveform.range(sample->audio,c,0,sample->audio.getNumSamples());
        sample->peak=juce::jmax(sample->peak,std::abs(range.minimum),std::abs(range.maximum));
    }
    cache[key]=sample; return sample;
}
void SampleEngine::run()
{
    uint64_t previous=UINT64_MAX;double previousRate=0;
    while(!threadShouldExit())
    {
        std::deque<Request> pending;
        // Decode one request per pass so bulk drops cannot retain an unbounded batch of audio.
        { const juce::ScopedLock guard(queueLock); if(!requests.empty()) { pending.push_back(std::move(requests.front())); requests.pop_front(); } }
        // Keep successful decodes alive until the new bank has taken ownership.
        std::vector<std::shared_ptr<const Sample>> loaded;
        for(auto& request:pending)
        {
            const auto id=request.instrument.isEmpty()?request.group:request.instrument;
            juce::String error; auto sample=decode(request.file,error);
            if(sample)
            {
                auto result=request.instrument.isEmpty()
                    ? library.addSampleInstrument(request.kit,request.group,request.file.getFullPathName())
                    : library.assignSample(request.kit,request.instrument,request.file.getFullPathName());
                if(result.ok) { if(onAssignment) onAssignment(); loaded.push_back(sample); setStatus(id,{false,true,"Loaded "+request.file.getFileName(),sample->audio.getNumSamples()/sample->rate}); }
                else setStatus(id,{false,false,result.message,0});
            }
            else setStatus(id,{false,false,error,0});
        }
        auto state=library.snapshot();
        const double rate=displayedRate.load();
        if(state.revision!=previous || rate!=previousRate)
        {
            updateBank(state,rate);previous=state.revision;previousRate=rate;
            completedRevision.store(previous);completedRate.store(rate);
        }
        outstanding.fetch_sub((int)pending.size());
        reclaim();
        if(!pending.empty()) statusEpoch.fetch_add(1);
        wait(20);
    }
}
SampleEngine::RenderControls SampleEngine::controlsFor(const Sample& sample,const InstrumentControls& c,bool hasSolo)
{
    RenderControls render;
    render.source=c;render.sound=c.sound;
    const auto gain=c.volumeDb<=-60?0.0f:juce::Decibels::decibelsToGain((float)c.volumeDb);
    render.left=gain*(float)juce::jmin(1.0,1-c.pan);render.right=gain*(float)juce::jmin(1.0,1+c.pan);
    render.pitch=std::pow(2.0,(c.tuning+c.sound.detune/100)/12);render.choke=c.choke;
    render.audible=!c.mute && (!hasSolo || c.solo);
    const auto& s=c.sound;const int length=sample.audio.getNumSamples();
    auto snap=[&](int position) {
        if(s.snap==0) return position;
        const auto* data=sample.audio.getReadPointer(0);
        for(int d=0;d<=256;++d) for(int side:{-1,1})
        {
            const int n=position+d*side;
            if(n>0 && n<length && ((data[n]>=0)!=(data[n-1]>=0))) return n;
        }
        return position;
    };
    render.start=juce::jlimit(0,length-1,snap((int)(s.start*length)));
    render.end=juce::jlimit(render.start+1,length,snap((int)(s.end*length)));
    const int region=render.end-render.start;
    render.loopStart=juce::jlimit(render.start,render.end-1,snap(render.start+(int)(s.loopStart*region)));
    render.loopEnd=juce::jlimit(render.loopStart+1,render.end,snap(render.start+(int)(s.loopEnd*region)));
    render.sampleGain=std::pow(10.0,s.sampleGain/20)*(s.normalise!=0 && sample.peak>0?1.0/sample.peak:1.0);
    std::array<double,6> db{s.sendA,s.sendB,s.sendC,s.sendD,s.sendE,s.sendF};
    for(size_t n=0;n<6;++n)render.sends[n]=db[n]<=-60?0.0f:juce::Decibels::decibelsToGain((float)db[n]);
    return render;
}
PreviewResult SampleEngine::renderPreview(std::shared_ptr<const Sample> source,InstrumentControls controls,
    const std::array<float,numMacros>& macros,double bpm,double rate,int send,const std::function<bool()>& cancelled)
{
    if(!source || (cancelled && cancelled())) return {};
    juce::ScopedNoDenormals noDenormals;
    KitLibrary model;
    SampleEngine engine(model,{},false);
    engine.prepare(juce::jlimit(8000.0,384000.0,rate));engine.setTempo(bpm);engine.setMacros(macros);
    auto bank=std::make_unique<Bank>();bank->samples[21]=source;
    // Still render the duration of a muted sound, then show a flat waveform.
    const bool muted=controls.mute;controls.mute=false;
    bank->controls[21]=controlsFor(*source,controls,false);
    bank->controls[21].attackPeak=engine.calibratedAttackPeak(controls.sound,engine.hostRate);
    engine.current=bank.get();engine.updateRenderControls();
    engine.busChannels.fill(-1);
    if(send>=0 && send<6)engine.busChannels[(size_t)send+16]=0;
    else engine.busChannels[0]=0; // The instrument signal before its destination or external effects.
    engine.startVoice(21,1,0.8f);auto& voice=engine.voices.front();voice.preview=true;
    auto result=std::make_shared<Sample>();result->rate=engine.hostRate;
    // At most 30 seconds / 2.88 million stereo frames (~23 MiB) per result.
    const int limit=juce::jmin(2880000,(int)(30*engine.hostRate));
    juce::AudioBuffer<float> block(2,512);
    int rendered=0,capacity=juce::jmin(4096,limit);
    result->audio.setSize(2,capacity);result->audio.clear();
    while(voice.sample && rendered<limit)
    {
        if(cancelled && cancelled()) return {};
        const int count=juce::jmin(512,limit-rendered);
        if(rendered+count>capacity)
        {capacity=juce::jmin(limit,capacity*2);result->audio.setSize(2,capacity,true,true);}
        block.clear();engine.render(block,0,count);
        for(int channel=0;channel<2;++channel)result->audio.copyFrom(channel,rendered,block,channel,0,count);
        rendered+=count;
    }
    const bool truncated=voice.sample!=nullptr;
    const int used=juce::jlimit(1,limit,(int)std::llround(voice.elapsed*engine.hostRate));
    result->audio.setSize(2,used,true,true,true);
    if(muted)result->audio.clear();
    result->peak=result->audio.getMagnitude(0,used);
    if(cancelled && cancelled()) return {};
    result->waveform.build(result->audio);
    return {std::move(result),truncated,{}};
}
void SampleEngine::updateBank(const State& state,double rate)
{
    auto next=std::make_unique<Bank>(); const auto* kit=activeKit(state); next->kitId=kit->id; next->kitEpoch=state.kitEpoch;
    next->rate=rate;
    size_t bytes=0; std::set<const Sample*> counted; double duration=0.0;
    bool hasSolo=false;
    for(const auto& group:kit->groups)for(const auto& instrument:group.instruments)next->hasEffects|=!instrument.effects.empty();
    for(auto& group:kit->groups) for(auto& instrument:group.instruments) hasSolo|=instrument.controls.solo;
    for(auto& group:kit->groups) for(auto& instrument:group.instruments)
    {
        const auto& c=instrument.controls;
        auto& render=next->controls[(size_t)instrument.note];
        render.source=c;
        const auto gain=c.volumeDb<=-60 ? 0.0f : juce::Decibels::decibelsToGain((float)c.volumeDb);
        // Stereo balance: centre is unity; hard pan silences the opposite channel.
        render.left=gain*(float)juce::jmin(1.0,1.0-c.pan);
        render.right=gain*(float)juce::jmin(1.0,1.0+c.pan);
        render.sound=c.sound;
        render.pitch=std::pow(2.0,(c.tuning+c.sound.detune/100.0)/12.0); render.choke=c.choke;
        render.audible=!c.mute && (!hasSolo || c.solo);
        next->ids[(size_t)instrument.note]=instrument.id;
        if(instrument.samplePath.isEmpty()) { setStatus(instrument.id,{}); const juce::ScopedLock guard(statusLock); waveforms.erase(instrument.id); continue; }
        juce::String error; auto sample=decode(juce::File(instrument.samplePath),error);
        if(sample && !counted.contains(sample.get()))
        {
            if(bytes + sample->bytes()>512u*1024u*1024u) { sample.reset(); error="Active kit exceeds 512 MB of sample and waveform data."; }
            else { bytes+=sample->bytes(); counted.insert(sample.get()); }
        }
        next->samples[(size_t)instrument.note]=sample;
        if(sample)
        {
            render=controlsFor(*sample,c,hasSolo);
            render.attackPeak=calibratedAttackPeak(c.sound,rate);
            auto& s=c.sound;
            { const juce::ScopedLock guard(statusLock); waveforms[instrument.id]=sample; }
            auto seconds=sample->audio.getNumSamples()/sample->rate; duration=juce::jmax(duration,seconds/render.pitch);
            if(s.mode==2 && s.loop!=0)
            {
                const auto& e=s.envelopes[0];const double release=e.count?e.points[(size_t)e.count-1].ms-e.points[(size_t)e.hold].ms+2:s.ampRelease;
                duration=juce::jmax(duration,30.0+release/1000.0);
            }
            if(s.attackOn!=0 && s.attackLevel>-60)duration=juce::jmax(duration,(s.attackPosition+s.attackDecay)*0.001);
            setStatus(instrument.id,{false,true,"Ready",seconds});
        }
        else { setStatus(instrument.id,{false,false,error,0}); const juce::ScopedLock guard(statusLock); waveforms.erase(instrument.id); }
    }
    { const juce::ScopedLock guard(statusLock); std::erase_if(waveforms,[&](const auto& entry){return findInstrument(*kit,entry.first)==nullptr;}); }
    const auto* old=published.load();
    bool same=old && old->kitId==next->kitId && old->kitEpoch==next->kitEpoch && old->ids==next->ids;
    if(same) for(int n=0;n<128;++n) if(old->samples[(size_t)n]!=next->samples[(size_t)n]) { same=false; break; }
    next->sampleGeneration=old ? old->sampleGeneration+(same?0:1) : 0;
    if(same && old->controls==next->controls && old->rate==rate && old->hasEffects==next->hasEffects) return;
    tail.store(duration);
    auto* pointer=next.get(); banks.push_back(std::move(next)); published.store(pointer);
}
void SampleEngine::reclaim()
{
    auto* active=published.load(); auto* reading=hazard.load();
    std::erase_if(banks,[&](const auto& bank){ return bank.get()!=active && bank.get()!=reading; });
    std::erase_if(cache,[](const auto& entry){return entry.second.expired();});
}
void SampleEngine::prepare(double rate,int blockSize)
{
    hostRate=std::isfinite(rate) && rate>0 ? rate : 44100.0; releaseSamples=juce::jmax(1,(int)(hostRate*0.002)); release();
    effectBlockSize=juce::jmax(1,blockSize);
    if(effects)instrumentAudio.setSize(EffectRack::channels,effectBlockSize);
    displayedRate.store(hostRate);
    // prepareToPlay is the non-realtime setup phase. Let the worker calibrate
    // at the new device rate; slow source loads can still finish asynchronously.
    if(isThreadRunning())waitForLoading(1000);
}
void SampleEngine::release()
{
    // Hosts may release/reprepare after an effect latency change (including
    // bypass). Keep the user's loop request; explicit Pause/editor close stops it.
    repeatingNote=-1;auditionBeatRemaining=0;
    stopVoices(); current=nullptr; sampleGeneration=UINT64_MAX; hazard.store(nullptr); previewNote.store(-1);
    pitchBend.fill(0); freePhase=0;
}
void SampleEngine::keyboardNote(int note,bool down)
{
    if(note<0 || note>127)return;
    if(auto slot=keyboardQueue.write(1);slot.blockSize1>0)
        keyboardCommands[(size_t)slot.startIndex1]={note,down,library.activeKitEpoch()};
    else panic.store(true); // A saturated queue must never leave a stuck gate.
}
void SampleEngine::indicateNote(int note,int channel,bool down,bool fromUI) noexcept
{
    if(note<0 || note>127)return;
    const uint64_t bit=uint64_t(1)<<(fromUI?16:channel-1);
    auto& state=activity[(size_t)note];
    if(down){state.fetch_or(bit);state.fetch_add(uint64_t(1)<<32);}
    else state.fetch_and(~bit);
}
void SampleEngine::stopVoices(int channel) noexcept
{
    for(auto& voice:voices) if(channel==0 || (!voice.fromUI && voice.channel==channel)) voice.sample=nullptr;
    const uint64_t clear=channel==0?0x1ffffULL:uint64_t(1)<<(channel-1);
    for(auto& state:activity)state.fetch_and(~clear);
}
void SampleEngine::startVoice(int note,int channel,float velocity,bool fromUI) noexcept
{
    if(!current || note<0 || note>127 || !current->samples[(size_t)note]) return;
    const auto& c=renderControls[(size_t)note]; if(!c.audible) return;
    if(c.choke!=0) for(auto& voice:voices) if(voice.sample && voice.choke==c.choke) fadeVoice(voice);
    int playing=0;
    for(auto& v:voices) if(v.sample && v.note==note && v.releaseRemaining<0)
    {
        ++playing;
        if(c.sound.retrigger!=0) fadeVoice(v);
    }
    while(playing>=(int)c.sound.voices)
    {
        Voice* oldest=nullptr;
        for(auto& v:voices) if(v.sample && v.note==note && v.releaseRemaining<0 && (!oldest || v.age<oldest->age)) oldest=&v;
        if(!oldest) break;
        fadeVoice(*oldest); --playing;
    }
    auto* voice=&voices.front();
    for(auto& candidate:voices)
    {
        if(!candidate.sample) { voice=&candidate; break; }
        if(candidate.age<voice->age) voice=&candidate;
    }
    voice->sample=current->samples[(size_t)note].get(); voice->velocity=velocity;
    voice->fromUI=fromUI;voice->channel=channel; voice->age=++clock; voice->note=note; voice->choke=c.choke; voice->releaseRemaining=-1;
    voice->sound=c.sound; voice->start=c.start; voice->end=c.end; voice->loopStart=c.loopStart; voice->loopEnd=c.loopEnd;
    voice->position=c.sound.reverse!=0?c.end-1:c.start; voice->elapsed=0; voice->held=true; voice->preview=false;
    const double lfoRate=c.sound.lfoSync!=0?tempo/60.0/std::pow(2.0,c.sound.lfoDivision-4):c.sound.lfoRate;
    voice->phase=c.sound.lfoRetrigger!=0?std::fmod(c.sound.lfoPhase/360.0,1.0):std::fmod(freePhase*lfoRate,1.0);
    voice->randomPan=random(); voice->randomLfo=random(); voice->filter1={}; voice->filter2={}; voice->filterCounter=0;
    for(auto* env:{&voice->amp,&voice->filterEnvelope,&voice->pitchEnvelope}) { env->reset(); env->setSampleRate(hostRate); }
    configureEnvelopes(*voice);
    voice->equalizer.reset(c.sound.equalizer,hostRate);
    voice->attackEqualizer.reset(c.sound.equalizer,hostRate);
    voice->sampleFinished=false;voice->attackFinished=false;voice->elapsedFrames=0;
    voice->attackStartFrame=(int64_t)std::llround(c.sound.attackPosition*0.001*hostRate);
    voice->amp.noteOn(); voice->filterEnvelope.noteOn(); voice->pitchEnvelope.noteOn();
    voice->left.reset(hostRate,0.005); voice->right.reset(hostRate,0.005); voice->step.reset(hostRate,0.005);
    voice->left.setCurrentAndTargetValue(c.left); voice->right.setCurrentAndTargetValue(c.right);
    voice->monoMix.reset(hostRate,0.005);voice->monoMix.setCurrentAndTargetValue((float)c.sound.mono);
    voice->attackGain.reset(hostRate,0.005);voice->attackGain.setCurrentAndTargetValue(c.sound.attackOn!=0 && c.sound.attackLevel>-60?juce::Decibels::decibelsToGain((float)c.sound.attackLevel):0.0f);
    voice->attackPhase=0;voice->attackNoise=0;voice->attackRandom=0x7f4a7c15;
    voice->step.setCurrentAndTargetValue(voice->sample->rate/hostRate*c.pitch);
}
void SampleEngine::configureEnvelopes(Voice& v) noexcept
{
    const auto& s=v.sound;
    auto parameters=[](double a,double d,double sustain,double r) { return juce::ADSR::Parameters{(float)(a/1000),(float)(d/1000),(float)sustain,(float)(r/1000)}; };
    v.amp.setParameters(s.mode==2 ? parameters(s.ampAttack,s.ampDecay,s.ampSustain,s.ampRelease) : parameters(s.fadeIn,0,1,s.fadeOut));
    v.filterEnvelope.setParameters(parameters(s.filterAttack,s.filterDecay,s.filterSustain,s.filterRelease));
    v.pitchEnvelope.setParameters(parameters(s.pitchAttack,s.pitchDecay,s.pitchSustain,s.pitchRelease));
    v.amp.setShape(s.mode==2?s.envelopes[0]:EnvelopeShape{});
    v.pitchEnvelope.setShape(s.envelopes[1]);v.filterEnvelope.setShape(s.envelopes[2]);
}
void SampleEngine::releaseNote(int note,int channel,bool fromUI) noexcept
{
    for(auto& v:voices) if(v.sample && v.note==note && v.channel==channel && v.fromUI==fromUI && v.held && v.sound.mode!=0)
    { v.held=false; v.amp.noteOff(); v.filterEnvelope.noteOff(); v.pitchEnvelope.noteOff(); }
}
void SampleEngine::fadeVoice(Voice& voice) noexcept
{
    if(voice.releaseRemaining<0) voice.releaseRemaining=releaseSamples;
}
void SampleEngine::updateVoices() noexcept
{
    for(auto& voice:voices) if(voice.sample)
    {
        const auto& c=renderControls[(size_t)voice.note];
        if(!c.audible) fadeVoice(voice);
        voice.choke=c.choke;
        // Region, loop markers and direction are latched at note-on. Other edits
        // affect sounding voices, without restarting their playback positions.
        auto previous=voice.sound;
        voice.sound=c.sound;
        voice.sound.reverse=previous.reverse; voice.sound.loop=previous.loop; voice.sound.mode=previous.mode;
        configureEnvelopes(voice);
        voice.equalizer.update(c.sound.equalizer,hostRate);
        voice.attackEqualizer.update(c.sound.equalizer,hostRate);
        voice.left.setTargetValue(c.left); voice.right.setTargetValue(c.right);
        voice.monoMix.setTargetValue((float)c.sound.mono);
        voice.attackGain.setTargetValue(c.sound.attackOn!=0 && c.sound.attackLevel>-60?juce::Decibels::decibelsToGain((float)c.sound.attackLevel):0.0f);
        voice.step.setTargetValue(voice.sample->rate/hostRate*c.pitch);
    }
}
void SampleEngine::updateRenderControls() noexcept
{
    std::copy(current->controls.begin(),current->controls.end(),renderControls.begin());
    for(auto& render:renderControls)
    {
        auto c=render.source;
        for(int n=0;n<numMacros;++n)
        {
            const auto& m=c.macros[(size_t)n]; if(m.target==0) continue;
            const double value=m.minimum+(m.maximum-m.minimum)*macroValues[(size_t)n];
            switch(m.target) {case 1:c.volumeDb=value;break;case 2:c.pan=value;break;case 3:c.tuning=value;break;default:c.sound.*(macroTargets[(size_t)m.target].member)=value;break;}
        }
        resolveEnvelopeMacros(c.sound,c.macros);
        const float gain=c.volumeDb<=-60?0.0f:juce::Decibels::decibelsToGain((float)c.volumeDb);
        render.left=gain*(float)juce::jmin(1.0,1-c.pan); render.right=gain*(float)juce::jmin(1.0,1+c.pan);
        render.pitch=std::pow(2.0,(c.tuning+c.sound.detune/100)/12);
        render.sampleGain*=juce::Decibels::decibelsToGain((float)(c.sound.sampleGain-render.sound.sampleGain));
        render.sound=c.sound;
        std::array<double,6> db{c.sound.sendA,c.sound.sendB,c.sound.sendC,c.sound.sendD,c.sound.sendE,c.sound.sendF};
        for(size_t n=0;n<6;++n) render.sends[n]=db[n]<=-60?0.0f:juce::Decibels::decibelsToGain((float)db[n]);
    }
    previousMacros=macroValues;
}
void SampleEngine::startAuditionVoice(int note) noexcept
{
    const auto previousClock=clock;
    activity[(size_t)note].fetch_add(uint64_t(1)<<32);
    startVoice(note,1,0.8f);
    if(clock!=previousClock) for(auto& v:voices) if(v.sample && v.age==clock) v.preview=true;
}
void SampleEngine::render(juce::AudioBuffer<float>& output,int start,int end) noexcept
{
    if(repeatingNote<0){renderVoices(output,start,end);return;}
    const double beatsPerSample=auditionTempo()/(60.0*hostRate);
    while(start<end)
    {
        if(auditionBeatRemaining<=0){startAuditionVoice(repeatingNote);auditionBeatRemaining+=1.0;}
        const int count=juce::jmin(end-start,juce::jmax(1,(int)std::ceil(auditionBeatRemaining/beatsPerSample)));
        renderVoices(output,start,start+count);start+=count;auditionBeatRemaining-=count*beatsPerSample;
    }
}
void SampleEngine::renderVoices(juce::AudioBuffer<float>& output,int start,int end) noexcept
{
    for(auto& voice:voices)
    {
        if(!voice.sample) continue;
        const auto& sample=*voice.sample;
        const auto& s=voice.sound;
        const auto& c=renderControls[(size_t)voice.note];
        const bool reverse=s.reverse!=0;
        const double driveGain=std::pow(10.0,s.drive/20);
        const double attackHz=juce::jmin(s.attackTone,hostRate*(s.attackType==0?0.22:0.44));
        const double attackNoiseCoefficient=1-std::exp(-juce::MathConstants<double>::twoPi*attackHz/hostRate);
        auto read=[&](int channel,double position) {
            position=juce::jlimit((double)voice.start,(double)(voice.end-1),position);
            const int index=(int)position,next=juce::jmin(index+1,voice.end-1);
            const float fraction=(float)(position-index); const auto* data=sample.audio.getReadPointer(channel);
            return data[index]+fraction*(data[next]-data[index]);
        };
        for(int frame=start;frame<end;++frame)
        {
            if(voice.position>=voice.end || voice.position<voice.start || !voice.amp.isActive())voice.sampleFinished=true;
            const auto attackFrame=voice.elapsedFrames-voice.attackStartFrame;
            const double attackDuration=s.attackDecay*0.001;
            if(attackFrame>=(int64_t)std::ceil(attackDuration*hostRate))voice.attackFinished=true;
            const bool attackPending=!voice.attackFinished && (voice.attackGain.getCurrentValue()>0 || voice.attackGain.getTargetValue()>0);
            if(voice.releaseRemaining==0 || (voice.sampleFinished && !attackPending)) { voice.sample=nullptr; break; }
            // Preview has a bounded gate even when Classic looping is selected.
            if(voice.preview && voice.held && voice.elapsed>=1.0 && s.mode!=0)
            { voice.held=false; voice.amp.noteOff(); voice.filterEnvelope.noteOff(); voice.pitchEnvelope.noteOff(); }
            const bool looping=s.mode==2 && s.loop!=0 && voice.held;
            double lfo=0;
            if(s.lfoOn!=0)
            {
                const double phase=voice.phase;
                switch((int)s.lfoShape)
                {
                    case 0:lfo=std::sin(phase*juce::MathConstants<double>::twoPi);break;
                    case 1:lfo=1.0-4.0*std::abs(phase-0.5);break;
                    case 2:lfo=phase<0.5?1:-1;break;
                    case 3:lfo=2*phase-1;break;
                    case 4:lfo=1-2*phase;break;
                    default:lfo=voice.randomLfo;break;
                }
                lfo*=s.lfoAttack>0?juce::jmin(1.0,voice.elapsed*1000/s.lfoAttack):1;
                const double rate=s.lfoSync!=0 ? tempo/60.0/std::pow(2.0,s.lfoDivision-4) : s.lfoRate;
                voice.phase+=rate/hostRate;
                if(voice.phase>=1) { voice.phase-=std::floor(voice.phase); voice.randomLfo=random(); }
            }
            const double pitchEnv=voice.pitchEnvelope.getNextSample();
            const double filterEnv=voice.filterEnvelope.getNextSample();
            const double semitones=pitchEnv*s.pitchAmount+lfo*s.lfoPitch+(voice.velocity-1)*s.velocityPitch+pitchBend[(size_t)(voice.channel-1)]*s.bendRange;
            const double step=voice.step.getNextValue()*(semitones==0?1.0:std::pow(2.0,semitones/12));
            const double remaining=reverse?voice.position-voice.start+1:voice.end-voice.position;
            const double fadeFrames=juce::jmax(sample.rate*0.002,s.fadeOut*0.001*hostRate*step);
            const float fade=looping?1.0f:(float)juce::jmin(1.0,remaining/fadeFrames);
            const float sampleAmplitude=voice.sampleFinished?0:voice.amp.getNextSample()*fade;
            const float amplitude=(float)(1-s.velocityVolume+s.velocityVolume*voice.velocity)
                *(float)(s.lfoOn!=0?1-s.lfoVolume*(1-lfo)*0.5:1);
            auto value=[&](int channel) {
                float v=read(channel,voice.position);
                const double crossfade=juce::jmin(s.loopFade*sample.rate/1000.0,(voice.loopEnd-voice.loopStart)*0.5);
                if(looping && crossfade>0)
                {
                    double distance=reverse?voice.position-voice.loopStart:voice.loopEnd-voice.position;
                    if(distance>=0 && distance<crossfade)
                    {
                        double alternate=reverse?voice.loopEnd-1-distance:voice.loopStart+crossfade-distance;
                        const float mix=(float)(1-distance/crossfade);
                        v=v*(1-mix)+read(channel,alternate)*mix;
                    }
                }
                return (float)(v*c.sampleGain);
            };
            const auto release=voice.releaseRemaining<0 ? 1.0f : (float)voice.releaseRemaining--/releaseSamples;
            float left=voice.sampleFinished?0:value(0),right=voice.sampleFinished?0:value(sample.audio.getNumChannels()==2?1:0);
            const float mono=(left+right)*0.5f,monoMix=voice.monoMix.getNextValue();
            if(monoMix>=1)left=right=mono;
            else if(monoMix>0) {left+=monoMix*(mono-left);right+=monoMix*(mono-right);}
            if(s.filterOn!=0)
            {
                if(voice.filterCounter--<=0)
                {
                    const auto hz=juce::jlimit(10.0,hostRate*0.45,s.cutoff*std::pow(2.0,filterEnv*s.filterAmount+lfo*s.lfoFilter+(voice.velocity-1)*s.velocityFilter));
                    voice.filterG=std::tan(juce::MathConstants<double>::pi*hz/hostRate); voice.filterK=1.0/s.resonance; voice.filterCounter=15;
                }
                auto filter=[&](float input,int channel) {
                    double v=input;
                    if(s.drive>0) v=std::tanh(v*driveGain);
                    for(int stage=0;stage<=(int)s.filterSlope;++stage)
                    {
                        auto& z1=voice.filter1[(size_t)channel][(size_t)stage]; auto& z2=voice.filter2[(size_t)channel][(size_t)stage];
                        const double g=voice.filterG,k=voice.filterK,a=1/(1+g*(g+k));
                        const double v1=a*(z1+g*(v-z2)),v2=z2+g*v1;
                        z1=2*v1-z1; z2=2*v2-z2;
                        const double high=v-k*v1-v2;
                        switch((int)s.filterType) { case 0:v=v2;break; case 1:v=high;break;case 2:v=v1;break;default:v=high+v2;break; }
                    }
                    return (float)v;
                };
                left=filter(left,0); right=filter(right,1);
            }
            voice.equalizer.process(left,right);
            left*=sampleAmplitude;right*=sampleAmplitude;
            // The attack has its own clock, EQ state and short decay window.
            // Sample envelopes, fades, pitch and sample end cannot silence it.
            const float attackGain=voice.attackGain.getNextValue();
            if(attackPending && attackFrame>=0 && attackGain>0)
            {
                const double attackElapsed=attackFrame/hostRate;
                const float window=(float)attackWindow(attackElapsed,attackDuration);
                float hitLeft=(float)attackOscillator((int)s.attackType,attackHz,hostRate,attackNoiseCoefficient,
                    voice.attackPhase,voice.attackNoise,voice.attackRandom),hitRight=hitLeft;
                voice.attackEqualizer.process(hitLeft,hitRight);
                left+=(hitLeft*window/c.attackPeak)*attackGain;
                right+=(hitRight*window/c.attackPeak)*attackGain;
            }
            const double pan=juce::jlimit(-1.0,1.0,voice.randomPan*s.randomPan+lfo*s.lfoPan);
            left*=voice.left.getNextValue()*amplitude*release*(float)juce::jmin(1.0,1-pan);
            right*=voice.right.getNextValue()*amplitude*release*(float)juce::jmin(1.0,1+pan);
            if(renderingInstruments)
            {
                const int channel=(voice.note-firstNote)*2;
                output.addSample(channel,frame,left);output.addSample(channel+1,frame,right);
            }
            else
            {
            auto write=[&](int bus,float gain) {
                const int ch=busChannels[(size_t)bus]; if(ch<0 || ch>=output.getNumChannels()) return;
                if(bus==0 && mainWidth==1) output.addSample(ch,frame,(left+right)*0.5f*gain);
                else if(ch+1<output.getNumChannels()) {output.addSample(ch,frame,left*gain); output.addSample(ch+1,frame,right*gain);}
            };
            // Disabled auxiliary destinations fall back to Main; disabled sends
            // stay silent so enabling them in a host cannot double the dry signal.
            const int target=(int)s.output;
            write(busChannels[(size_t)target]>=0?target:0,1);
            for(int send=0;send<6;++send) if(c.sends[(size_t)send]>0) write(send+16,c.sends[(size_t)send]);
            }
            voice.position+=reverse?-step:step;
            if(looping)
            {
                const double crossfade=juce::jmin(s.loopFade*sample.rate/1000.0,(voice.loopEnd-voice.loopStart)*0.5);
                const double loopLength=juce::jmax(1.0,voice.loopEnd-voice.loopStart-crossfade);
                if(!reverse && voice.position>=voice.loopEnd) voice.position=voice.loopStart+crossfade+std::fmod(voice.position-voice.loopEnd,loopLength);
                if(reverse && voice.position<voice.loopStart) voice.position=voice.loopEnd-1-crossfade-std::fmod(voice.loopStart-voice.position,loopLength);
            }
            voice.elapsed=++voice.elapsedFrames/hostRate;
        }
    }
}
void SampleEngine::mixEffects(juce::AudioBuffer<float>& output,int offset,int frames) noexcept
{
    for(int note=firstNote;note<firstNote+maxInstruments;++note)
    {
        const auto& c=renderControls[(size_t)note];if(!current->samples[(size_t)note] || !c.audible)continue;
        const int input=(note-firstNote)*2;
        auto write=[&](int bus,float gain){
            const int channel=busChannels[(size_t)bus];if(channel<0 || channel>=output.getNumChannels())return;
            if(bus==0 && mainWidth==1)
            {
                output.addFrom(channel,offset,instrumentAudio,input,0,frames,gain*0.5f);
                output.addFrom(channel,offset,instrumentAudio,input+1,0,frames,gain*0.5f);
            }
            else if(channel+1<output.getNumChannels())
                for(int lane=0;lane<2;++lane)output.addFrom(channel+lane,offset,instrumentAudio,input+lane,0,frames,gain);
        };
        const int destination=(int)c.sound.output;write(busChannels[(size_t)destination]>=0?destination:0,1);
        for(int send=0;send<6;++send)if(c.sends[(size_t)send]>0)write(send+16,c.sends[(size_t)send]);
    }
}
void SampleEngine::process(juce::AudioBuffer<float>& output,const juce::MidiBuffer& midi) noexcept
{
    Bank* next=nullptr;
    do { next=published.load(); hazard.store(next); } while(next!=published.load());
    if(next!=current)
    {
        if(!next || next->sampleGeneration!=sampleGeneration) stopVoices();
        if(current && next && current->kitEpoch!=next->kitEpoch)repeatNote.store(-1);
        current=next; sampleGeneration=next?next->sampleGeneration:UINT64_MAX;
        if(current) { updateRenderControls(); updateVoices(); }
    }
    else if(current && macroValues!=previousMacros) {updateRenderControls();updateVoices();}
    output.clear();
    if(!current || current->rate!=hostRate || current->kitEpoch!=library.activeKitEpoch()) { stopVoices(); return; }
    const bool stopped=panic.exchange(false);
    if(stopped)stopVoices();
    if(pausePreview.exchange(false))for(auto& voice:voices)if(voice.preview)fadeVoice(voice);
    if(repeatEpoch.load()!=library.activeKitEpoch())repeatNote.store(-1);
    int requestedRepeat=repeatNote.load();
    if(requestedRepeat>=0 && !current->samples[(size_t)requestedRepeat]){repeatNote.store(-1);requestedRepeat=-1;}
    if(requestedRepeat!=repeatingNote){repeatingNote=requestedRepeat;auditionBeatRemaining=0;}

    {
        auto commands=keyboardQueue.read(keyboardQueue.getNumReady());
        commands.forEach([&](int index) {
            const auto& command=keyboardCommands[(size_t)index];
            if(stopped || command.kitEpoch!=current->kitEpoch)return;
            indicateNote(command.note,1,command.down,true);
            if(command.down)startVoice(command.note,1,0.8f,true);
            else releaseNote(command.note,1,true);
        });
    }
    if(auto note=previewNote.exchange(-1);note>=0)
    {
        startAuditionVoice(note);
    }
    renderingInstruments=effects && (effects->active() || current->hasEffects);
    if(stopped && effects)effects->reset();
    const int frames=output.getNumSamples();
    for(int base=0;base<frames || base==0;base+=juce::jmax(1,renderingInstruments?effectBlockSize:frames))
    {
        const int count=renderingInstruments?juce::jmin(effectBlockSize,frames-base):frames;
        if(renderingInstruments){instrumentAudio.setSize(EffectRack::channels,count,false,false,true);instrumentAudio.clear();}
        auto& target=renderingInstruments?instrumentAudio:output;
        int position=0;
        for(const auto metadata:midi)
        {
            if(metadata.samplePosition<base || (metadata.samplePosition>=base+count && base+count<frames))continue;
            const int offset=juce::jlimit(position,count,metadata.samplePosition-base);
            render(target,position,offset);position=offset;
            // Inspect MIDI without allocating a MidiMessage for variable-length SysEx.
            if(metadata.numBytes<1)continue;
            const auto* data=metadata.data;const int command=data[0]&0xf0,channel=(data[0]&0x0f)+1;
            if(command==0x90 && metadata.numBytes>=3 && data[2]>0){indicateNote(data[1],channel,true);startVoice(data[1],channel,data[2]/127.0f);}
            else if((command==0x80 || command==0x90) && metadata.numBytes>=3){indicateNote(data[1],channel,false);releaseNote(data[1],channel);}
            else if(command==0xe0 && metadata.numBytes>=3)pitchBend[(size_t)(channel-1)]=((data[2]*128+data[1])-8192)/8192.0;
            else if(command==0xb0 && metadata.numBytes>=3 && (data[1]==120 || data[1]==123))stopVoices(channel);
        }
        render(target,position,count);
        if(renderingInstruments && count>0){effects->process(instrumentAudio,current->kitEpoch,playHead,nonRealtime);mixEffects(output,base,count);}
    }
    freePhase+=output.getNumSamples()/hostRate;
}
}
