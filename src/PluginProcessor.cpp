#include "PluginProcessor.h"
#include "PluginEditor.h"

DrumAudioProcessor::DrumAudioProcessor(bool track)
    : AudioProcessor([] {
        BusesProperties buses;
        buses=buses.withOutput("Output",juce::AudioChannelSet::stereo(),true);
        for(int n=2;n<=16;++n) buses=buses.withOutput("Out "+juce::String(n),juce::AudioChannelSet::stereo(),false);
        for(int n=0;n<6;++n) buses=buses.withOutput("Send "+juce::String::charToString((juce::juce_wchar)('A'+n)),juce::AudioChannelSet::stereo(),false);
        return buses;
    }()),trackInstance(track),library(track)
{
    if(trackInstance){
        auto state=library.snapshot();library.rename(state.activeKit,"MIDI Track");library.rename(state.kits[0].groups[0].id,"Sounds");
        libraryPreview=std::make_unique<DrumAudioProcessor>(false);
        libraryPreview->useLibrarySound=[this](const juce::String& kitId,const juce::String& instrumentId){
            libraryPreview->effects.capture();auto source=libraryPreview->library.snapshot();const drum::Kit* kit=nullptr;for(const auto& candidate:source.kits)if(candidate.id==kitId)kit=&candidate;
            auto* sound=kit?drum::findInstrument(*kit,instrumentId):nullptr;if(!sound)return drum::Result::fail("Select a library sound first.");auto target=library.snapshot();auto* destination=drum::activeKit(target);auto result=library.importInstrument(target.activeKit,destination->groups[0].id,*sound);if(result.ok)triggerAsyncUpdate();return result;
        };
        libraryPreview->saveSharedLibrary=[this]{libraryPreview->effects.capture();return sharedStore.save(libraryPreview->library);};
        libraryPreview->reloadSharedLibrary=[this]{return sharedStore.load(libraryPreview->library);};
    }
    sampleEngine.setEffects(&effects);
    library.beforeClone=[this]{effects.capture();};
    library.afterRestore=[this]{effects.stateRestored();};
    for(int n=0;n<drum::numMacros;++n)
    {
        auto* parameter=new juce::AudioParameterFloat(juce::ParameterID("macro"+juce::String(n+1),1),"Macro "+juce::String(n+1),0.0f,1.0f,0.5f);
        addParameter(parameter); macros[(size_t)n]=parameter;
    }
}

DrumAudioProcessor::~DrumAudioProcessor() { previewActive.store(false);if(libraryPreview)libraryPreview->getSampleEngine().shutdown();sampleEngine.shutdown();cancelPendingUpdate(); }
void DrumAudioProcessor::handleAsyncUpdate()
{
    if(trackInstance)migrateLegacyKits();
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void DrumAudioProcessor::prepareToPlay(double rate, int blockSize)
{
    if(juce::MessageManager::getInstance()->isThisTheMessageThread())effects.sync();
    effects.prepare(rate,blockSize);
    sampleEngine.prepare(rate,blockSize);
    std::array<int,22> channels; channels.fill(-1); int offset=0;
    for(int n=0;n<getBusCount(false);++n)
    {
        auto count=getChannelCountOfBus(false,n);
        if(count>0) { channels[(size_t)n]=offset; offset+=count; }
    }
    sampleEngine.setOutputChannels(channels,getMainBusNumOutputChannels());
    if(libraryPreview){libraryPreview->setRateAndBufferSizeDetails(rate,blockSize);libraryPreview->prepareToPlay(rate,blockSize);previewBuffer.setSize(2,juce::jmax(1,blockSize));}

}
void DrumAudioProcessor::releaseResources() { sampleEngine.release();effects.release();if(libraryPreview)libraryPreview->releaseResources(); }

bool DrumAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    if(layout.inputBuses.size()!=0 || layout.outputBuses.size()!=22) return false;
    for(int n=1;n<layout.outputBuses.size();++n)
        if(!layout.outputBuses[n].isDisabled() && layout.outputBuses[n]!=juce::AudioChannelSet::stereo()) return false;
    return (output == juce::AudioChannelSet::mono()
         || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet().isDisabled();
}

void DrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    std::array<float,drum::numMacros> values;
    for(int n=0;n<drum::numMacros;++n) values[(size_t)n]=macros[(size_t)n]->get();
    sampleEngine.setMacros(values);
    if(auto* playhead=getPlayHead()) if(auto position=playhead->getPosition())
        if(auto bpm=position->getBpm(); bpm && std::isfinite(*bpm)) sampleEngine.setTempo(*bpm);
    sampleEngine.setProcessingContext(getPlayHead(),isNonRealtime());
    sampleEngine.process(buffer, midi);
    midi.clear();
    if(libraryPreview){libraryPreview->setPlayHead(getPlayHead());libraryPreview->setNonRealtime(isNonRealtime());}
    if(libraryPreview && previewActive.load() && previewBuffer.getNumSamples()>0){
        for(int offset=0;offset<buffer.getNumSamples();){int count=juce::jmin(previewBuffer.getNumSamples(),buffer.getNumSamples()-offset);float* data[]={previewBuffer.getWritePointer(0),previewBuffer.getWritePointer(1)};juce::AudioBuffer<float> block(data,2,count);previewMidi.clear();libraryPreview->processBlock(block,previewMidi);
            if(getMainBusNumOutputChannels()==1){buffer.addFrom(0,offset,block,0,0,count,.5f);buffer.addFrom(0,offset,block,1,0,count,.5f);}else for(int channel=0;channel<2;++channel)buffer.addFrom(channel,offset,block,channel,0,count);offset+=count;}
    }
}

