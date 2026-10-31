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
        auto state=library.snapshot();library.rename(state.activeKit,"No kit open");library.rename(state.kits[0].groups[0].id,"Sounds");
        libraryPreview=std::make_unique<DrumAudioProcessor>(false);
        libraryPreview->libraryOwner=this;
        libraryPreview->useLibrarySound=[this](const juce::String& kitId,const juce::String& instrumentId){
            if(!kitOpened.load())return drum::Result::fail("No kit is open. Choose New kit or Open kit in Drum, then Add again.");libraryPreview->effects.capture();auto source=libraryPreview->library.snapshot();const drum::Kit* kit=nullptr;for(const auto& candidate:source.kits)if(candidate.id==kitId)kit=&candidate;
            auto* sound=kit?drum::findInstrument(*kit,instrumentId):nullptr;if(!sound)return drum::Result::fail("Select a library sound first.");auto target=library.snapshot();const drum::Group* sourceGroup=nullptr;for(const auto& group:kit->groups)for(const auto& candidate:group.instruments)if(candidate.id==instrumentId)sourceGroup=&group;auto result=library.importGroupedInstrument(target.activeKit,*sourceGroup,*sound);if(result.ok)triggerAsyncUpdate();return result;
        };
        libraryPreview->saveSharedLibrary=[this]{libraryPreview->effects.capture();return sharedStore.save(libraryPreview->library);};
        libraryPreview->reloadSharedLibrary=[this]{return sharedStore.load(libraryPreview->library);};
    }
    patternPlayer=std::make_unique<drum::PatternPlayer>(library);
    sampleEngine.setEffects(&effects);
    library.beforeClone=[this]{effects.capture();};
    library.afterRestore=[this]{effects.stateRestored();};
    for(int n=0;n<drum::numMacros;++n)
    {
        auto* parameter=new juce::AudioParameterFloat(juce::ParameterID("macro"+juce::String(n+1),1),"Macro "+juce::String(n+1),0.0f,1.0f,0.5f);
        addParameter(parameter); macros[(size_t)n]=parameter;
    }
    if(trackInstance){
        liveKitLink=std::make_unique<drum::LiveKitLink>(library);
        // The instance token lives in the name, not the restored value.
        // Live only exposes automatable parameters to its Control Surface API.
        // A duplicate instance therefore cannot publish over its source's mapping.
        liveLinkParameter=new juce::AudioParameterFloat(juce::ParameterID("liveLink",1),liveKitLink->parameterName(),juce::NormalisableRange<float>(0,1),0,juce::AudioParameterFloatAttributes().withAutomatable(true));
        addParameter(liveLinkParameter);
    }

}

void DrumAudioProcessor::useLiveKit()
{
    if(!liveKitLink)return;
    // Existing Live Sets retain their configured parameter list across upgrades.
    // A real gesture lets Configure mode discover the new identity parameter.
    // Its value has no audio or identity meaning; never touch a musical macro.
    if(liveLinkParameter){
        liveLinkParameter->beginChangeGesture();
        liveLinkParameter->setValueNotifyingHost(liveLinkParameter->get() < .5f ? 1.0f : 0.0f);
        liveLinkParameter->endChangeGesture();
    }
    liveKitLink->requestSetup();
}

