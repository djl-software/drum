#include "GlobalKitStore.h"
#include "PortableFiles.h"
#include <map>
namespace drum {
namespace {
using Kits=std::map<juce::String,juce::String>;
Kits kits(const juce::XmlElement* xml){Kits result;if(xml)for(auto* kit:xml->getChildIterator())if(kit->hasTagName("Kit"))result[kit->getStringAttribute("id")]=kit->toString();return result;}
void selectValidKit(juce::XmlElement& xml){auto id=xml.getStringAttribute("activeKit");for(auto* k:xml.getChildIterator())if(k->getStringAttribute("id")==id)return;if(auto* first=xml.getFirstChildElement())xml.setAttribute("activeKit",first->getStringAttribute("id"));}
bool emptyLibrary(const juce::XmlElement& xml){KitLibrary model;if(!model.restore(xml))return false;auto state=model.snapshot();return state.kits.size()==1 && state.kits[0].name=="Kit 1" && state.kits[0].groups.size()==1 && state.kits[0].groups[0].name=="Group 1" && instrumentCount(state.kits[0])==0;}
}
juce::File GlobalKitStore::defaultFile(){auto test=juce::SystemStats::getEnvironmentVariable("DJL_DRUM_TEST_LIBRARY",{});return test.isNotEmpty()?juce::File(test):PortableFiles::root().getChildFile("Libraries/collections.xml");}
std::unique_ptr<juce::XmlElement> GlobalKitStore::read() const {if(!target.existsAsFile() || target.getSize()>256ll*1024*1024)return {};auto xml=juce::parseXML(target);if(!xml || !PortableFiles::resolve(*xml,target).ok)return {};KitLibrary model;if(!model.restore(*xml))return {};return model.toXml();}
Result GlobalKitStore::write(const juce::XmlElement& xml){return PortableFiles::write(xml,target);}
Result GlobalKitStore::load(KitLibrary& destination){const juce::ScopedLock guard(stateLock);juce::InterProcessLock lock("DJLDrumLibrary"+juce::String(target.getFullPathName().hashCode64()));if(!lock.enter(2000))return Result::fail("Library is busy in another Drum instance. Try again.");struct Unlock{juce::InterProcessLock& lock;~Unlock(){lock.exit();}}unlock{lock};
    if(!target.exists() && target==defaultFile() && juce::SystemStats::getEnvironmentVariable("DJL_DRUM_TEST_LIBRARY",{}).isEmpty()){
        auto old=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/Library/kits.xml");
        if(old.existsAsFile()){
            auto previous=old.getSize()<=256ll*1024*1024?juce::parseXML(old):nullptr;KitLibrary check;
            if(!previous || !check.restore(*previous))return Result::fail("Previous shared library cannot be read. It has not been changed.");
            auto migrated=write(*previous);if(!migrated.ok)return migrated;
        }
    }
    auto xml=read();if(!xml){if(target.exists())return Result::fail("Shared library cannot be read; it has not been overwritten.");KitLibrary initial;xml=initial.toXml();auto result=write(*xml);if(!result.ok)return result;}if(!destination.restore(*xml))return Result::fail("Invalid shared library.");baseline=std::make_unique<juce::XmlElement>(*xml);return Result::success();}
bool GlobalKitStore::hasChanges(const KitLibrary& source) const {const juce::ScopedLock guard(stateLock);return baseline && kits(baseline.get())!=kits(source.toXml().get());}
Result GlobalKitStore::save(KitLibrary& source){const juce::ScopedLock guard(stateLock);if(!baseline)return Result::fail("Open the shared library before saving.");juce::InterProcessLock lock("DJLDrumLibrary"+juce::String(target.getFullPathName().hashCode64()));if(!lock.enter(2000))return Result::fail("Library is busy in another Drum instance. Try again.");struct Unlock{juce::InterProcessLock& lock;~Unlock(){lock.exit();}}unlock{lock};auto disk=read();if(!disk)return Result::fail("Shared library is missing or unreadable; current edits have been kept.");auto edited=source.toXml();auto collected=PortableFiles::collect(*edited,target);if(!collected.ok)return collected;PortableFiles::resolve(*edited,target);auto base=kits(baseline.get()),current=kits(disk.get()),next=kits(edited.get());auto merged=current;
    for(const auto& [id,value]:base)if(!next.contains(id)){if(current.contains(id) && current[id]!=value)return Result::fail("This kit changed in another instance. Save a copy or reload before removing it.");merged.erase(id);}
    for(const auto& [id,value]:next){if(base.contains(id) && base[id]==value)continue;if(base.contains(id) && !current.contains(id))return Result::fail("This kit was removed in another instance. Reload before saving changes.");if(current.contains(id) && (!base.contains(id) || current[id]!=base[id]) && current[id]!=value)return Result::fail("This kit changed in another instance. Reload the library before editing it again.");merged[id]=value;}
    if(merged.empty() || merged.size()>maxKits)return Result::fail("Shared library must contain 1 to 8 kits. No edits were overwritten.");
    auto combined=std::make_unique<juce::XmlElement>(*edited);combined->deleteAllChildElements();
    // Preserve local order, then append kits newly saved by other instances.
    for(auto* k:edited->getChildIterator())if(auto it=merged.find(k->getStringAttribute("id"));it!=merged.end()){combined->addChildElement(juce::parseXML(it->second).release());merged.erase(it);}
    for(auto* k:disk->getChildIterator())if(auto it=merged.find(k->getStringAttribute("id"));it!=merged.end()){combined->addChildElement(juce::parseXML(it->second).release());merged.erase(it);}
    selectValidKit(*combined);KitLibrary check;if(!check.restore(*combined))return Result::fail("Merged library is invalid; nothing was saved.");auto result=write(*combined);if(result.ok){source.restore(*edited);baseline=std::move(edited);}return result;
}
Result GlobalKitStore::importLegacy(const juce::XmlElement& source){const juce::ScopedLock guard(stateLock);KitLibrary incoming;if(!incoming.restore(source))return Result::fail("Legacy kits could not be read.");juce::InterProcessLock lock("DJLDrumLibrary"+juce::String(target.getFullPathName().hashCode64()));if(!lock.enter(2000))return Result::fail("Shared library is busy; legacy kits remain saved in this project.");struct Unlock{juce::InterProcessLock& lock;~Unlock(){lock.exit();}}unlock{lock};auto disk=read();if(!disk){if(target.exists())return Result::fail("Shared library unreadable; legacy kits remain in this project.");disk=incoming.toXml();return write(*disk);}auto sourceXml=incoming.toXml();auto collected=PortableFiles::collect(*sourceXml,target);if(!collected.ok)return collected;PortableFiles::resolve(*sourceXml,target);auto existing=kits(disk.get());if(emptyLibrary(*disk) && !existing.contains(sourceXml->getFirstChildElement()->getStringAttribute("id"))){disk->deleteAllChildElements();existing.clear();}
    for(auto* kit:sourceXml->getChildIterator()){auto id=kit->getStringAttribute("id");if(existing.contains(id)){if(existing[id]!=kit->toString())return Result::fail("A legacy kit differs from its shared version; the original remains saved in this project.");continue;}if(disk->getNumChildElements()>=maxKits)return Result::fail("Shared library is full; legacy kits remain saved in this project.");disk->addChildElement(new juce::XmlElement(*kit));}
    selectValidKit(*disk);return write(*disk);
}
}
