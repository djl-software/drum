#pragma once
#include "KitLibrary.h"
namespace drum {
class GlobalKitStore {
public:
    explicit GlobalKitStore(juce::File file=defaultFile()):target(std::move(file)){}
    static juce::File defaultFile();
    Result load(KitLibrary& destination);
    Result save(const KitLibrary& source);
    Result importLegacy(const juce::XmlElement&);
    bool hasChanges(const KitLibrary&) const;
    std::unique_ptr<juce::XmlElement> baselineCopy() const{const juce::ScopedLock guard(stateLock);return baseline?std::make_unique<juce::XmlElement>(*baseline):nullptr;}
    bool restoreBaseline(const juce::XmlElement& xml){const juce::ScopedLock guard(stateLock);KitLibrary model;if(!model.restore(xml))return false;baseline=model.toXml();return true;}
    juce::File file() const{return target;}
private:
    Result write(const juce::XmlElement&);
    std::unique_ptr<juce::XmlElement> read() const;
    mutable juce::CriticalSection stateLock;
    juce::File target;
    std::unique_ptr<juce::XmlElement> baseline;
};
}