DrumAudioProcessor::~DrumAudioProcessor() { liveKitLink.reset(); patternPlayer->stop();previewActive.store(false);if(libraryPreview)libraryPreview->getSampleEngine().shutdown();sampleEngine.shutdown();cancelPendingUpdate(); }
void DrumAudioProcessor::handleAsyncUpdate()
{
    if(trackInstance)migrateLegacyKits();
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void DrumAudioProcessor::prepareToPlay(double rate, int blockSize)
{
    if(juce::MessageManager::getInstance()->isThisTheMessageThread())effects.sync();
    patternPlayer->prepare(rate);patternMidi.ensureSize(2*1024*1024);
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
    patternMidi.clear();patternMidi.addEvents(midi,0,-1,0);patternPlayer->process(patternMidi,buffer.getNumSamples(),sampleEngine.auditionTempo());
    sampleEngine.process(buffer, patternMidi);
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
    if(trackInstance){juce::XmlElement session("DrumSession");session.setAttribute("version",1);session.setAttribute("autoRemapLiveClips",autoRemapLiveClips());session.setAttribute("kitOpen",kitOpened.load());session.setAttribute("document",currentDocument.getFullPathName());session.addChildElement(new juce::XmlElement(*state));const juce::ScopedLock lock(migrationLock);if(legacyKits)session.createNewChildElement("LegacyKits")->addChildElement(new juce::XmlElement(*legacyKits));if(libraryPreview && sharedLibraryDirty()){auto* draft=session.createNewChildElement("LibraryDraft");draft->addChildElement(libraryPreview->library.toXml().release());if(auto base=sharedStore.baselineCopy())draft->createNewChildElement("Baseline")->addChildElement(base.release());}if(libraryLoaded && libraryPreview)session.addChildElement(libraryPreview->libraryWorkspaceState().release());copyXmlToBinary(session,data);}else copyXmlToBinary(*state, data);
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
          if(trackInstance){if(liveKitLink)liveKitLink->autoRemap=saved->getBoolAttribute("autoRemapLiveClips",false);kitOpened=saved->getBoolAttribute("kitOpen",true);auto path=saved->getStringAttribute("document");currentDocument=juce::File::isAbsolutePath(path)?juce::File(path):juce::File();const juce::ScopedLock lock(migrationLock);legacyKits=std::move(pending);migrationAttempted=false;triggerAsyncUpdate();}

          for(int n=0;n<drum::numMacros;++n)
          {
            // Restoring state is not a user gesture. The wrapper synchronises
            // its controller afterwards; do not mark the host project edited.
            juce::AudioProcessorParameter* parameter=macros[(size_t)n];
            parameter->setValue(values[(size_t)n]);
          }
          if(trackInstance)if(auto* draft=saved->getChildByName("LibraryDraft"))if(auto* data=draft->getChildByName("DrumState"))if(auto* base=draft->getChildByName("Baseline"))if(auto* previous=base->getChildByName("DrumState")){if(sharedStore.restoreBaseline(*previous) && libraryPreview->library.restore(*data))libraryLoaded=true;}
          if(trackInstance)if(auto* workspace=saved->getChildByName("LibraryWorkspace"))if(libraryPreview->restoreLibraryWorkspace(*workspace))libraryLoaded=true;
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
    canonical->deleteAllChildElements();canonical->addChildElement(selected.release());for(int n=0;n<drum::numMacros;++n){auto key="macro"+juce::String(n+1);canonical->setAttribute(key,xml.getStringAttribute(key,"0.5"));}return canonical;
}
void DrumAudioProcessor::migrateLegacyKits()
{
    std::unique_ptr<juce::XmlElement> pending;{const juce::ScopedLock lock(migrationLock);if(legacyKits && !migrationAttempted){pending=std::make_unique<juce::XmlElement>(*legacyKits);migrationAttempted=true;}}if(!pending)return;
    auto result=sharedStore.importLegacy(*pending);const juce::ScopedLock lock(migrationLock);migrationStatus=result.ok?"Previous kits are available in the shared library.":result.message;if(result.ok && legacyKits && legacyKits->toString()==pending->toString())legacyKits.reset();
}
drum::Result DrumAudioProcessor::openSharedLibrary()
{
    if(!libraryPreview)return drum::Result::fail("This is already the kit library.");{const juce::ScopedLock lock(migrationLock);migrationAttempted=false;}migrateLegacyKits();if(libraryLoaded){previewActive.store(true);return drum::Result::success();}
    auto remembered=libraryWorkspaceFile();
    if(remembered.existsAsFile()){
        auto xml=remembered.getSize()<=256ll*1024*1024?juce::parseXML(remembered):nullptr;
        if(!xml || !xml->hasTagName("LibraryWorkspace") || !libraryPreview->restoreLibraryWorkspace(*xml))return drum::Result::fail("The remembered Library workspace cannot be read. It has been kept unchanged.");
        libraryLoaded=true;previewActive.store(true);return drum::Result::success();
    }
    auto result=sharedStore.load(libraryPreview->library);if(result.ok){libraryPreview->initialiseLibraryDocuments();libraryLoaded=true;previewActive.store(true);}return result;
}

void DrumAudioProcessor::newKit()
{
    drum::KitLibrary fresh;library.restore(*fresh.toXml());for(auto* parameter:macros)parameter->setValueNotifyingHost(.5f);kitOpened=true;currentDocument=juce::File();triggerAsyncUpdate();
}
drum::Result DrumAudioProcessor::saveDocument(const juce::File& file)
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(!hasOpenKit())return drum::Result::fail("Create or open a kit before saving.");
    effects.capture();auto state=library.toXml();auto active=state->getStringAttribute("activeKit");
    for(auto* child=state->getFirstChildElement();child;){auto* next=child->getNextElement();if(child->getStringAttribute("id")!=active)state->removeChildElement(child,true);child=next;}
    for(int n=0;n<drum::numMacros;++n)state->setAttribute("macro"+juce::String(n+1),(double)macros[(size_t)n]->get());
    juce::XmlElement document("DrumDocument");document.setAttribute("version",1);document.setAttribute("kind",trackInstance?"kit":"library");document.addChildElement(state.release());
    if(!trackInstance){
        for(const auto& entry:libraryDocuments)if(entry.first!=active && entry.second.file==file)return drum::Result::fail("That file belongs to another open library. Choose a different filename.");
        auto name=file.getFileNameWithoutExtension().substring(0,64);
        document.getChildByName("DrumState")->getFirstChildElement()->setAttribute("name",name);
    }
    auto result=drum::PortableFiles::write(document,file);
    if(result.ok){if(trackInstance)currentDocument=file;else {library.rename(active,file.getFileNameWithoutExtension().substring(0,64));auto& info=libraryDocuments[active];info.file=file;info.baseline=libraryElement(active);libraryDirtyRevision=~uint64_t(0);}triggerAsyncUpdate();}return result;
}
drum::Result DrumAudioProcessor::openDocument(const juce::File& file)
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(!file.existsAsFile() || file.getSize()>256ll*1024*1024)return drum::Result::fail("File is missing or too large.");
    auto document=juce::parseXML(file);if(!document || !document->hasTagName("DrumDocument") || document->getIntAttribute("version")!=1 || document->getStringAttribute("kind")!=(trackInstance?"kit":"library"))return drum::Result::fail("This is not a supported Drum "+juce::String(trackInstance?"kit":"library")+" file.");
    auto* state=document->getChildByName("DrumState");if(!state)return drum::Result::fail("Document has no instrument data.");auto paths=drum::PortableFiles::resolve(*state,file);if(!paths.ok)return paths;
    drum::KitLibrary check;if(!check.restore(*state) || check.snapshot().kits.size()!=1)return drum::Result::fail("Invalid collection data.");
    if(trackInstance){
        std::array<float,drum::numMacros> values;for(int n=0;n<drum::numMacros;++n){auto value=state->getDoubleAttribute("macro"+juce::String(n+1),.5);if(!std::isfinite(value) || value<0 || value>1)return drum::Result::fail("Invalid macro value.");values[(size_t)n]=(float)value;}
        library.restore(*state);for(int n=0;n<drum::numMacros;++n)macros[(size_t)n]->setValueNotifyingHost(values[(size_t)n]);kitOpened=true;currentDocument=file;
    }else{
        auto combined=library.toXml();auto id=state->getStringAttribute("activeKit");
        for(const auto& entry:libraryDocuments)if(entry.second.file==file){library.activateKit(entry.first);return drum::Result::success(entry.first);}
        if(libraryElement(id) && libraryDocumentDirty(id))return drum::Result::fail("This library has unsaved changes. Save or close its tab before loading another version.");
        if(closedLibraries)combined->deleteAllChildElements();
        for(auto* child=combined->getFirstChildElement();child;){auto* next=child->getNextElement();if(child->getStringAttribute("id")==id)combined->removeChildElement(child,true);child=next;}
        if(combined->getNumChildElements()>=drum::maxKits)return drum::Result::fail("Eight libraries are already open. Close one before opening another.");
        state->getFirstChildElement()->setAttribute("name",file.getFileNameWithoutExtension().substring(0,64));
        combined->addChildElement(new juce::XmlElement(*state->getFirstChildElement()));combined->setAttribute("activeKit",id);if(!library.restore(*combined))return drum::Result::fail("Library could not be opened.");
        closedLibraries=false;auto& info=libraryDocuments[id];info.file=file;info.baseline=libraryElement(id);
    }
    triggerAsyncUpdate();return drum::Result::success();
}

