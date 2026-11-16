#pragma once
#include "KitLibrary.h"

namespace djl::mac {
// Same-file kit links share structure; each host track owns its sound settings.
inline void preserveTrackSends(juce::XmlElement& incoming,const juce::XmlElement& local)
{
    auto* target=incoming.getChildByName("Kit");auto* source=local.getChildByName("Kit");
    if(!target || !source || target->getStringAttribute("id")!=source->getStringAttribute("id"))return;
    target->deleteAllChildElementsWithTagName("Return");
    for(auto* node:source->getChildIterator())if(node->hasTagName("Return"))target->addChildElement(new juce::XmlElement(*node));
    for(auto* group:target->getChildIterator())if(group->hasTagName("Group")){
        for(auto* oldGroup:source->getChildIterator())if(oldGroup->hasTagName("Group") && oldGroup->getStringAttribute("id")==group->getStringAttribute("id"))
        {
            for(int n=0;n<6;++n){auto key="mixSend"+juce::String(n);group->setAttribute(key,oldGroup->getStringAttribute(key));}
            for(auto key:{"mixVolume","mixPan","mixMute","mixSolo","mixOutput"})group->setAttribute(key,oldGroup->getStringAttribute(key));
            group->deleteAllChildElementsWithTagName("GroupChain");group->deleteAllChildElementsWithTagName("GroupReturn");
            for(auto* node:oldGroup->getChildIterator())if(node->hasTagName("GroupChain") || node->hasTagName("GroupReturn"))group->addChildElement(new juce::XmlElement(*node));
        }
        for(auto* instrument:group->getChildIterator())if(instrument->hasTagName("Instrument")){
            const juce::XmlElement* previous=nullptr;
            for(auto* oldGroup:source->getChildIterator())if(oldGroup->hasTagName("Group"))
                for(auto* old:oldGroup->getChildIterator())if(old->hasTagName("Instrument") && old->getStringAttribute("id")==instrument->getStringAttribute("id"))previous=old;
            if(!previous)continue;
            // Keep linked kit identities, names and keyboard order. Every sound
            // tab, sample choice, chain and send remains owned by this track.
            auto localInstrument=std::make_unique<juce::XmlElement>(*previous);
            for(auto key:{"id","name","note"})localInstrument->setAttribute(key,instrument->getStringAttribute(key));
            instrument->removeAllAttributes();
            for(int n=0;n<localInstrument->getNumAttributes();++n)instrument->setAttribute(localInstrument->getAttributeName(n),localInstrument->getAttributeValue(n));
            instrument->deleteAllChildElements();
            for(auto* node:localInstrument->getChildIterator())instrument->addChildElement(new juce::XmlElement(*node));
        }
    }
    auto disconnectMissingGroup=[&](juce::XmlElement& route){auto id=route.getStringAttribute("group");if(id.isEmpty())return;bool exists=false;for(auto* group:target->getChildIterator())if(group->hasTagName("Group") && group->getStringAttribute("id")==id)exists=true;if(!exists){route.setAttribute("group","");route.setAttribute("main",true);}};
    for(auto* child:target->getChildIterator()){if(child->hasTagName("Return"))disconnectMissingGroup(*child);if(child->hasTagName("Group"))for(auto* route:child->getChildIterator())if(route->hasTagName("GroupReturn"))disconnectMissingGroup(*route);}
    for(int n=0;n<drum::numMacros;++n){
        auto key="macro"+juce::String(n+1);incoming.setAttribute(key,local.getStringAttribute(key));
        auto name="macroName"+juce::String(n+1);target->setAttribute(name,source->getStringAttribute(name));
    }
}
}