juce::AudioProcessorEditor* DrumAudioProcessor::createEditor()
{
    return new DrumAudioProcessorEditor(*this);
}

void DrumAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    effects.capture();if(libraryPreview && libraryLoaded.load())libraryPreview->effects.capture();
    const auto state = library.toXml();
    for(int n=0;n<drum::numMacros;++n) state->setAttribute("macro"+juce::String(n+1),(double)macros[(size_t)n]->get());
    if(trackInstance){juce::XmlElement session("DrumSession");session.setAttribute("version",1);session.addChildElement(new juce::XmlElement(*state));const juce::ScopedLock lock(migrationLock);if(legacyKits)session.createNewChildElement("LegacyKits")->addChildElement(new juce::XmlElement(*legacyKits));if(libraryPreview && sharedLibraryDirty()){auto* draft=session.createNewChildElement("LibraryDraft");draft->addChildElement(libraryPreview->library.toXml().release());if(auto base=sharedStore.baselineCopy())draft->createNewChildElement("Baseline")->addChildElement(base.release());}copyXmlToBinary(session,data);}else copyXmlToBinary(*state, data);
}

void DrumAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 768 * 1024 * 1024) return;
    if (auto saved = getXmlFromBinary(data, size))
    {
        const juce::XmlElement* state=saved.get();std::unique_ptr<juce::XmlElement> converted,pending;
        if(saved->hasTagName("DrumSession")){if(saved->getIntAttribute("version")!=1)return;state=saved->getChildByName("DrumState");if(!state)return;if(auto* old=saved->getChildByName("LegacyKits"))if(auto* kits=old->getChildByName("DrumState"))pending=std::make_unique<juce::XmlElement>(*kits);}
        else if(trackInstance){converted=trackFromLibrary(*saved);if(!converted)return;pending=std::make_unique<juce::XmlElement>(*saved);state=converted.get();}

        if(trackInstance && saved->hasTagName("DrumSession")){
            drum::KitLibrary check;if(!check.restore(*state) || check.snapshot().kits.size()!=1)return;
            if(auto* draft=saved->getChildByName("LibraryDraft")){auto* content=draft->getChildByName("DrumState");auto* base=draft->getChildByName("Baseline");if(!content || !base || !base->getChildByName("DrumState"))return;drum::KitLibrary draftCheck,baseCheck;if(!draftCheck.restore(*content) || !baseCheck.restore(*base->getChildByName("DrumState")))return;}
            if(pending){drum::KitLibrary old;if(!old.restore(*pending))return;}
        }
        std::array<float,drum::numMacros> values;
        for(int n=0;n<drum::numMacros;++n)
        {
            const auto key="macro"+juce::String(n+1);
            auto raw=state->getStringAttribute(key,"0.5");char* end=nullptr;auto v=std::strtod(raw.toRawUTF8(),&end);
            if(end==raw.toRawUTF8() || *end!='\0' || !std::isfinite(v) || v<0 || v>1) return;
            values[(size_t)n]=(float)v;
        }
        if(library.restore(*state))
        {
          if(trackInstance){const juce::ScopedLock lock(migrationLock);legacyKits=std::move(pending);migrationAttempted=false;triggerAsyncUpdate();}

          for(int n=0;n<drum::numMacros;++n)
          {
            // Restoring state is not a user gesture. The wrapper synchronises
            // its controller afterwards; do not mark the host project edited.
            juce::AudioProcessorParameter* parameter=macros[(size_t)n];
            parameter->setValue(values[(size_t)n]);
          }
          if(trackInstance)if(auto* draft=saved->getChildByName("LibraryDraft"))if(auto* data=draft->getChildByName("DrumState"))if(auto* base=draft->getChildByName("Baseline"))if(auto* previous=base->getChildByName("DrumState")){if(sharedStore.restoreBaseline(*previous) && libraryPreview->library.restore(*data))libraryLoaded=true;}
          // Hosts may recall on either thread. Load devices immediately on the
          // message thread; the rack timer handles worker-thread recalls.
          if(juce::MessageManager::getInstance()->isThisTheMessageThread())effects.sync();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumAudioProcessor(true); }

std::unique_ptr<juce::XmlElement> DrumAudioProcessor::trackFromLibrary(const juce::XmlElement& xml)
{
    drum::KitLibrary source;if(!source.restore(xml))return {};auto canonical=source.toXml();auto snapshot=source.snapshot();auto active=snapshot.activeKit;
    std::unique_ptr<juce::XmlElement> selected;for(auto* kit:canonical->getChildIterator())if(kit->getStringAttribute("id")==active)selected=std::make_unique<juce::XmlElement>(*kit);if(!selected)return {};
    auto group=std::make_unique<juce::XmlElement>("Group");group->setAttribute("id",juce::Uuid().toString());group->setAttribute("name","Sounds");group->setAttribute("colour",0);
    for(auto* oldGroup:selected->getChildIterator())for(auto* instrument:oldGroup->getChildIterator())group->addChildElement(new juce::XmlElement(*instrument));
    selected->deleteAllChildElements();selected->addChildElement(group.release());canonical->deleteAllChildElements();canonical->addChildElement(selected.release());for(int n=0;n<drum::numMacros;++n){auto key="macro"+juce::String(n+1);canonical->setAttribute(key,xml.getStringAttribute(key,"0.5"));}return canonical;
}
void DrumAudioProcessor::migrateLegacyKits()
{
    std::unique_ptr<juce::XmlElement> pending;{const juce::ScopedLock lock(migrationLock);if(legacyKits && !migrationAttempted){pending=std::make_unique<juce::XmlElement>(*legacyKits);migrationAttempted=true;}}if(!pending)return;
    auto result=sharedStore.importLegacy(*pending);const juce::ScopedLock lock(migrationLock);migrationStatus=result.ok?"Previous kits are available in the shared library.":result.message;if(result.ok && legacyKits && legacyKits->toString()==pending->toString())legacyKits.reset();
}
drum::Result DrumAudioProcessor::openSharedLibrary()
{
    if(!libraryPreview)return drum::Result::fail("This is already the kit library.");{const juce::ScopedLock lock(migrationLock);migrationAttempted=false;}migrateLegacyKits();if(libraryLoaded && sharedLibraryDirty()){previewActive.store(true);return drum::Result::success();}auto result=sharedStore.load(libraryPreview->library);if(result.ok){libraryLoaded=true;previewActive.store(true);}return result;
}