std::unique_ptr<juce::XmlElement> DrumAudioProcessor::libraryElement(const juce::String& id) const
{
    auto xml=library.toXml();for(auto* item:xml->getChildIterator())if(item->getStringAttribute("id")==id)return std::make_unique<juce::XmlElement>(*item);return {};
}
void DrumAudioProcessor::initialiseLibraryDocuments()
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    libraryDocuments.clear();closedLibraries=false;libraryDirtyRevision=~uint64_t(0);
    for(const auto& kit:library.snapshot().kits)libraryDocuments[kit.id].baseline=libraryElement(kit.id);
    closedLibraries=isDefaultLibraryPlaceholder();
}
bool DrumAudioProcessor::isDefaultLibraryPlaceholder() const
{
    auto state=library.snapshot();if(state.kits.size()!=1)return false;const auto& kit=state.kits[0];
    return kit.name=="Kit 1" && kit.groups.size()==1 && kit.groups[0].name=="Group 1" && drum::instrumentCount(kit)==0 && kit.patterns.empty() && libraryDocumentFile(kit.id)==juce::File() && !libraryDocumentDirty(kit.id);
}
bool DrumAudioProcessor::libraryDocumentDirty(const juce::String& id) const
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(closedLibraries)return false;auto revision=library.currentRevision();
    if(revision!=libraryDirtyRevision){
        libraryDirtyCache.clear();auto xml=library.toXml();
        for(auto* item:xml->getChildIterator()){auto key=item->getStringAttribute("id");auto found=libraryDocuments.find(key);libraryDirtyCache[key]=found==libraryDocuments.end() || !found->second.baseline || !item->isEquivalentTo(found->second.baseline.get(),false);}
        libraryDirtyRevision=revision;
    }
    auto found=libraryDirtyCache.find(id);return found!=libraryDirtyCache.end() && found->second;
}
bool DrumAudioProcessor::libraryDocumentsDirty() const
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(closedLibraries)return false;for(const auto& kit:library.snapshot().kits)if(libraryDocumentDirty(kit.id))return true;return false;
}
juce::File DrumAudioProcessor::libraryDocumentFile(const juce::String& id) const
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    auto found=libraryDocuments.find(id);return found==libraryDocuments.end()?juce::File():found->second.file;
}
juce::String DrumAudioProcessor::libraryDocumentName(const juce::String& id) const
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    auto file=libraryDocumentFile(id);if(file!=juce::File())return file.getFileNameWithoutExtension();for(const auto& kit:library.snapshot().kits)if(kit.id==id)return kit.name;return {};
}
drum::Result DrumAudioProcessor::newLibraryDocument()
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(trackInstance)return drum::Result::fail("Open the Library screen first.");
    if(closedLibraries){closedLibraries=false;auto id=library.snapshot().activeKit;libraryDocuments.erase(id);library.rename(id,"Untitled library");return drum::Result::success(id);}
    auto result=library.addKit();if(result.ok)library.rename(result.id,"Untitled library");return result;
}
drum::Result DrumAudioProcessor::duplicateLibraryDocument(const juce::String& id)
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    if(closedLibraries)return drum::Result::fail("Open a library first.");return library.duplicateKit(id);
}
drum::Result DrumAudioProcessor::closeLibraryDocument(const juce::String& id)
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    auto state=library.snapshot();if(!libraryElement(id))return drum::Result::fail("Library is no longer open.");
    if(state.kits.size()==1){drum::KitLibrary fresh;library.restore(*fresh.toXml());closedLibraries=true;}
    else {auto result=library.removeKit(id);if(!result.ok)return result;}
    libraryDocuments.erase(id);return drum::Result::success();
}
void DrumAudioProcessor::discardLibraryDocuments()
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    effects.hideEditors();auto state=library.snapshot();
    for(const auto& kit:state.kits){
        if(!libraryDocumentDirty(kit.id))continue;
        auto found=libraryDocuments.find(kit.id);
        if(found==libraryDocuments.end() || !found->second.baseline){closeLibraryDocument(kit.id);continue;}
        auto xml=library.toXml();for(auto* item=xml->getFirstChildElement();item;item=item->getNextElement())if(item->getStringAttribute("id")==kit.id){xml->replaceChildElement(item,new juce::XmlElement(*found->second.baseline));break;}
        library.restore(*xml);
    }
}

