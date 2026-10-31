#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KitLibrary.h"
#include "SampleEngine.h"
#include "EffectRack.h"
#include "GlobalKitStore.h"
#include "PortableFiles.h"
#include "PatternPlayer.h"
#include "LiveKitLink.h"

class DrumAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    explicit DrumAudioProcessor(bool trackInstance=false);
    bool isMidiTrack() const{return trackInstance;}
    bool hasOpenKit() const{return !trackInstance || kitOpened.load();}
    void newKit();
    drum::Result saveDocument(const juce::File&);
    drum::Result openDocument(const juce::File&);
    juce::File documentFile() const{return currentDocument;}

    DrumAudioProcessor* libraryEditorProcessor() const{return libraryPreview.get();}
    drum::Result openSharedLibrary();
    bool sharedLibraryDirty() const{return libraryPreview && sharedStore.hasChanges(libraryPreview->getLibrary());}
    void closeSharedLibrary(){previewActive.store(false);if(libraryPreview){libraryPreview->getSampleEngine().pauseAudition();libraryPreview->effects.hideEditors();}}
    std::function<drum::Result(const juce::String&,const juce::String&)> useLibrarySound;
    std::function<drum::Result()> saveSharedLibrary,reloadSharedLibrary;
    juce::String libraryStatus() const {const juce::ScopedLock lock(migrationLock);return migrationStatus;}

    bool autoRemapLiveClips() const{return liveKitLink && liveKitLink->autoRemap.load();}
    void setAutoRemapLiveClips(bool value){if(liveKitLink){liveKitLink->autoRemap=value;liveKitLink->publish();triggerAsyncUpdate();}}
    void publishLiveKit(){if(liveKitLink)liveKitLink->publish();}
    ~DrumAudioProcessor() override;
    drum::KitLibrary& getLibrary() { return library; }
    drum::PatternPlayer& getPatterns(){return *patternPlayer;}
    drum::SampleEngine& getSampleEngine() { return sampleEngine; }
    drum::EffectRack& getEffects() { return effects; }
    juce::AudioParameterFloat& macro(int n) { return *macros[(size_t)n]; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DJL Drum"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return sampleEngine.tailSeconds()+effects.tailSeconds(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
private:
    std::unique_ptr<drum::LiveKitLink> liveKitLink;
    const bool trackInstance;
    std::atomic<bool> kitOpened{false};
    juce::File currentDocument;
    drum::KitLibrary library;
    std::unique_ptr<drum::PatternPlayer> patternPlayer;
    juce::MidiBuffer patternMidi;
    drum::GlobalKitStore sharedStore;
    std::unique_ptr<DrumAudioProcessor> libraryPreview;
    std::atomic<bool> previewActive{false};
    juce::AudioBuffer<float> previewBuffer;
    juce::MidiBuffer previewMidi;
    mutable juce::CriticalSection migrationLock;
    std::unique_ptr<juce::XmlElement> legacyKits;
    juce::String migrationStatus;
    bool migrationAttempted=false;
    std::atomic<bool> libraryLoaded{false};
    void migrateLegacyKits();
    static std::unique_ptr<juce::XmlElement> trackFromLibrary(const juce::XmlElement&);

    std::array<juce::AudioParameterFloat*,drum::numMacros> macros {};
    drum::EffectRack effects { library, [this] { triggerAsyncUpdate(); }, [this](int samples) { setLatencySamples(samples); } };
    drum::SampleEngine sampleEngine { library, [this] { triggerAsyncUpdate(); } };
    void handleAsyncUpdate() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAudioProcessor)
};
