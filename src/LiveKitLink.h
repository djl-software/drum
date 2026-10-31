#pragma once
#include "KitLibrary.h"
#include <juce_events/juce_events.h>
namespace drum {
// Message-thread publication only: never waits for Live or performs audio-thread I/O.
class LiveKitLink final : private juce::Timer {
public:
    static juce::File folder(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("DJL/Drum/LiveLink");}
    explicit LiveKitLink(KitLibrary& source):library(source),token(juce::Uuid().toString().removeCharacters("-").substring(0,16)) {startTimer(500);}
    ~LiveKitLink() override {stopTimer();file().deleteFile();}
    juce::String parameterName() const{return "DJL Link "+token;}
    juce::File file() const{return folder().getChildFile(token+".json");}
    std::atomic<bool> autoRemap{false};
    void publish(){
        auto state=library.snapshot();auto* kit=activeKit(state);if(!kit)return;
        auto* object=new juce::DynamicObject;juce::var payload(object);
        object->setProperty("protocol",1);object->setProperty("token",token);
        object->setProperty("timestamp",juce::Time::getCurrentTime().toMilliseconds()/1000.0);
        object->setProperty("kit",kit->name);object->setProperty("kitId",kit->id);object->setProperty("autoRemap",autoRemap.load());juce::Array<juce::var> notes;
        for(const auto& group:kit->groups)for(const auto& instrument:group.instruments){auto* note=new juce::DynamicObject;
            note->setProperty("id",instrument.id);note->setProperty("note",instrument.note);note->setProperty("name",instrument.name);notes.add(juce::var(note));}
        object->setProperty("notes",notes);
        if(folder().createDirectory().wasOk())file().replaceWithText(juce::JSON::toString(payload,true));
    }
private:
    void timerCallback() override{publish();}
    KitLibrary& library;juce::String token;
};
}