std::unique_ptr<juce::XmlElement> DrumAudioProcessor::libraryWorkspaceState() const
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    auto state=std::make_unique<juce::XmlElement>("LibraryWorkspace");state->setAttribute("closed",closedLibraries);state->setAttribute("selectedGroup",librarySelectedGroup);state->setAttribute("selectedInstrument",librarySelectedInstrument);state->addChildElement(library.toXml().release());
    for(const auto& entry:libraryDocuments){auto* doc=state->createNewChildElement("Document");doc->setAttribute("id",entry.first);doc->setAttribute("file",entry.second.file.getFullPathName());if(entry.second.baseline)doc->addChildElement(new juce::XmlElement(*entry.second.baseline));}
    return state;
}
bool DrumAudioProcessor::restoreLibraryWorkspace(const juce::XmlElement& saved)
{
    const juce::ScopedLock documentGuard(libraryDocumentLock);
    auto* content=saved.getChildByName("DrumState");drum::KitLibrary check;if(!content || !check.restore(*content))return false;
    std::map<juce::String,LibraryDocument> restored;
    for(auto* doc:saved.getChildIterator())if(doc->hasTagName("Document")){
        auto id=doc->getStringAttribute("id"),path=doc->getStringAttribute("file");if(id.isEmpty() || restored.count(id) || (path.isNotEmpty() && !juce::File::isAbsolutePath(path)))return false;
        auto& info=restored[id];if(path.isNotEmpty())info.file=juce::File(path);
        if(auto* base=doc->getFirstChildElement()){auto test=std::make_unique<juce::XmlElement>(*content);test->deleteAllChildElements();test->addChildElement(new juce::XmlElement(*base));test->setAttribute("activeKit",id);if(base->getStringAttribute("id")!=id || !check.restore(*test))return false;info.baseline=std::make_unique<juce::XmlElement>(*base);}
    }
    if(!library.restore(*content))return false;libraryDocuments=std::move(restored);closedLibraries=saved.getBoolAttribute("closed");libraryDirtyRevision=~uint64_t(0);if(!closedLibraries)closedLibraries=isDefaultLibraryPlaceholder();librarySelectedGroup=saved.getStringAttribute("selectedGroup");librarySelectedInstrument=saved.getStringAttribute("selectedInstrument");return true;
}
juce::File DrumAudioProcessor::libraryWorkspaceFile()
{
    auto test=juce::SystemStats::getEnvironmentVariable("DJL_DRUM_TEST_LIBRARY",{});return test.isNotEmpty()?juce::File(test).getSiblingFile("library-workspace.xml"):juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/library-workspace.xml");
}
drum::Result DrumAudioProcessor::rememberLibraryWorkspace()
{
    auto file=libraryWorkspaceFile();if(!file.getParentDirectory().createDirectory().wasOk() || !file.replaceWithText(libraryWorkspaceState()->toString()))return drum::Result::fail("Could not remember the Library workspace.");return drum::Result::success();
}
