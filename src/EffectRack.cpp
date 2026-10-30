#include "EffectRack.h"
#include "BuiltinEffects.h"
#include <set>

namespace drum
{
class EffectRack::HostedProcessor final : public juce::AudioProcessor
{
public:
    HostedProcessor(std::unique_ptr<juce::AudioProcessor> p, EffectRack& o, bool mono,bool sidechain)
        : AudioProcessor(BusesProperties().withInput("Input",sidechain?juce::AudioChannelSet::discreteChannels(4):juce::AudioChannelSet::stereo(),true)
                         .withOutput("Output",juce::AudioChannelSet::stereo(),true)), plugin(std::move(p)), hasSidechain(sidechain), owner(o), monoOnly(mono) {}
    void prepareToPlay(double hz,int frames) override
    {
        for(auto& peak:outputPeaks)peak.store(0);
        plugin->setRateAndBufferSizeDetails(hz,frames);plugin->prepareToPlay(hz,frames);
        setLatencySamples(plugin->getLatencySamples());midi.ensureSize(8192);generation=owner.resetGeneration.load();
        if(hasSidechain)sidechainAudio.setSize(4,frames);
    }
    void releaseResources() override { plugin->releaseResources(); }
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override
    {
        const juce::ScopedLock guard(plugin->getCallbackLock());
        const auto next=owner.resetGeneration.load();
        if(next!=generation){plugin->reset();generation=next;}
        plugin->setPlayHead(getPlayHead());plugin->setNonRealtime(owner.offline.load());midi.clear();
        if(hasSidechain)
        {
            sidechainAudio.setSize(4,audio.getNumSamples(),false,false,true);
            for(int c=0;c<4;++c)sidechainAudio.copyFrom(c,0,audio,c,0,audio.getNumSamples());
            const auto mix=sidechainMix.load(),gain=sidechainGain.load();
            for(int c=0;c<2;++c){sidechainAudio.applyGain(c+2,0,audio.getNumSamples(),gain*mix);sidechainAudio.addFrom(c+2,0,audio,c,0,audio.getNumSamples(),1-mix);}
            plugin->processBlock(sidechainAudio,midi);
            for(int c=0;c<2;++c)audio.copyFrom(c,0,sidechainAudio,c,0,audio.getNumSamples());
        }
        else if(monoOnly)
        {
            audio.applyGain(0,0,audio.getNumSamples(),0.5f);audio.addFrom(0,0,audio,1,0,audio.getNumSamples(),0.5f);
            float* data[]={audio.getWritePointer(0)};juce::AudioBuffer<float> mono(data,1,audio.getNumSamples());
            plugin->processBlock(mono,midi);audio.copyFrom(1,0,audio,0,0,audio.getNumSamples());
        }
        else plugin->processBlock(audio,midi);
        // A broken processor must not spread NaNs through the whole kit.
        const float decay=std::exp(-(float)audio.getNumSamples()/((float)getSampleRate()*.5f));
        for(int c=0;c<2;++c){float peak=0;for(int n=0;n<audio.getNumSamples();++n){auto value=audio.getSample(c,n);
            if(!std::isfinite(value))audio.setSample(c,n,0);else peak=juce::jmax(peak,std::abs(value));}
            outputPeaks[(size_t)c].store(juce::jmax(peak,outputPeaks[(size_t)c].load()*decay),std::memory_order_relaxed);}
    }
    const juce::String getName() const override { return plugin->getName(); }
    bool acceptsMidi() const override {return false;} bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return plugin->getTailLengthSeconds();}
    bool hasEditor() const override {return false;} juce::AudioProcessorEditor* createEditor() override {return nullptr;}
    int getNumPrograms() override {return 1;} int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {} const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& b) override {plugin->getStateInformation(b);}
    void setStateInformation(const void* b,int size) override {plugin->setStateInformation(b,size);}
    std::unique_ptr<juce::AudioProcessor> plugin;
    bool hasSidechain=false;
    std::atomic<float> sidechainGain {1},sidechainMix {1};
    std::array<std::atomic<float>,2> outputPeaks{};
private:
    EffectRack& owner; bool monoOnly; uint64_t generation=0;juce::MidiBuffer midi;
    juce::AudioBuffer<float> sidechainAudio;
};
class EffectRack::PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow(Graph::Node::Ptr n,juce::AudioProcessor& plugin)
        :DocumentWindow(plugin.getName(),juce::Colour(0xff19212e),closeButton),node(n)
    {
        setUsingNativeTitleBar(true);
        auto* editor=plugin.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(plugin);
        setContentOwned(editor,true);setResizable(editor->isResizable(),false);centreWithSize(getWidth(),getHeight());
    }
    ~PluginWindow() override {clearContentComponent();}
    void closeButtonPressed() override {setVisible(false);}
private:
    Graph::Node::Ptr node;
};
struct EffectRack::Runtime
{
    EffectSlot saved;
    juce::String kit,instrument,error;
    Graph::Node::Ptr node;
    std::unique_ptr<PluginWindow> window;
    HostedProcessor* processor() const {return node?static_cast<HostedProcessor*>(node->getProcessor()):nullptr;}
};
EffectRack::EffectRack(KitLibrary& model,std::function<void()> callback,std::function<void(int)> latencyCallback)
    :library(model),changed(std::move(callback)),latency(std::move(latencyCallback))
{
    graph.setPlayConfigDetails(channels,channels,44100,512);
    input=graph.addNode(std::make_unique<Graph::AudioGraphIOProcessor>(Graph::AudioGraphIOProcessor::audioInputNode),{},Graph::UpdateKind::none);
    output=graph.addNode(std::make_unique<Graph::AudioGraphIOProcessor>(Graph::AudioGraphIOProcessor::audioOutputNode),{},Graph::UpdateKind::none);
    graph.addListener(this);noMidi.ensureSize(8192);startTimerHz(10);
}
EffectRack::~EffectRack()
{
    stopTimer();graph.removeListener(this);
    for(auto& [id,r]:nodes)if(auto* p=r->processor())p->plugin->removeListener(this);
    nodes.clear();graph.clear();
}
void EffectRack::prepare(double hz,int frames)
{
    rate.store(hz);blockSize.store(juce::jmax(1,frames));
    const juce::ScopedLock guard(managementLock);
    graph.setPlayConfigDetails(channels,channels,hz,blockSize.load());graph.prepareToPlay(hz,blockSize.load());
    latency(graph.getLatencySamples());
}
void EffectRack::release() {const juce::ScopedLock guard(managementLock);graph.releaseResources();}
void EffectRack::stateRestored() {restored.store(true);}
void EffectRack::audioProcessorParameterChanged(juce::AudioProcessor*,int,float) {dirty.store(true);changed();}
void EffectRack::audioProcessorChanged(juce::AudioProcessor* p,const ChangeDetails& details)
{
    if(p==&graph){latencyDirty.store(true);return;}
    dirty.store(true);if(details.latencyChanged)latencyDirty.store(true);changed();
}
void EffectRack::capture()
{
    const juce::ScopedLock guard(managementLock);
    if(restored.load())return;
    const auto state=library.snapshot();
    for(const auto& kit:state.kits)for(const auto& group:kit.groups)for(const auto& instrument:group.instruments)
    {
        auto slots=instrument.effects;bool updated=false;
        for(auto& slot:slots)if(auto it=nodes.find(slot.id);it!=nodes.end())
        {
            auto& runtime=*it->second;auto* p=runtime.processor();
            // Do not overwrite a newer restore or replacement with a retiring instance.
            if(!p || slot.description!=runtime.saved.description || slot.state!=runtime.saved.state)continue;
            juce::MemoryBlock saved;{const juce::ScopedLock audioGuard(p->plugin->getCallbackLock());p->plugin->getStateInformation(saved);}
            if(saved.getSize()>16*1024*1024){runtime.error="State exceeds 16 MiB; settings cannot be saved.";continue;}
            auto encoded=saved.toBase64Encoding();
            if(encoded!=slot.state){slot.state=encoded;updated=true;}
            if(runtime.error.startsWith("State "))runtime.error.clear();
        }
        if(updated)
        {
            const auto result=library.setEffects(kit.id,instrument.id,slots);
            for(const auto& slot:slots)if(auto it=nodes.find(slot.id);it!=nodes.end() && it->second->processor())
            {
                if(result.ok)it->second->saved.state=slot.state;
                else it->second->error="State could not be saved: project effect storage limit reached.";
            }
        }
    }
}
void EffectRack::timerCallback()
{
    if(++captureTicks>=10){captureTicks=0;if(dirty.exchange(false))capture();}
    sync();
}
void EffectRack::sync()
{
    const juce::ScopedLock guard(managementLock);
    const bool reload=restored.exchange(false),latencyChanged=latencyDirty.exchange(false);
    auto state=library.snapshot();
    if(!reload && state.revision==seenRevision && !latencyChanged)return;
    // Keep edited settings when moving to another kit, but never overwrite a recall.
    if(!reload && graphEpoch.load()!=state.kitEpoch){capture();state=library.snapshot();}
    seenRevision=state.revision;const auto* kit=activeKit(state);
    std::vector<std::pair<int,std::vector<EffectSlot>>> next;
    for(const auto& group:kit->groups)for(const auto& instrument:group.instruments)if(!instrument.effects.empty())next.push_back({instrument.note,instrument.effects});
    if(!reload && next==layout && graphEpoch.load()==state.kitEpoch && !latencyChanged)return;
    std::set<juce::String> wanted;for(const auto& [note,slots]:next)for(const auto& slot:slots)wanted.insert(slot.id);
    for(auto it=nodes.begin();it!=nodes.end();)
    {
        if(reload || !wanted.contains(it->first))
        {
            auto& runtime=*it->second;runtime.window.reset();
            if(auto* p=runtime.processor()){p->plugin->removeListener(this);graph.removeNode(runtime.node,Graph::UpdateKind::none);}
            it=nodes.erase(it);
        }
        else ++it;
    }
    for(const auto& group:kit->groups)for(const auto& instrument:group.instruments)for(const auto& slot:instrument.effects)
    {
        auto& runtime=nodes[slot.id];
        if(runtime && (runtime->saved.description!=slot.description || runtime->saved.state!=slot.state))
        {
            runtime->window.reset();if(auto* p=runtime->processor()){p->plugin->removeListener(this);graph.removeNode(runtime->node,Graph::UpdateKind::none);}runtime.reset();
        }
        if(!runtime)
        {
            runtime=std::make_unique<Runtime>();runtime->kit=kit->id;runtime->instrument=instrument.id;runtime->saved=slot;
            juce::PluginDescription description;auto xml=juce::parseXML(slot.description);
            if(!xml || !description.loadFromXml(*xml) || (!isBuiltin(description) && description.pluginFormatName!="VST3") || description.isInstrument || description.name=="DJL Drum")
                runtime->error="Unsupported device. Choose a VST3 audio effect.";
            else if(!isBuiltin(description) && !juce::File(description.fileOrIdentifier).exists())runtime->error="Missing VST3. Restore its file, then Retry.";
            else
            {
                std::unique_ptr<juce::AudioProcessor> plugin;
                if(isBuiltin(description))plugin=createBuiltin(description);
                else plugin=format.createInstanceFromDescription(description,rate.load(),blockSize.load(),runtime->error);
                if(plugin)
                {
                    juce::MemoryBlock saved;
                    if(slot.state.isNotEmpty() && saved.fromBase64Encoding(slot.state))plugin->setStateInformation(saved.getData(),(int)saved.getSize());
                    auto busLayout=plugin->getBusesLayout();
                    for(auto& bus:busLayout.inputBuses)bus=juce::AudioChannelSet::disabled();
                    for(auto& bus:busLayout.outputBuses)bus=juce::AudioChannelSet::disabled();
                    bool configured=false,mono=false,sidechain=false;
                    if(!busLayout.inputBuses.isEmpty() && !busLayout.outputBuses.isEmpty())
                    {
                        busLayout.inputBuses.set(0,juce::AudioChannelSet::stereo());busLayout.outputBuses.set(0,juce::AudioChannelSet::stereo());configured=plugin->setBusesLayout(busLayout);
                        if(!configured){busLayout.inputBuses.set(0,juce::AudioChannelSet::mono());busLayout.outputBuses.set(0,juce::AudioChannelSet::mono());configured=plugin->setBusesLayout(busLayout);mono=configured;}
                        if(configured && !mono && busLayout.inputBuses.size()>1)
                        {auto withSidechain=busLayout;withSidechain.inputBuses.set(1,juce::AudioChannelSet::stereo());sidechain=plugin->setBusesLayout(withSidechain);}
                    }
                    if(configured)
                    {
                        plugin->addListener(this);runtime->node=graph.addNode(std::make_unique<HostedProcessor>(std::move(plugin),*this,mono,sidechain),{},Graph::UpdateKind::none);runtime->error.clear();
                    }
                    else runtime->error="This effect needs an unsupported audio layout or sidechain.";
                }
            }
        }
        runtime->saved.bypassed=slot.bypassed;
        if(auto* p=runtime->processor()){p->setLatencySamples(p->plugin->getLatencySamples());p->suspendProcessing(slot.bypassed);p->sidechainGain.store(juce::Decibels::decibelsToGain((float)slot.sidechainGain));p->sidechainMix.store((float)slot.sidechainMix);}
    }
    for(const auto& connection:graph.getConnections())graph.removeConnection(connection,Graph::UpdateKind::none);
    bool any=false;double longestTail=0;
    for(auto& muted:mutedSidechainSources)muted.store(false);
    for(int note=firstNote;note<firstNote+maxInstruments;++note)
    {
        Graph::NodeID previous=input->nodeID;int previousChannel=(note-firstNote)*2;double chainTail=0;
        for(const auto& [assigned,slots]:next)if(assigned==note)for(const auto& slot:slots)
        {
            auto& r=*nodes.at(slot.id);if(slot.bypassed || !r.node)continue;any=true;
            for(int c=0;c<2;++c)graph.addConnection({{previous,previousChannel+c},{r.node->nodeID,c}},Graph::UpdateKind::none);
            if(r.processor()->hasSidechain && slot.sidechainNote>=firstNote)
            {
                for(int c=0;c<2;++c)graph.addConnection({{input->nodeID,(slot.sidechainNote-firstNote)*2+c},{r.node->nodeID,2+c}},Graph::UpdateKind::none);
                if(slot.sidechainMute && slot.sidechainNote!=note)mutedSidechainSources[(size_t)(slot.sidechainNote-firstNote)].store(true);
            }
            previous=r.node->nodeID;previousChannel=0;
            const double seconds=r.processor()->plugin->getTailLengthSeconds();chainTail+=std::isfinite(seconds)?juce::jlimit(0.0,3600.0,seconds):3600;
        }
        for(int c=0;c<2;++c)graph.addConnection({{previous,previousChannel+c},{output->nodeID,(note-firstNote)*2+c}},Graph::UpdateKind::none);
        longestTail=juce::jmax(longestTail,chainTail);
    }
    graph.rebuild();layout=std::move(next);graphEpoch.store(state.kitEpoch);hasEffects.store(any);tail.store(longestTail);latency(graph.getLatencySamples());
}
void EffectRack::process(juce::AudioBuffer<float>& audio,uint64_t epoch,juce::AudioPlayHead* playHead,bool nonRealtime)
{
    if(epoch!=graphEpoch.load()){audio.clear();return;}
    if(!hasEffects.load())return;
    offline.store(nonRealtime);graph.setPlayHead(playHead);noMidi.clear();graph.processBlock(audio,noMidi);
    for(int n=0;n<maxInstruments;++n)if(mutedSidechainSources[(size_t)n].load()){audio.clear(n*2,0,audio.getNumSamples());audio.clear(n*2+1,0,audio.getNumSamples());}
}
juce::OwnedArray<juce::PluginDescription> EffectRack::discover(const juce::File& file)
{
    juce::OwnedArray<juce::PluginDescription> found;
    if(file.hasFileExtension("vst3"))format.findAllTypesForFile(found,file.getFullPathName());
    for(int n=found.size();--n>=0;)if(found[n]->isInstrument || found[n]->name=="DJL Drum")found.remove(n);
    return found;
}
Result EffectRack::edit(const juce::String& kit,const juce::String& instrument,const std::function<Result(std::vector<EffectSlot>&)>& operation)
{
    capture();const auto state=library.snapshot();const Kit* k=nullptr;for(const auto& candidate:state.kits)if(candidate.id==kit)k=&candidate;
    const auto* i=k?findInstrument(*k,instrument):nullptr;if(!i)return Result::fail("Instrument no longer exists.");
    auto slots=i->effects;auto result=operation(slots);if(!result.ok)return result;
    auto saved=library.setEffects(kit,instrument,slots);if(!saved.ok)return saved;
    sync();changed();return result;
}
Result EffectRack::add(const juce::String& kit,const juce::String& instrument,const juce::PluginDescription& description,int position)
{
    if((!isBuiltin(description) && description.pluginFormatName!="VST3") || description.isInstrument || description.name=="DJL Drum")return Result::fail("Choose a VST3 audio effect.");
    return edit(kit,instrument,[&](auto& slots){
        if(slots.size()>=maxEffects)return Result::fail("This chain already contains 16 effects.");
        EffectSlot slot{juce::Uuid().toString(),description.createXml()->toString(),{},false};
        slots.insert(slots.begin()+(position<0?(int)slots.size():juce::jlimit(0,(int)slots.size(),position)),slot);return Result::success(slot.id);
    });
}
Result EffectRack::remove(const juce::String& kit,const juce::String& instrument,const juce::String& slot)
{
    return edit(kit,instrument,[&](auto& slots){std::erase_if(slots,[&](const auto& s){return s.id==slot;});return Result::success();});
}
std::vector<EffectSlot> EffectRack::copyChain(const juce::String& kit,const juce::String& instrument)
{
    capture();auto state=library.snapshot();for(const auto& k:state.kits)if(k.id==kit)if(auto* i=findInstrument(k,instrument))return i->effects;return {};
}
Result EffectRack::replaceChain(const juce::String& kit,const juce::String& instrument,std::vector<EffectSlot> replacement)
{
    for(auto& slot:replacement){slot.id=juce::Uuid().toString();slot.sidechainNote=-1;slot.sidechainMute=false;}
    return edit(kit,instrument,[&](auto& slots){slots=std::move(replacement);return Result::success();});
}
Result EffectRack::move(const juce::String& kit,const juce::String& instrument,const juce::String& slot,int position)
{
    return edit(kit,instrument,[&](auto& slots){
        auto it=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s.id==slot;});if(it==slots.end())return Result::fail("Effect no longer exists.");
        auto copy=*it;slots.erase(it);slots.insert(slots.begin()+juce::jlimit(0,(int)slots.size(),position),copy);return Result::success(slot);
    });
}
Result EffectRack::bypass(const juce::String& kit,const juce::String& instrument,const juce::String& slot,bool bypassed)
{
    return edit(kit,instrument,[&](auto& slots){for(auto& s:slots)if(s.id==slot){s.bypassed=bypassed;return Result::success(slot);}return Result::fail("Effect no longer exists.");});
}
void EffectRack::retry(const juce::String& slot)
{
    {const juce::ScopedLock guard(managementLock);if(auto it=nodes.find(slot);it!=nodes.end() && !it->second->node)nodes.erase(it);seenRevision=UINT64_MAX;layout.clear();}sync();
}
juce::String EffectRack::status(const juce::String& slot) const
{
    const juce::ScopedLock guard(managementLock);auto it=nodes.find(slot);if(it==nodes.end())return "Loading...";
    const auto& r=*it->second;return r.error.isNotEmpty()?r.error:r.saved.bypassed?"Bypassed - dry pass-through":"Active";
}
juce::AudioProcessor* EffectRack::instance(const juce::String& slot) const
{
    const juce::ScopedLock guard(managementLock);auto it=nodes.find(slot);return it!=nodes.end() && it->second->processor()?it->second->processor()->plugin.get():nullptr;
}
std::array<float,2> EffectRack::peaks(const juce::String& slot) const
{
    const juce::ScopedLock guard(managementLock);auto it=nodes.find(slot);
    if(it==nodes.end() || it->second->saved.bypassed || !it->second->processor())return {};
    auto* p=it->second->processor();return {p->outputPeaks[0].load(std::memory_order_relaxed),p->outputPeaks[1].load(std::memory_order_relaxed)};
}
void EffectRack::openEditor(const juce::String& slot)
{
    const juce::ScopedLock guard(managementLock);auto it=nodes.find(slot);if(it==nodes.end() || !it->second->processor())return;
    auto& r=*it->second;if(!r.window)r.window=std::make_unique<PluginWindow>(r.node,*r.processor()->plugin);
    r.window->setVisible(true);r.window->toFront(true);
}
bool EffectRack::supportsSidechain(const juce::String& slot) const
{
    const juce::ScopedLock guard(managementLock);auto it=nodes.find(slot);return it!=nodes.end() && it->second->processor() && it->second->processor()->hasSidechain;
}
Result EffectRack::sidechain(const juce::String& kit,const juce::String& instrument,const juce::String& slot,int note,double gain,double mix,bool mute)
{
    return edit(kit,instrument,[&](auto& slots){for(auto& s:slots)if(s.id==slot){s.sidechainNote=note;s.sidechainGain=gain;s.sidechainMix=mix;s.sidechainMute=mute;return Result::success(slot);}return Result::fail("Effect no longer exists.");});
}
}
